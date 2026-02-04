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

// Global output buffer for the web
std::string outputBuffer;

// Custom Print functions for Web (Capture to buffer)
void wasm_print(manifast::vm::VM* vm, manifast::Any* args, int nargs) {
    for (int i = 0; i < nargs; ++i) {
        if (args[i].type == 0) outputBuffer += std::to_string(args[i].number);
        else if (args[i].type == 3 && args[i].ptr) outputBuffer += (char*)args[i].ptr;
        else outputBuffer += "nil";
    }
}

void wasm_println(manifast::vm::VM* vm, manifast::Any* args, int nargs) {
    wasm_print(vm, args, nargs);
    outputBuffer += "\n";
}

// Ensure Manifast Runtime symbols that might be missing if Runtime.cpp isn't fully linked
// Check if native functions need overrides? 
// VM.cpp registers "print" and "println" in constructor.
// We must override them AFTER VM construction.

const char* mf_run_script(const char* source) {
    outputBuffer.clear();
    
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer);
    
    try {
        auto statements = parser.parse();
        
        manifast::vm::Chunk chunk;
        manifast::vm::Compiler compiler;
        
        if (compiler.compile(statements, chunk)) {
            manifast::vm::VM vm;
            
            // Override Native Functions for Web Output
            vm.defineNative("print", wasm_print);
            vm.defineNative("println", wasm_println);
            
            vm.interpret(&chunk);
            chunk.free();
        } else {
             outputBuffer += "Compilation Failed\n";
        }
    } catch (const std::exception& e) {
        outputBuffer += "Error: ";
        outputBuffer += e.what();
        outputBuffer += "\n";
    } catch (...) {
        outputBuffer += "Unknown Error\n";
    }
    
    return outputBuffer.c_str(); 
}

}
