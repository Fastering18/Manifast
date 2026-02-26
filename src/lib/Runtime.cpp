#include "manifast/Runtime.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <vector>
#include <string>
#include <cmath>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "manifast/PlotBackend.h"


extern "C" {

static size_t g_allocated_memory = 0;
static ManifastPlotShowCallback g_plot_show_callback = nullptr;
static ManifastClearOutputCallback g_clear_output_callback = nullptr; // New global callback

static std::unordered_map<std::string, char*> g_string_pool;

static char* manifast_intern_string(const char* s) {
    auto it = g_string_pool.find(s);
    if (it != g_string_pool.end()) return it->second;
    char* news = mf_strdup(s);
    g_string_pool[s] = news;
    return news;
}

MF_API void manifast_set_plot_show_callback(ManifastPlotShowCallback cb) {
    g_plot_show_callback = cb;
}

// Implementation for the new clear output callback setter
MF_API void manifast_set_clear_output_callback(ManifastClearOutputCallback cb) {
    g_clear_output_callback = cb;
}

MF_API void* mf_malloc(size_t size) {
    if (size > 256 * 1024 * 1024) { // Hard cap 256MB
        fprintf(stderr, "Error: Insane allocation size requested: %zu bytes\n", size);
        exit(1);
    }
    if (size > 10 * 1024 * 1024) {
        fprintf(stderr, "Warning: Large allocation: %zu bytes\n", size);
    }
    if (g_allocated_memory + size > MANIFAST_MEM_LIMIT) {
        fprintf(stderr, "Error: Manifast memory limit exceeded (%zu bytes requested, %zu allocated)\n", size, g_allocated_memory);
        exit(1);
    }
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Error: Out of memory (malloc failed for %zu bytes)\n", size);
        exit(1);
    }
    g_allocated_memory += size;
    return ptr;
}

MF_API char* mf_strdup(const char* s) {
    if (!s) return nullptr;
    // Safety check for string length to prevent junk pointer crawl
    size_t size = 0;
    while (s[size] != '\0' && size < 1024 * 1024) size++;
    if (size == 1024 * 1024) fprintf(stderr, "Warning: mf_strdup hit 1MB limit - likely junk pointer\n");
    size++; // include null terminator

    if (g_allocated_memory + size > MANIFAST_MEM_LIMIT) {
        fprintf(stderr, "Error: Manifast memory limit exceeded (strdup: %zu bytes)\n", size);
        exit(1);
    }
    char* ptr = (char*)malloc(size);
    if (!ptr) {
        fprintf(stderr, "Error: Out of memory (strdup failed for %zu bytes)\n", size);
        exit(1);
    }
    memcpy(ptr, s, size);
    ptr[size-1] = '\0'; // Ensure terminator
    g_allocated_memory += size;
    return ptr;
}

MF_API Any* manifast_create_number(double val) {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 0;
    a->number = val;
    a->ptr = nullptr;
    return a;
}

MF_API Any* manifast_create_string(const char* str) {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 1; // String
    a->number = 0;
    a->ptr = manifast_intern_string(str);
    return a;
}

MF_API Any* manifast_create_boolean(bool val) {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 2; // Boolean
    a->number = val ? 1.0 : 0.0;
    a->ptr = nullptr;
    return a;
}

MF_API Any* manifast_create_nil() {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 3; // Nil
    a->number = 0;
    a->ptr = nullptr;
    return a;
}

MF_API Any* manifast_create_array(uint32_t initial_size) {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 6; // Array
    a->number = 0;
    
    ManifastArray* arr = (ManifastArray*)mf_malloc(sizeof(ManifastArray));
    arr->size = initial_size;
    arr->capacity = initial_size > 0 ? initial_size : 4;
    arr->elements = (Any*)mf_malloc(sizeof(Any) * arr->capacity);
    
    // Initialize elements to 0
    for(uint32_t i = 0; i < arr->size; ++i) {
        arr->elements[i].type = 0;
        arr->elements[i].number = 0;
        arr->elements[i].ptr = nullptr;
    }
    
    a->ptr = arr;
    return a;
}

