#include "manifast/Runtime.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {

static size_t g_allocated_memory = 0;

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
    a->ptr = mf_strdup(str);
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
    
    obj->entries[obj->size].key = mf_strdup(key);
    obj->entries[obj->size].value = *val_any;
    obj->size++;
}

MF_API void manifast_object_set(Any* obj_any, const char* key, Any* val_any) {
    if (obj_any->type != 7) return;
    manifast_object_set_raw((ManifastObject*)obj_any->ptr, key, val_any);
}

MF_API Any* manifast_object_get_raw(ManifastObject* obj, const char* key) {
    for (uint32_t i = 0; i < obj->size; ++i) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            return &obj->entries[i].value;
        }
    }
    static Any nilVal = {3, 0.0, nullptr};
    return &nilVal;
}

MF_API Any* manifast_object_get(Any* obj_any, const char* key) {
    if (obj_any->type != 7) {
        static Any nilVal = {3, 0.0, nullptr};
        return &nilVal;
    }
    return manifast_object_get_raw((ManifastObject*)obj_any->ptr, key);
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
            printf("%s", any->number ? "true" : "false");
            break;
        case 3: // Nil
            printf("nil");
            break;
        case 4: // Native
            printf("[Native Function]");
            break;
        case 5: // Bytecode
            printf("[Function]");
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
            printf("{Object}");
            break;
        default:
            printf("unknown type %d", any->type);
    }
}

MF_API void manifast_println_any(Any* any) {
    manifast_print_any(any);
    printf("\n");
}

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

MF_API void manifast_assert(Any* cond, Any* msg) {
    bool truth = false;
    if (cond->type == 0) truth = (cond->number != 0);
    else if (cond->type == 1) truth = (cond->ptr != nullptr);
    else if (cond->type == 2) truth = (cond->number != 0);
    else if (cond->type == 3) truth = false;
    else truth = true;

    if (!truth) {
        if (msg && msg->type == 1) {
            std::string errMsg = "Assertion Failed: " + std::string((char*)msg->ptr);
            fprintf(stderr, "%s\n", errMsg.c_str());
            throw manifast::RuntimeError(errMsg);
        } else {
            fprintf(stderr, "Assertion Failed\n");
            throw manifast::RuntimeError("Assertion Failed");
        }
    }
}


} // extern "C"
