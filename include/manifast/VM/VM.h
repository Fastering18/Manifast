#pragma once

#include "manifast/VM/Chunk.h"
#include <vector>
#include <map>
#include <string>

namespace manifast {
namespace vm {

class VM {
public:
    VM();
    ~VM();

    void interpret(Chunk* chunk);

private:
    // Registers (Stack)
    // Lua uses a stack, where functions operate on a window (CallFrame)
    std::vector<Any> stack;
    Any* stackTop;
    
    // Call Stack
    struct CallFrame {
        Chunk* chunk;
        Instruction* ip;
        Any* slots; // Register window start (R0)
    };
    
    std::vector<CallFrame> frames;
    
    void run();
    
    // Helpers
    void resetStack();
    
    // Globals
    using NativeFn = void (*)(VM* vm, Any* args, int nargs);
    std::map<std::string, Any> globals;
    
    void defineNative(const std::string& name, NativeFn fn);
};

} // namespace vm
} // namespace manifast