MF_API Any* manifast_create_object() {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 7; // Object
    a->number = 0;
    
    ManifastObject* obj = (ManifastObject*)mf_malloc(sizeof(ManifastObject));
    obj->size = 0;
    obj->capacity = 4;
    obj->entries = (ManifastObjectEntry*)mf_malloc(sizeof(ManifastObjectEntry) * obj->capacity);
    
    a->ptr = obj;
    return a;
}

MF_API Any* manifast_create_class(const char* name) {
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 8; // Class
    a->number = 0;
    
    ManifastClass* klass = (ManifastClass*)mf_malloc(sizeof(ManifastClass));
    klass->name = mf_strdup(name);
    
    Any* methodsObj = manifast_create_object();
    klass->methods = (ManifastObject*)methodsObj->ptr;
    
    a->ptr = klass;
    return a;
}

MF_API void manifast_class_add_method(Any* class_any, const char* name, ManifastNativeFn fn) {
    if (class_any->type != 8) return;
    ManifastClass* klass = (ManifastClass*)class_any->ptr;
    Any fn_any;
    fn_any.type = 4; // Native
    fn_any.number = 0;
    fn_any.ptr = (void*)fn;
    manifast_object_set_raw(klass->methods, name, &fn_any);
}

MF_API Any* manifast_create_instance(Any* class_any) {
    if (class_any->type != 8) return nullptr;
    
    Any* a = (Any*)mf_malloc(sizeof(Any));
    a->type = 9; // Instance
    a->number = 0;
    
    ManifastInstance* inst = (ManifastInstance*)mf_malloc(sizeof(ManifastInstance));
    inst->klass = (ManifastClass*)class_any->ptr;
    
    Any* fieldsObj = manifast_create_object();
    inst->fields = (ManifastObject*)fieldsObj->ptr;
    
    a->ptr = inst;
    return a;
}

MF_API void manifast_object_set_raw(ManifastObject* obj, const char* key, Any* val_any) {
    // Check if exists
    for (uint32_t i = 0; i < obj->size; ++i) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            obj->entries[i].value = *val_any;
            return;
        }
    }
    
    // Resize if needed
    if (obj->size == obj->capacity) {
        uint32_t old_cap = obj->capacity;
        obj->capacity *= 2;
        obj->entries = (ManifastObjectEntry*)realloc(obj->entries, sizeof(ManifastObjectEntry) * obj->capacity);
    }
    
    obj->entries[obj->size].key = manifast_intern_string(key);
    obj->entries[obj->size].value = *val_any;
    obj->size++;
}

MF_API void manifast_object_set(Any* obj_any, const char* key, Any* val_any) {
    if (obj_any->type == 7) { // Object
        manifast_object_set_raw((ManifastObject*)obj_any->ptr, key, val_any);
    } else if (obj_any->type == 9) { // Instance
        ManifastInstance* inst = (ManifastInstance*)obj_any->ptr;
        manifast_object_set_raw(inst->fields, key, val_any);
    }
}

MF_API Any* manifast_object_get_raw(ManifastObject* obj, const char* key) {
    if (obj->size == 0) {
        static Any nilVal = {3, 0.0, nullptr};
        return &nilVal;
    }
    
    // Intern the lookup key so we can use pointer equality
    const char* interned_key = manifast_intern_string(key);
    
    for (uint32_t i = 0; i < obj->size; ++i) {
        if (obj->entries[i].key == interned_key) {
            return &obj->entries[i].value;
        }
    }
    static Any nilVal = {3, 0.0, nullptr};
    return &nilVal;
}

MF_API Any* manifast_object_get(Any* obj_any, const char* key) {
    if (obj_any->type == 7) { // Object
        return manifast_object_get_raw((ManifastObject*)obj_any->ptr, key);
    } else if (obj_any->type == 9) { // Instance
        ManifastInstance* inst = (ManifastInstance*)obj_any->ptr;
        // 1. Look in fields
        Any* val = manifast_object_get_raw(inst->fields, key);
        if (val && val->type != 3) return val;
        // 2. Look in class methods
        return manifast_object_get_raw(inst->klass->methods, key);
    } else if (obj_any->type == 8) { // Class
        ManifastClass* klass = (ManifastClass*)obj_any->ptr;
        return manifast_object_get_raw(klass->methods, key);
    }
    static Any nilVal = {3, 0.0, nullptr};
    return &nilVal;
}

