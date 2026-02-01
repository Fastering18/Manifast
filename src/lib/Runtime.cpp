#include "manifast/Runtime.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {

Any* manifast_create_number(double val) {
    Any* a = (Any*)malloc(sizeof(Any));
    a->type = 0;
    a->number = val;
    a->ptr = nullptr;
    return a;
}

Any* manifast_create_array(uint32_t initial_size) {
    Any* a = (Any*)malloc(sizeof(Any));
    a->type = 1;
    a->number = 0;
    
    ManifastArray* arr = (ManifastArray*)malloc(sizeof(ManifastArray));
    arr->size = initial_size;
    arr->capacity = initial_size > 0 ? initial_size : 4;
    arr->elements = (Any*)malloc(sizeof(Any) * arr->capacity);
    
    // Initialize elements to 0
    for(uint32_t i = 0; i < arr->size; ++i) {
        arr->elements[i].type = 0;
        arr->elements[i].number = 0;
        arr->elements[i].ptr = nullptr;
    }
    
    a->ptr = arr;
    return a;
}

Any* manifast_create_object() {
    Any* a = (Any*)malloc(sizeof(Any));
    a->type = 2;
    a->number = 0;
    
    ManifastObject* obj = (ManifastObject*)malloc(sizeof(ManifastObject));
    obj->size = 0;
    obj->capacity = 4;
    obj->entries = (ManifastObjectEntry*)malloc(sizeof(ManifastObjectEntry) * obj->capacity);
    
    a->ptr = obj;
    return a;
}

void manifast_object_set(Any* obj_any, const char* key, Any* val_any) {
    if (obj_any->type != 2) return;
    ManifastObject* obj = (ManifastObject*)obj_any->ptr;
    
    // Check if exists
    for (uint32_t i = 0; i < obj->size; ++i) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            obj->entries[i].value = *val_any;
            return;
        }
    }
    
    // Resize if needed
    if (obj->size == obj->capacity) {
        obj->capacity *= 2;
        obj->entries = (ManifastObjectEntry*)realloc(obj->entries, sizeof(ManifastObjectEntry) * obj->capacity);
    }
    
    // Add new
    obj->entries[obj->size].key = strdup(key);
    obj->entries[obj->size].value = *val_any;
    obj->size++;
}

void manifast_array_set(Any* arr_any, double index_dbl, Any* val_any) {
    if (arr_any->type != 1) return;
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    uint32_t idx = (uint32_t)index_dbl;
    
    if (idx < arr->size) {
        arr->elements[idx] = *val_any;
    }
}

Any* manifast_array_get(Any* arr_any, double index_dbl) {
    if (arr_any->type != 1) return manifast_create_number(0);
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    uint32_t idx = (uint32_t)index_dbl;
    
    if (idx < arr->size) {
        Any* res = (Any*)malloc(sizeof(Any));
        *res = arr->elements[idx];
        return res;
    }
    return manifast_create_number(0);
}

void manifast_print_any(Any* any) {
    if (!any) {
        printf("null\n");
        return;
    }
    switch (any->type) {
        case 0:
            if (any->number == (long long)any->number)
                printf("%lld\n", (long long)any->number);
            else
                printf("%g\n", any->number);
            break;
        case 1:
            printf("[Array]\n");
            break;
        case 2:
            printf("{Object}\n");
            break;
        default:
            printf("unknown type %d\n", any->type);
    }
}

} // extern "C"
