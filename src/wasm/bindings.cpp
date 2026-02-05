#include "manifast/VM/VM.h"
#include "manifast/VM/Compiler.h"
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include <string>
#include <vector>

// Forward decl or include Runtime? 
// VM.h includes Runtime.h indirectly? No. VM.h -> Chunk.h -> Any? 
// Checking files... Chunk.h has Any? 
// Actually Runtime.h defines types. We need it.
#include "manifast/Runtime.h"

extern "C" {

// Custom Print functions for Web (Stream to stdout)
void wasm_print(manifast::vm::VM* vm, ::Any* args, int nargs);

void wasm_print_any(::Any* val, int depth = 0) {
    if (depth > 32) { printf("..."); return; }

    if (val->type == 0) {
        printf("%g", val->number);
    }
    else if (val->type == 1 && val->ptr) printf("%s", (char*)val->ptr);
    else if (val->type == 2) printf("%s", val->number ? "benar" : "salah");
    else if (val->type == 3) printf("nil");
    else if (val->type == 6 && val->ptr) { // Array
        ManifastArray* arr = (ManifastArray*)val->ptr;
        if (arr->size > 10000) { printf("[Large Array]"); return; }
        printf("[");
        for (uint32_t i = 0; i < arr->size; i++) {
            if (i > 0) printf(", ");
            wasm_print_any(&arr->elements[i], depth + 1);
        }
        printf("]");
    }
    else if (val->type == 7 && val->ptr) { // Object
        ManifastObject* obj = (ManifastObject*)val->ptr;
        if (obj->size > 10000) { printf("{Large Object}"); return; }
        printf("{");
        for (uint32_t i = 0; i < obj->size; i++) {
            if (i > 0) printf(", ");
            if (obj->entries[i].key) printf("%s", obj->entries[i].key);
            printf(": ");
            wasm_print_any(&obj->entries[i].value, depth + 1);
        }
        printf("}");
    }
    else if (val->type == 8 && val->ptr) { // Class
        ManifastClass* klass = (ManifastClass*)val->ptr;
        printf("[Kelas %s]", klass->name ? klass->name : "?");
    }
    else if (val->type == 9 && val->ptr) { // Instance
        ManifastInstance* inst = (ManifastInstance*)val->ptr;
        printf("[Instance of %s]", inst->klass->name ? inst->klass->name : "?");
    }
    else if (val->type == 4) printf("[Native Function]");
    else if (val->type == 5) printf("[Function]");
    else printf("{Object}");
}

void wasm_print(manifast::vm::VM* vm, ::Any* args, int nargs) {
    for (int i = 0; i < nargs; ++i) {
        wasm_print_any(&args[i]);
    }
    fflush(stdout);
}

void wasm_println(manifast::vm::VM* vm, ::Any* args, int nargs) {
    wasm_print(vm, args, nargs);
    printf("\n");
    fflush(stdout);
}

const char* mf_run_script_debug(const char* source, bool debugDev) {
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer, source);
    parser.debugMode = debugDev;
    
    auto statements = parser.parse();
    
    manifast::vm::Chunk chunk;
    manifast::vm::Compiler compiler;
    compiler.debugMode = debugDev;
    
    if (compiler.compile(statements, chunk)) {
        if (debugDev) {
            fprintf(stderr, "Parsed %zu statements. Compiled %zu instructions.\n", statements.size(), chunk.code.size());
            fflush(stderr);
        }
        manifast::vm::VM vm;
        vm.debugMode = debugDev;
        
        vm.defineNative("print", wasm_print);
        vm.defineNative("println", wasm_println);
        
        vm.interpret(&chunk, source);
        chunk.free();
    } else {
        printf("Compilation Failed\n");
    }
    
    return ""; 
}

const char* mf_run_script(const char* source) {
    return mf_run_script_debug(source, false);
}

}
