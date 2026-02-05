#pragma once

#include "OpCode.h"
#include "../Runtime.h"
#include <vector>
#include <string>
#include <memory>

namespace manifast {
namespace vm {

// Represents a block of bytecode (function body, script)
struct Chunk {
    std::string name;
    
    // Instructions
    std::vector<Instruction> code;
    
    // Debug info (line number per instruction)
    std::vector<int> lines;
    std::vector<int> offsets;
    
    // Constants pool
    std::vector<Any> constants;
    
    // Sub-functions (nested chunks)
    std::vector<std::unique_ptr<Chunk>> functions;
    
    void write(Instruction instruction, int line, int offset = -1) {
        code.push_back(instruction);
        lines.push_back(line);
        offsets.push_back(offset);
    }
    
    int addConstant(Any value) {
        constants.push_back(value);
        return (int)constants.size() - 1;
    }
    
    // Helpers
    void free() {
        code.clear();
        lines.clear();
        offsets.clear();
        constants.clear();
        functions.clear();
    }
};

} // namespace vm
} // namespace manifast
