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
    
    // Call Stack
    struct CallFrame {
        Chunk* chunk;
        int pc; // Program counter (index into code)
        int baseSlot; // Register window start (index into stack)
        int returnReg; // Target register in caller frame
    };
    
    std::vector<CallFrame> frames;
    
    void run();
    
    // Helpers
    void resetStack();
    
    std::unordered_map<std::string, Any> globals;
};

} // namespace vm
} // namespace manifast
