#pragma once

#include "manifast/VM/Chunk.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace manifast {
namespace vm {

class VM {
public:
    VM();
    ~VM();

    void interpret(Chunk* chunk);
    
    // Globals
    using NativeFn = void (*)(VM* vm, Any* args, int nargs);
    void defineNative(const std::string& name, NativeFn fn);

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
    
    std::unordered_map<std::string, Any> globals;
};

} // namespace vm
} // namespace manifast