MF_API void manifast_array_set(Any* arr_any, double index_d, Any* val_any) {
    if (arr_any->type != 6) return;
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    uint32_t index = (uint32_t)index_d;
    
    if (index < 1) {
        fprintf(stderr, "Error: Array index must be >= 1 (got %u)\n", index);
        return;
    }
    index--; // Convert to 0-based internal
    
    if (index >= arr->size) {
        // Auto-expand if within reasonable bounds?
        // For now just error or resize
        if (index < 1000000) {
            uint32_t new_size = index + 1;
            if (new_size > arr->capacity) {
                uint32_t new_cap = arr->capacity * 2;
                while (new_cap < new_size) new_cap *= 2;
                arr->elements = (Any*)realloc(arr->elements, sizeof(Any) * new_cap);
                arr->capacity = new_cap;
            }
            // Init new elements to nil
            for (uint32_t i = arr->size; i < new_size; i++) {
                arr->elements[i] = (Any){3, 0.0, nullptr};
            }
            arr->size = new_size;
        } else {
             fprintf(stderr, "Error: Array index out of bounds: %u (size %u)\n", index + 1, arr->size);
             return;
        }
    }
    
    arr->elements[index] = *val_any;
}

MF_API Any* manifast_array_get(Any* arr_any, double index_d) {
    static Any nilVal = {3, 0.0, nullptr};
    if (arr_any->type != 6) return &nilVal;
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    uint32_t index = (uint32_t)index_d;

    if (index < 1 || (index - 1) >= arr->size) {
        return &nilVal;
    }
    
    return &arr->elements[index - 1];
}

MF_API double manifast_array_len(Any* arr_any) {
    if (arr_any->type != 6) return 0;
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    return (double)arr->size;
}

MF_API void manifast_array_push(Any* arr_any, Any* val_any) {
    if (arr_any->type != 6) return;
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    
    if (arr->size == arr->capacity) {
        uint32_t new_cap = arr->capacity * 2;
        if (new_cap == 0) new_cap = 4;
        arr->elements = (Any*)realloc(arr->elements, sizeof(Any) * new_cap);
        arr->capacity = new_cap;
    }
    
    arr->elements[arr->size] = *val_any;
    arr->size++;
}

MF_API Any* manifast_array_pop(Any* arr_any) {
    if (arr_any->type != 6) return manifast_create_nil();
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    if (arr->size == 0) return manifast_create_nil();
    
    Any val = arr->elements[arr->size - 1];
    arr->size--;
    
    Any* res = (Any*)mf_malloc(sizeof(Any));
    *res = val;
    return res;
}

// --- Native Math Functions ---
#define MATH_BEGIN() \
    int idx = 0; \
    if (nargs >= 1 && args[0].type != 0) idx++; \
    if (nargs - idx < 1) { args[-1] = {3, 0.0, nullptr}; return; }

static void m_sin(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, sin(args[idx].number), nullptr}; }
static void m_cos(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, cos(args[idx].number), nullptr}; }
static void m_tan(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, tan(args[idx].number), nullptr}; }
static void m_asin(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, asin(args[idx].number), nullptr}; }
static void m_acos(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, acos(args[idx].number), nullptr}; }
static void m_atan(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, atan(args[idx].number), nullptr}; }
static void m_atan2(void* vm, Any* args, int nargs) { 
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 2 && args[idx].type == 0 && args[idx+1].type == 0) args[-1] = {0, atan2(args[idx].number, args[idx+1].number), nullptr}; 
    else args[-1] = {3, 0.0, nullptr}; 
}
static void m_sqrt(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, sqrt(args[idx].number), nullptr}; }
static void m_abs(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, fabs(args[idx].number), nullptr}; }
static void m_floor(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, floor(args[idx].number), nullptr}; }
static void m_ceil(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, ceil(args[idx].number), nullptr}; }
static void m_pow(void* vm, Any* args, int nargs) { 
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 2 && args[idx].type == 0 && args[idx+1].type == 0) args[-1] = {0, pow(args[idx].number, args[idx+1].number), nullptr}; 
    else args[-1] = {3, 0.0, nullptr}; 
}
static void m_log(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, log(args[idx].number), nullptr}; }
static void m_exp(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, exp(args[idx].number), nullptr}; }

