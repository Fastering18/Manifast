#pragma once

#include "Chunk.h"
#include <vector>

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
};

} // namespace vm
} // namespace manifast
