#ifndef MANIFAST_RUNTIME_H
#define MANIFAST_RUNTIME_H

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

#ifdef _WIN32
#define MF_API __declspec(dllexport)
#else
#define MF_API
#endif

// Constraints
#define MANIFAST_MEM_LIMIT (128 * 1024 * 1024) // 128MB for now

extern "C" {

enum AnyType {
    ANY_NUMBER = 0,
    ANY_STRING = 1,
    ANY_BOOLEAN = 2,
    ANY_NIL = 3,
    ANY_NATIVE = 4,
    ANY_BYTECODE = 5,
    ANY_ARRAY = 6,
    ANY_OBJECT = 7,
    ANY_CLASS = 8,
    ANY_INSTANCE = 9,
    ANY_INT8 = 10,
    ANY_INT16 = 11,
    ANY_INT32 = 12,
    ANY_INT64 = 13,
    ANY_FLOAT32 = 14,
    ANY_FLOAT64 = 15,
    ANY_CHAR = 16
};

struct Any {
    int32_t type;    // Uses AnyType enum
    double number;
    void* ptr;
};

typedef void (*ManifastNativeFn)(void* vm, Any* args, int nargs);

struct ManifastArray {
    uint32_t size;
    uint32_t capacity;
    Any* elements;
};

struct ManifastObjectEntry {
    char* key;
    Any value;
};

struct ManifastObject {
    uint32_t size;
    uint32_t capacity;
    ManifastObjectEntry* entries;
};

struct ManifastClass {
    char* name;
    ManifastObject* methods; // Methods are stored in an object-like structure
};

struct ManifastInstance {
    ManifastClass* klass;
    ManifastObject* fields;
};

// Runtime functions called by LLVM IR
MF_API Any* manifast_create_number(double val);
MF_API Any* manifast_create_string(const char* str);
MF_API Any* manifast_create_boolean(bool val);
MF_API Any* manifast_create_nil();
MF_API Any* manifast_create_array(uint32_t initial_size);
MF_API Any* manifast_create_object();
MF_API Any* manifast_create_class(const char* name);
MF_API Any* manifast_create_instance(Any* class_any);
MF_API void manifast_object_set(Any* obj_any, const char* key, Any* val_any);
MF_API Any* manifast_object_get(Any* obj_any, const char* key);
MF_API void manifast_object_set_raw(ManifastObject* obj, const char* key, Any* val_any);
MF_API Any* manifast_object_get_raw(ManifastObject* obj, const char* key);
MF_API void manifast_array_set(Any* arr_any, double index, Any* val_any);
MF_API Any* manifast_array_get(Any* arr_any, double index);
MF_API double manifast_array_len(Any* arr_any);
MF_API void manifast_array_push(Any* arr_any, Any* val_any);
MF_API Any* manifast_array_pop(Any* arr_any);
MF_API void manifast_print_any(Any* any);
MF_API void manifast_println_any(Any* any);
MF_API void manifast_printfmt(Any* fmt, Any* any); // Simple version for now
MF_API Any* manifast_input();
MF_API void manifast_assert(Any* cond, Any* msg);
MF_API Any* manifast_impor(const char* name);
MF_API void manifast_class_add_method(Any* class_any, const char* name, ManifastNativeFn fn);
MF_API Any* manifast_call_dynamic(Any* callee, Any* args, int nargs);
MF_API void manifast_plot_for(Any* y_arr, Any* x_arr, Any* config);
MF_API void manifast_type_check(Any* val, int expected_type);

// Internal Memory Management (exported for tests if needed)
MF_API void* mf_malloc(size_t size);
MF_API char* mf_strdup(const char* s);

#include <stdint.h>

// Plot callback for WASM/embedded use
typedef void (*ManifastPlotShowCallback)(const uint8_t* rgba, int w, int h);
MF_API void manifast_set_plot_show_callback(ManifastPlotShowCallback cb);

// Clear output callback
typedef void (*ManifastClearOutputCallback)();
MF_API void manifast_set_clear_output_callback(ManifastClearOutputCallback cb);

} // extern "C"

#ifdef __cplusplus
#include <stdexcept>
#include <string>

#if defined(__EMSCRIPTEN__) && !defined(__EXCEPTIONS)
#include <iostream>
#include <cstdlib>
#define MANIFAST_THROW(msg) do { std::cerr << "Runtime Error: " << msg << std::endl; std::abort(); } while(0)
#else
#define MANIFAST_THROW(msg) throw manifast::RuntimeError(msg)
#endif

namespace manifast {
    class RuntimeError : public std::runtime_error {
    public:
        RuntimeError(const std::string& message) : std::runtime_error(message) {}
    };
}
#endif

#endif // MANIFAST_RUNTIME_H
