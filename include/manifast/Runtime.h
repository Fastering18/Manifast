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

struct Any {
    int32_t type;    // 0=Number, 1=String, 2=Boolean, 3=Nil, 4=Native, 5=Bytecode, 6=Array, 7=Object, 8=Class, 9=Instance
    double number;
    void* ptr;
};

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
MF_API void manifast_print_any(Any* any);
MF_API void manifast_println_any(Any* any);
MF_API void manifast_printfmt(Any* fmt, Any* any); // Simple version for now
MF_API Any* manifast_input();
MF_API void manifast_assert(Any* cond, Any* msg);

// Internal Memory Management (exported for tests if needed)
MF_API void* mf_malloc(size_t size);
MF_API char* mf_strdup(const char* s);

} // extern "C"

#ifdef __cplusplus
#include <stdexcept>
namespace manifast {
    class RuntimeError : public std::runtime_error {
    public:
        RuntimeError(const std::string& message) : std::runtime_error(message) {}
    };
}
#endif

#endif // MANIFAST_RUNTIME_H