// --- Extended Math (MATLAB-common) ---
static void m_sinh(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, sinh(args[idx].number), nullptr}; }
static void m_cosh(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, cosh(args[idx].number), nullptr}; }
static void m_tanh(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, tanh(args[idx].number), nullptr}; }
static void m_asinh(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, asinh(args[idx].number), nullptr}; }
static void m_acosh(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, acosh(args[idx].number), nullptr}; }
static void m_atanh(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, atanh(args[idx].number), nullptr}; }
static void m_round(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, round(args[idx].number), nullptr}; }
static void m_trunc(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, trunc(args[idx].number), nullptr}; }
static void m_log2(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, log2(args[idx].number), nullptr}; }
static void m_log10(void* vm, Any* args, int nargs) { MATH_BEGIN(); args[-1] = {0, log10(args[idx].number), nullptr}; }
static void m_sign(void* vm, Any* args, int nargs) { MATH_BEGIN(); double v = args[idx].number; args[-1] = {0, (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0, nullptr}; }
static void m_hypot(void* vm, Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 2 && args[idx].type == 0 && args[idx+1].type == 0) args[-1] = {0, hypot(args[idx].number, args[idx+1].number), nullptr};
    else args[-1] = {3, 0.0, nullptr};
}
static void m_fmod(void* vm, Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 2 && args[idx].type == 0 && args[idx+1].type == 0) args[-1] = {0, fmod(args[idx].number, args[idx+1].number), nullptr};
    else args[-1] = {3, 0.0, nullptr};
}
static void m_max(void* vm, Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 2 && args[idx].type == 0 && args[idx+1].type == 0) args[-1] = {0, fmax(args[idx].number, args[idx+1].number), nullptr};
    else args[-1] = {3, 0.0, nullptr};
}
static void m_min(void* vm, Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 2 && args[idx].type == 0 && args[idx+1].type == 0) args[-1] = {0, fmin(args[idx].number, args[idx+1].number), nullptr};
    else args[-1] = {3, 0.0, nullptr};
}
static void m_clamp(void* vm, Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 3 && args[idx].type == 0 && args[idx+1].type == 0 && args[idx+2].type == 0) {
        double v = args[idx].number, lo = args[idx+1].number, hi = args[idx+2].number;
        args[-1] = {0, fmax(lo, fmin(v, hi)), nullptr};
    } else args[-1] = {3, 0.0, nullptr};
}
static void m_linspace(void* vm, Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    if (nargs - idx >= 3 && args[idx].type == 0 && args[idx+1].type == 0 && args[idx+2].type == 0) {
        double start = args[idx].number, stop = args[idx+1].number;
        int n = (int)args[idx+2].number;
        if (n < 1) n = 1;
        if (n > 100000) n = 100000;
        Any* arr = manifast_create_array((uint32_t)n);
        ManifastArray* a = (ManifastArray*)arr->ptr;
        double step = (n > 1) ? (stop - start) / (n - 1) : 0.0;
        for (int i = 0; i < n; i++) {
            a->elements[i] = {0, start + step * i, nullptr};
        }
        args[-1] = *arr;
    } else args[-1] = {3, 0.0, nullptr};
}

