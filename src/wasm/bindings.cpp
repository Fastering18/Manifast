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
void wasm_print(manifast::vm::VM* vm, ::Any* args, int nargs) {
    for (int i = 0; i < nargs; ++i) {
        if (args[i].type == 0) {
            char buf[64];
            sprintf(buf, "%g", args[i].number);
            outputBuffer += buf;
        }
        else if (args[i].type == 1 && args[i].ptr) outputBuffer += (char*)args[i].ptr;
        else if (args[i].type == 2) outputBuffer += (args[i].number ? "true" : "false");
        else if (args[i].type == 3) outputBuffer += "nil";
        else outputBuffer += "[Object/Function]";
    }
}

void wasm_println(manifast::vm::VM* vm, ::Any* args, int nargs) {
    wasm_print(vm, args, nargs);
    outputBuffer += "\n";
}

const char* mf_run_script(const char* source) {
    outputBuffer.clear();
    
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer);
    
    // No try-catch as -fno-exceptions is used
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
    
    return outputBuffer.c_str(); 
}

}
