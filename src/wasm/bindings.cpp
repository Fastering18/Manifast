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

static std::string g_wasm_output;

void wasm_print_any(::Any* val, int depth = 0) {
    if (depth > 32) { g_wasm_output += "..."; return; }

    if (val->type == 0) {
        char buf[64];
        if (val->number == (long long)val->number) sprintf(buf, "%lld", (long long)val->number);
        else sprintf(buf, "%g", val->number);
        g_wasm_output += buf;
    }
    else if (val->type == 1 && val->ptr) g_wasm_output += (char*)val->ptr;
    else if (val->type == 2) g_wasm_output += (val->number ? "benar" : "salah");
    else if (val->type == 3) g_wasm_output += "nil";
    else if (val->type == 6 && val->ptr) { // Array
        ManifastArray* arr = (ManifastArray*)val->ptr;
        g_wasm_output += "[";
        for (uint32_t i = 0; i < arr->size; i++) {
            if (i > 0) g_wasm_output += ", ";
            wasm_print_any(&arr->elements[i], depth + 1);
        }
        g_wasm_output += "]";
    }
    else if (val->type == 7 && val->ptr) { // Object
        ManifastObject* obj = (ManifastObject*)val->ptr;
        g_wasm_output += "{";
        for (uint32_t i = 0; i < obj->size; i++) {
            if (i > 0) g_wasm_output += ", ";
            if (obj->entries[i].key) g_wasm_output += obj->entries[i].key;
            g_wasm_output += ": ";
            wasm_print_any(&obj->entries[i].value, depth + 1);
        }
        g_wasm_output += "}";
    }
    else if (val->type == 8 && val->ptr) {
        ManifastClass* klass = (ManifastClass*)val->ptr;
        g_wasm_output += "[Kelas ";
        g_wasm_output += (klass->name ? klass->name : "?");
        g_wasm_output += "]";
    }
    else if (val->type == 9 && val->ptr) {
        ManifastInstance* inst = (ManifastInstance*)val->ptr;
        g_wasm_output += "[Instance of ";
        g_wasm_output += (inst->klass->name ? inst->klass->name : "?");
        g_wasm_output += "]";
    }
    else if (val->type == 4) g_wasm_output += "[Native Function]";
    else if (val->type == 5) g_wasm_output += "[Function]";
    else g_wasm_output += "{Object}";
}

void wasm_print(manifast::vm::VM* vm, ::Any* args, int nargs) {
    for (int i = 0; i < nargs; ++i) {
        if (i > 0) g_wasm_output += "\t";
        wasm_print_any(&args[i]);
    }
    // Also print to actual stdout for console visibility
    printf("%s", g_wasm_output.c_str());
    fflush(stdout);
}

void wasm_println(manifast::vm::VM* vm, ::Any* args, int nargs) {
    wasm_print(vm, args, nargs);
    g_wasm_output += "\n";
    printf("\n");
    fflush(stdout);
}

// Overrides for CodeGen/JIT
void manifast_print_any(::Any* val) {
    wasm_print_any(val);
}

void manifast_println_any(::Any* val) {
    wasm_print_any(val);
    g_wasm_output += "\n";
}

const char* mf_run_script_tier(const char* source, int tier) {
    g_wasm_output = "";
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer, source);
    
    auto statements = parser.parse();
    
    manifast::vm::VM vm;
    vm.setTier((manifast::vm::VM::Tier)tier);
    
    vm.defineNative("print", wasm_print);
    vm.defineNative("println", wasm_println);
    
    // For Tier 1/2, use the LLVM JIT backend (Native only)
#ifndef __EMSCRIPTEN__
    if (tier > 0) {
        manifast::CodeGen codegen;
        codegen.compile(statements);
        if (codegen.run()) {
            // Captured
        } else {
            g_wasm_output = "JIT/AOT Execution Failed";
        }
    } else {
#endif
        // Fallback for Wasm or Tier 0
        manifast::vm::Chunk chunk;
        manifast::vm::Compiler compiler;
        
        if (compiler.compile(statements, chunk)) {
            vm.interpret(&chunk, source);
            chunk.free();
        } else {
            g_wasm_output = "Compilation Failed";
        }
#ifndef __EMSCRIPTEN__
    }
#endif
    
    static std::string last_output;
    last_output = g_wasm_output;
    return last_output.c_str();
}

const char* mf_run_script(const char* source) {
    return mf_run_script_tier(source, 0); // Default to Tier 0
}

}