MF_API Any* manifast_impor(const char* name) {
    std::string sname = name;
    if (sname.length() > 3 && (sname.ends_with(".dll") || sname.ends_with(".so") || sname.ends_with(".dylib"))) {
        void* handle = nullptr;
#ifdef _WIN32
        handle = (void*)LoadLibraryA(name);
#else
        handle = dlopen(name, RTLD_NOW);
#endif
        if (!handle) return manifast_create_nil();

        typedef Any* (*InitFn)(void*);
        InitFn init = nullptr;
#ifdef _WIN32
        init = (InitFn)GetProcAddress((HMODULE)handle, "manifast_init");
#else
        init = (InitFn)dlsym(handle, "manifast_init");
#endif
        if (init) return init(nullptr);
        return manifast_create_nil();
    }

    if (strcmp(name, "math") == 0) {
        Any* obj = manifast_create_object();
        struct { const char* n; ManifastNativeFn f; } math_funcs[] = {
            {"sin", m_sin}, {"cos", m_cos}, {"tan", m_tan},
            {"asin", m_asin}, {"acos", m_acos}, {"atan", m_atan},
            {"atan2", m_atan2}, {"sqrt", m_sqrt}, {"abs", m_abs},
            {"floor", m_floor}, {"ceil", m_ceil}, {"pow", m_pow},
            {"log", m_log}, {"exp", m_exp},
            {"sinh", m_sinh}, {"cosh", m_cosh}, {"tanh", m_tanh},
            {"asinh", m_asinh}, {"acosh", m_acosh}, {"atanh", m_atanh},
            {"round", m_round}, {"trunc", m_trunc},
            {"log2", m_log2}, {"log10", m_log10},
            {"sign", m_sign}, {"hypot", m_hypot}, {"mod", m_fmod},
            {"max", m_max}, {"min", m_min}, {"clamp", m_clamp},
            {"linspace", m_linspace}
        };
        int count = sizeof(math_funcs) / sizeof(math_funcs[0]);
        for (int i = 0; i < count; i++) {
            Any val = {4, 0.0, (void*)math_funcs[i].f};
            manifast_object_set(obj, math_funcs[i].n, &val);
        }
        Any pi = {0, 3.141592653589793, nullptr};
        Any e = {0, 2.718281828459045, nullptr};
        Any tau = {0, 6.283185307179586, nullptr};
        Any inf_val = {0, INFINITY, nullptr};
        Any nan_val = {0, NAN, nullptr};
        manifast_object_set(obj, "pi", &pi);
        manifast_object_set(obj, "e", &e);
        manifast_object_set(obj, "tau", &tau);
        manifast_object_set(obj, "inf", &inf_val);
        manifast_object_set(obj, "nan", &nan_val);
        return obj;
    } else if (strcmp(name, "os") == 0) {
        Any* obj = manifast_create_object();
        auto waktuNano = [](void* vm, Any* args, int nargs) {
            auto now = std::chrono::steady_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
            args[-1] = {0, (double)ns, nullptr};
        };
        auto exitFn = [](void* vm, Any* args, int nargs) {
             int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
             int code = (nargs - idx >= 1 && args[idx].type == 0) ? (int)args[idx].number : 0;
             exit(code);
        };
        auto clearOutput = [](void* vm, Any* args, int nargs) {
            if (g_clear_output_callback) {
                g_clear_output_callback();
            } else {
                printf("\033[2J\033[H");
                fflush(stdout);
            }
            args[-1] = {3, 0.0, nullptr};
        };
        Any fn1 = {4, 0.0, (void*)+waktuNano};
        Any fn2 = {4, 0.0, (void*)+exitFn};
        Any fn3 = {4, 0.0, (void*)+clearOutput};
        manifast_object_set(obj, "waktuNano", &fn1);
        manifast_object_set(obj, "keluar", &fn2);
        manifast_object_set(obj, "clearOutput", &fn3);
        return obj;
    } else if (strcmp(name, "string") == 0) {
        Any* obj = manifast_create_object();
        auto split = [](void* vm, Any* args, int nargs) {
            args[-1] = {3, 0.0, nullptr}; 
            int argIdx = 0;
            if (nargs >= 1 && args[argIdx].type != 1) argIdx++;
            int remaining = nargs - argIdx;
            if (remaining < 2 || args[argIdx].type != 1 || args[argIdx+1].type != 1) {
                 args[-1] = *manifast_create_array(0);
                 return;
            }
            std::string str = (char*)args[argIdx].ptr;
            std::string delim = (char*)args[argIdx+1].ptr;
            Any* arr = manifast_create_array(0);
            if (delim.empty()) {
                Any val = {1, 0.0, (void*)mf_strdup(str.c_str())};
                manifast_array_push(arr, &val);
                args[-1] = *arr;
                return;
            }
            size_t start = 0, end;
            while ((end = str.find(delim, start)) != std::string::npos) {
                std::string p = str.substr(start, end - start);
                Any val = {1, 0.0, (void*)mf_strdup(p.c_str())};
                manifast_array_push(arr, &val);
                start = end + delim.length();
            }
            std::string last = str.substr(start);
            Any lVal = {1, 0.0, (void*)mf_strdup(last.c_str())};
            manifast_array_push(arr, &lVal);
            args[-1] = *arr;
        };
        auto substring = [](void* vm, Any* args, int nargs) {
            args[-1] = {3, 0.0, nullptr}; 
            int argIdx = 0;
            if (nargs >= 1 && args[argIdx].type != 1) argIdx++;
            int remaining = nargs - argIdx;
            if (remaining < 3 || args[argIdx].type != 1) return;
            if (!args[argIdx].ptr) return;
            std::string str = (char*)args[argIdx].ptr;
            int start = (int)args[argIdx+1].number;
            int len = (int)args[argIdx+2].number;
            if (start < 1) start = 1;
            if (start > (int)str.length() || len <= 0) { 
                args[-1] = {1, 0.0, (void*)mf_strdup("")}; 
                return; 
            }
            if (start + len - 1 > (int)str.length()) len = (int)str.length() - start + 1;
            std::string res = str.substr(start - 1, len);
            args[-1] = {1, 0.0, (void*)mf_strdup(res.c_str())};
        };
        Any fn1 = {4, 0.0, (void*)+split};
        Any fn2 = {4, 0.0, (void*)+substring};
        manifast_object_set(obj, "split", &fn1);
        manifast_object_set(obj, "substring", &fn2);
        return obj;
    }
    else if (strcmp(name, "plot") == 0) {
        Any* obj = manifast_create_object();

        // Shared plot state (static so it persists across calls)
        static manifast::plot::PlotBackend g_plot;
        static bool g_plot_initialized = false;

        auto plot_reset = [](void* vm, Any* args, int nargs) {
            g_plot = manifast::plot::PlotBackend();
            g_plot_initialized = false;
            args[-1] = {3, 0.0, nullptr};
        };

        static auto extract_config = [](Any* args, int argIdx, int nargs) {
            manifast::plot::ChartConfig cfg;
            int remaining = nargs - argIdx;
            if (remaining >= 3 && args[argIdx+2].type == 7) {
                ManifastObject* cobj = (ManifastObject*)args[argIdx+2].ptr;
                Any* t = manifast_object_get_raw(cobj, "title");
                if (t && t->type == 1 && t->ptr) cfg.title = (char*)t->ptr;
                Any* xl = manifast_object_get_raw(cobj, "xlabel");
                if (xl && xl->type == 1 && xl->ptr) cfg.xlabel = (char*)xl->ptr;
                Any* yl = manifast_object_get_raw(cobj, "ylabel");
                if (yl && yl->type == 1 && yl->ptr) cfg.ylabel = (char*)yl->ptr;
                Any* w = manifast_object_get_raw(cobj, "width");
                if (w && w->type == 0) cfg.width = (int)w->number;
                Any* h = manifast_object_get_raw(cobj, "height");
                if (h && h->type == 0) cfg.height = (int)h->number;
            }
            return cfg;
        };

        static auto extract_series = [](Any* args, int argIdx) {
            manifast::plot::Series s;
            s.color = 0;
            if (args[argIdx].type == 6 && args[argIdx+1].type == 6) {
                ManifastArray* xa = (ManifastArray*)args[argIdx].ptr;
                ManifastArray* ya = (ManifastArray*)args[argIdx+1].ptr;
                for (uint32_t i = 0; i < xa->size; i++)
                    s.x.push_back(xa->elements[i].type == 0 ? xa->elements[i].number : 0);
                for (uint32_t i = 0; i < ya->size; i++)
                    s.y.push_back(ya->elements[i].type == 0 ? ya->elements[i].number : 0);
            }
            return s;
        };

        auto plot_line = [](void* vm, Any* args, int nargs) {
            int idx = 0; if (nargs >= 1 && args[0].type != 6) idx++;
            if (nargs - idx < 2) { args[-1] = {3, 0.0, nullptr}; return; }
            auto cfg = extract_config(args, idx, nargs);
            g_plot.setConfig(cfg);
            g_plot.addSeries(extract_series(args, idx));
            g_plot_initialized = true;
            args[-1] = {2, 1.0, nullptr};
        };

        auto plot_scatter = [](void* vm, Any* args, int nargs) {
            int idx = 0; if (nargs >= 1 && args[0].type != 6) idx++;
            if (nargs - idx < 2) { args[-1] = {3, 0.0, nullptr}; return; }
            auto cfg = extract_config(args, idx, nargs);
            g_plot.setConfig(cfg);
            g_plot.addSeries(extract_series(args, idx));
            g_plot_initialized = true;
            args[-1] = {2, 1.0, nullptr};
        };

        auto plot_bar = [](void* vm, Any* args, int nargs) {
            int idx = 0; if (nargs >= 1 && args[0].type != 6) idx++;
            if (nargs - idx < 2) { args[-1] = {3, 0.0, nullptr}; return; }
            auto cfg = extract_config(args, idx, nargs);
            g_plot.setConfig(cfg);
            g_plot.addSeries(extract_series(args, idx));
            g_plot_initialized = true;
            args[-1] = {2, 1.0, nullptr};
        };

        auto plot_save = [](void* vm, Any* args, int nargs) {
            int idx = 0; if (nargs >= 1 && args[0].type != 1) idx++;
            if (nargs - idx < 1 || args[idx].type != 1 || !args[idx].ptr) {
                args[-1] = {2, 0.0, nullptr}; return;
            }
            std::string path = (char*)args[idx].ptr;
            bool ok = g_plot.saveToFile(path, manifast::plot::ChartType::Line);
            args[-1] = {2, ok ? 1.0 : 0.0, nullptr};
        };

        auto plot_show = [](void* vm, Any* args, int nargs) {
            g_plot.renderChart(manifast::plot::ChartType::Line);
            if (g_plot_show_callback) {
                const auto& fb = g_plot.getFramebuffer();
                g_plot_show_callback(fb.data(), g_plot.getWidth(), g_plot.getHeight());
                args[-1] = {2, 1.0, nullptr};
            } else {
                bool ok = g_plot.showWindow(manifast::plot::ChartType::Line);
                args[-1] = {2, ok ? 1.0 : 0.0, nullptr};
            }
        };

        Any fn_line = {4, 0.0, (void*)+plot_line};
        Any fn_scatter = {4, 0.0, (void*)+plot_scatter};
        Any fn_bar = {4, 0.0, (void*)+plot_bar};
        Any fn_save = {4, 0.0, (void*)+plot_save};
        Any fn_show = {4, 0.0, (void*)+plot_show};
        Any fn_reset = {4, 0.0, (void*)+plot_reset};
        manifast_object_set(obj, "line", &fn_line);
        manifast_object_set(obj, "scatter", &fn_scatter);
        manifast_object_set(obj, "bar", &fn_bar);
        manifast_object_set(obj, "save", &fn_save);
        manifast_object_set(obj, "show", &fn_show);
        manifast_object_set(obj, "reset", &fn_reset);
        return obj;
    }
    return manifast_create_nil();
}

