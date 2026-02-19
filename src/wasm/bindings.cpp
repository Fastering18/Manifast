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
    else if (val->type == 4) g_wasm_output += "[Fungsi Native]";
    else if (val->type == 5) g_wasm_output += "[Fungsi]";
    else g_wasm_output += "{Objek}";
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
}

void wasm_assert(manifast::vm::VM* vm, ::Any* args, int nargs) {
    if (nargs < 1) {
        MANIFAST_THROW("Runtime Error: assert() membutuhkan minimal 1 argumen");
    }
    
    bool truth = false;
    if (args[0].type == 2) truth = (args[0].number != 0);
    else if (args[0].type == 3) truth = false;
    else if (args[0].type == 0) truth = (args[0].number != 0);
    else truth = true;

    if (!truth) {
        std::string msg = "Assertion Gagal";
        if (nargs >= 2 && args[1].type == 1) {
            msg = (char*)args[1].ptr;
        }
        MANIFAST_THROW("Runtime Error: [ASSERT GAGAL] " + msg);
    }
}

// Overrides for CodeGen/JIT
void manifast_print_any(::Any* val) {
    wasm_print_any(val);
}

void manifast_println_any(::Any* val) {
    wasm_print_any(val);
    g_wasm_output += "\n";
}

void manifast_assert(::Any* cond, ::Any* msg) {
    bool truth = false;
    if (cond->type == 2) truth = (cond->number != 0);
    else if (cond->type == 3) truth = false;
    else if (cond->type == 0) truth = (cond->number != 0);
    else truth = true;

    if (!truth) {
        std::string errMsg = "Assertion Gagal";
        if (msg && msg->type == 1) {
            errMsg = (char*)msg->ptr;
        }
        g_wasm_output += "\n[ASSERT GAGAL] " + errMsg + "\n";
    }
}

// manifast_array_len, manifast_array_push, manifast_array_pop are provided by Runtime.cpp (CORE_SOURCES)

void wasm_len(manifast::vm::VM* vm, ::Any* args, int nargs) {
    if (nargs < 1) {
        args[-1] = {0, 0, nullptr};
        return;
    }
    args[-1] = {0, manifast_array_len(&args[0]), nullptr};
}

const char* mf_run_script_tier(const char* source, int tier) {
    g_wasm_output = "";
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer, source);
    
    auto statements = parser.parse();
    
    manifast::vm::VM vm;
    vm.setTier((manifast::vm::Tier)tier);
    
    vm.defineNative("print", wasm_print);
    vm.defineNative("println", wasm_println);
    vm.defineNative("assert", wasm_assert);
    vm.defineNative("len", wasm_len);
    
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
