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

extern "C" {

struct Any {
    int32_t type;    // 0=Number, 1=Array, 2=Object, 3=String
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

// Runtime functions called by LLVM IR
MF_API Any* manifast_create_number(double val);
MF_API Any* manifast_create_string(const char* str);
MF_API Any* manifast_create_array(uint32_t initial_size);
MF_API Any* manifast_create_object();
MF_API void manifast_object_set(Any* obj_any, const char* key, Any* val_any);
MF_API Any* manifast_object_get(Any* obj_any, const char* key);
MF_API void manifast_array_set(Any* arr_any, double index, Any* val_any);
MF_API Any* manifast_array_get(Any* arr_any, double index);
MF_API void manifast_print_any(Any* any);

} // extern "C"

#endif // MANIFAST_RUNTIME_H
