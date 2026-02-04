#include "manifast/Runtime.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {

static size_t g_allocated_memory = 0;

MF_API void* mf_malloc(size_t size) {
    if (g_allocated_memory + size > MANIFAST_MEM_LIMIT) {
        fprintf(stderr, "Error: Manifast memory limit exceeded (%zu bytes requested, %zu allocated)\n", size, g_allocated_memory);
        exit(1);
    }
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(1);
    }
    g_allocated_memory += size;
    return ptr;
}

MF_API char* mf_strdup(const char* s) {
    if (!s) return nullptr;
    size_t size = strlen(s) + 1;
    if (g_allocated_memory + size > MANIFAST_MEM_LIMIT) {
        fprintf(stderr, "Error: Manifast memory limit exceeded (strdup: %zu bytes)\n", size);
        exit(1);
    }
    char* ptr = strdup(s);
    if (!ptr) {
        fprintf(stderr, "Error: Out of memory\n");
        exit(1);
    }
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

MF_API void manifast_object_set(Any* obj_any, const char* key, Any* val_any) {
    if (obj_any->type != 7) return;
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
        size_t old_cap = obj->capacity;
        obj->capacity *= 2;
        size_t new_size = sizeof(ManifastObjectEntry) * obj->capacity;
        
        // Manual safe realloc
        void* new_ptr = mf_malloc(new_size);
        memcpy(new_ptr, obj->entries, sizeof(ManifastObjectEntry) * old_cap);
        // We don't free old entries here yet because we don't have mf_free, 
        // but for now let's just use realloc and track diff?
        // Actually, let's keep it simple and just use realloc with manual tracking.
        /*
        obj->entries = (ManifastObjectEntry*)realloc(obj->entries, new_size);
        g_allocated_memory += (new_size - sizeof(ManifastObjectEntry)*old_cap);
        */
        // Let's implement mf_realloc? No, let's just use realloc for now as it's common.
        void* reallocated = realloc(obj->entries, new_size);
        if(!reallocated) { fprintf(stderr, "Error: OOM on realloc\n"); exit(1); }
        obj->entries = (ManifastObjectEntry*)reallocated;
        g_allocated_memory += (sizeof(ManifastObjectEntry) * (obj->capacity - old_cap));
    }
    
    // Add new
    obj->entries[obj->size].key = mf_strdup(key);
    obj->entries[obj->size].value = *val_any;
    obj->size++;
}

MF_API Any* manifast_object_get(Any* obj_any, const char* key) {
    if (obj_any->type != 7) return manifast_create_number(0);
    ManifastObject* obj = (ManifastObject*)obj_any->ptr;
    for (uint32_t i = 0; i < obj->size; ++i) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            Any* res = (Any*)mf_malloc(sizeof(Any));
            *res = obj->entries[i].value;
            return res;
        }
    }
    return manifast_create_number(0);
}

MF_API void manifast_array_set(Any* arr_any, double index_dbl, Any* val_any) {
    if (arr_any->type != 6) return;
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    uint32_t idx = (uint32_t)index_dbl;
    
    if (idx < arr->size) {
        arr->elements[idx] = *val_any;
    }
}

MF_API Any* manifast_array_get(Any* arr_any, double index_dbl) {
    if (arr_any->type != 6) return manifast_create_number(0);
    ManifastArray* arr = (ManifastArray*)arr_any->ptr;
    uint32_t idx = (uint32_t)index_dbl;
    
    if (idx < arr->size) {
        Any* res = (Any*)mf_malloc(sizeof(Any));
        *res = arr->elements[idx];
        return res;
    }
    return manifast_create_number(0);
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
            printf("[Array]");
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


} // extern "C"
