#ifndef MANIFAST_RUNTIME_H
#define MANIFAST_RUNTIME_H

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

extern "C" {

struct Any {
    int32_t type;    // 0=Number, 1=Array, 2=Object
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
Any* manifast_create_number(double val);
Any* manifast_create_array(uint32_t initial_size);
Any* manifast_create_object();
void manifast_object_set(Any* obj_any, const char* key, Any* val_any);
void manifast_array_set(Any* arr_any, double index, Any* val_any);
Any* manifast_array_get(Any* arr_any, double index);
void manifast_print_any(Any* any);

} // extern "C"

#endif // MANIFAST_RUNTIME_H