MF_API Any* manifast_call_dynamic(Any* callee, Any* args, int nargs) {
    if (callee->type == 4) { // Native
        ManifastNativeFn fn = (ManifastNativeFn)callee->ptr;
        std::vector<Any> call_args(nargs + 1);
        for(int i = 0; i < nargs; ++i) call_args[i+1] = args[i];
        
        fn(nullptr, &call_args[1], nargs);
        
        Any* res = (Any*)mf_malloc(sizeof(Any));
        *res = call_args[0];
        return res;
    } else if (callee->type == 8) { // Class (Constructor)
        Any* inst = manifast_create_instance(callee);
        ManifastClass* klass = (ManifastClass*)callee->ptr;
        Any* constructor = manifast_object_get_raw(klass->methods, "inisiasi");
        if (constructor && constructor->type != 3) {
            std::vector<Any> c_args(nargs + 1);
            c_args[0] = *inst; // self
            for (int i = 0; i < nargs; i++) c_args[i+1] = args[i];
            manifast_call_dynamic(constructor, c_args.data(), nargs + 1);
        }
        return inst;
    }
    
    std::string typeName = "unknown";
    switch(callee->type) {
        case 0: typeName = "angka"; break;
        case 1: typeName = "string"; break;
        case 2: typeName = "bool"; break;
        case 3: typeName = "nil"; break;
        case 6: typeName = "array"; break;
        case 7: typeName = "objek"; break;
        case 9: typeName = "objek"; break; // Instance
    }
    
    MANIFAST_THROW("Runtime Error: Panggilan ke non-fungsi (tipe " + std::to_string(callee->type) + ")");
}

