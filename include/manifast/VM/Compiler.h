#pragma once

#include "Chunk.h"
#include "../AST.h"
#include <vector>
#include <map>
#include <string>

namespace manifast {
namespace vm {

class Compiler {
public:
    Compiler();
    
    // Compile AST into a Chunk
    // Returns true on success
    bool compile(const std::vector<std::unique_ptr<Stmt>>& statements, Chunk& chunk);

private:
    Chunk* currentChunk;
    int nextReg; // Next available register index (RegStack pointer)
    
    // Scopes for local variables checking
    struct Local {
        std::string name;
        int depth;
        int reg; // Which register holds this local
    };
    
    std::vector<Local> locals;
    int scopeDepth;
    
    // Dispatch
    void compile(Stmt* stmt);
    int compile(Expr* expr); // Returns register index containing result
    
    // Helpers
    int emit(Instruction i, int line = 0);
    int makeConstant(Any value);
    int resolveLocal(const std::string& name);
    int allocReg();
    void freeReg(); // Pop last reg
    
    Chunk* compileFunctionBody(const std::vector<std::string>& params, Stmt* body);
    
    void beginScope();
    void endScope();
};

} // namespace vm
} // namespace manifast
