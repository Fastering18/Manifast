#pragma once

#include "manifast/VM/Chunk.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace manifast {
namespace vm {

    enum class Tier { T0, T1, T2 };
    
class VM {
public:
    VM();
    ~VM();

    void setTier(Tier t) { currentTier = t; }
    Tier getTier() const { return currentTier; }

    void interpret(Chunk* chunk, std::string_view source = "");
    void runtimeError(const std::string& message);
    
    // Globals
    using NativeFn = void (*)(VM* vm, Any* args, int nargs);
    void defineNative(const std::string& name, NativeFn fn);
    Any getLastResult() const { return lastResult; }
    std::vector<Chunk*> managedChunks; // Chunks owned by the VM (e.g. from impor)
    bool debugMode = false;

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
    
    void run(int entryFrameDepth);
    
    // Helpers
    void resetStack();
    
private:
    Any lastResult;
    std::string source;
    std::unordered_map<std::string, Any> globals;
    Tier currentTier = Tier::T0;
};

} // namespace vm
} // namespace manifast