#ifndef __EMSCRIPTEN__
MF_API void manifast_print_any(Any* any) {
    if (!any) {
        printf("null");
        return;
    }
    switch (any->type) {
        case 0:
            if (any->number == (long long)any->number)
                printf("%lld", (long long)any->number);
            else
                printf("%g", any->number);
            break;
        case 1: // String
            printf("%s", (char*)any->ptr);
            break;
        case 2: // Boolean
            printf("%s", any->number ? "benar" : "salah");
            break;
        case 3: // Nil
            printf("nil");
            break;
        case 4: // Native
            printf("[Fungsi Native]");
            break;
        case 5: // Bytecode
            printf("[Fungsi Bytecode]");
            break;
        case 6: // Array
            printf("[");
            {
                ManifastArray* arr = (ManifastArray*)any->ptr;
                for (uint32_t i = 0; i < arr->size; i++) {
                     manifast_print_any(&arr->elements[i]);
                     if (i < arr->size - 1) printf(", ");
                }
            }
            printf("]");
            break;
        case 7: // Object
            printf("{Objek}");
            break;
        case 8: // Class
            if (any->ptr) {
                ManifastClass* klass = (ManifastClass*)any->ptr;
                printf("[Kelas %s]", klass->name ? klass->name : "Anonim");
            } else {
                printf("[Kelas Anonim]");
            }
            break;
        case 9: // Instance
            if (any->ptr) {
                ManifastInstance* inst = (ManifastInstance*)any->ptr;
                if (inst->klass && inst->klass->name) {
                    printf("[Instance dari Kelas %s]", inst->klass->name);
                } else {
                    printf("[Instance Objek]");
                }
            } else {
                printf("[Instance Null]");
            }
            break;
        default:
            printf("tipe tidak dikenal %d", any->type);
    }
}

MF_API void manifast_println_any(Any* any) {
    manifast_print_any(any);
    printf("\n");
}
#endif // !__EMSCRIPTEN__

MF_API void manifast_printfmt(Any* fmt, Any* any) {
    // Placeholder for real printfmt
    manifast_print_any(any);
}

MF_API Any* manifast_input() {
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin)) {
        // Remove newline
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        return manifast_create_string(buf);
    }
    return manifast_create_string("");
}

#ifndef __EMSCRIPTEN__
MF_API void manifast_assert(Any* cond, Any* msg) {
    bool truth = false;
    if (cond->type == 0) truth = (cond->number != 0);
    else if (cond->type == 1) truth = (cond->ptr != nullptr);
    else if (cond->type == 2) truth = (cond->number != 0);
    else if (cond->type == 3) truth = false;
    else truth = true;

    if (!truth) {
        std::string assert_msg_str = "Assertion Failed";
        if (msg && msg->type == 1) {
            assert_msg_str = "Assertion Gagal: " + std::string((char*)msg->ptr);
        }
        throw manifast::RuntimeError(assert_msg_str);
    }
}
#endif // !__EMSCRIPTEN__

} // extern "C"
