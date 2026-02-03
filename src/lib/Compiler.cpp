#include "manifast/VM/Compiler.h"
#include <iostream>

namespace manifast {
namespace vm {

Compiler::Compiler() : nextReg(0), scopeDepth(0), currentChunk(nullptr) {}

bool Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& statements, Chunk& chunk) {
    currentChunk = &chunk;
    nextReg = 0;
    locals.clear();
    scopeDepth = 0;
    
    try {
        for (const auto& stmt : statements) {
            compile(stmt.get());
        }
        
        // Emit return 0 at end
        emit(createABC(OpCode::RETURN, 0, 1, 0)); 
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Compilation Error: " << e.what() << "\n";
        return false;
    }
}

// Helpers
int Compiler::emit(Instruction i, int line) {
    currentChunk->write(i, line);
    return (int)currentChunk->code.size() - 1;
}

int Compiler::allocReg() {
    return nextReg++;
}

void Compiler::freeReg() {
    nextReg--;
}

int Compiler::makeConstant(Any value) {
    return currentChunk->addConstant(value);
}

int Compiler::resolveLocal(const std::string& name) {
    for (int i = (int)locals.size() - 1; i >= 0; i--) {
        if (locals[i].name == name) {
            return locals[i].reg;
        }
    }
    return -1;
}

void Compiler::beginScope() {
    scopeDepth++;
}

void Compiler::endScope() {
    scopeDepth--;
    // Pop locals from this scope
    while (!locals.empty() && locals.back().depth > scopeDepth) {
        locals.pop_back();
        // Also free the register? 
        // In this simple compiler, locals just take up registers linearly.
        // We can reclaim them if we want to reuse registers.
        // For MVP, simplistic monotonic alloc or stack-like pop.
        nextReg--; 
    }
}

// Dispatch

void Compiler::compile(Stmt* stmt) {
    if (auto* s = dynamic_cast<ExprStmt*>(stmt)) {
        int r = compile(s->expression.get());
        freeReg(); // Statement expression result discarded
    }
    else if (auto* s = dynamic_cast<VarDeclStmt*>(stmt)) {
        int reg = allocReg(); // Allocate register for this variable
        if (s->initializer) {
            int initReg = compile(s->initializer.get());
            // Move initReg to reg
            emit(createABC(OpCode::MOVE, reg, initReg, 0));
            freeReg(); // Free temp reg
        } else {
             emit(createABC(OpCode::LOADNIL, reg, 1, 0));
        }
        
        locals.push_back({s->name, scopeDepth, reg});
        // Do NOT freeReg(), keeping it for the variable
    }
    else if (auto* s = dynamic_cast<BlockStmt*>(stmt)) {
        beginScope();
        for (auto& st : s->statements) {
            compile(st.get());
        }
        endScope();
    }
    else if (auto* s = dynamic_cast<IfStmt*>(stmt)) {
         // Should implement jumps here
    }
    // ... loops
}

int Compiler::compile(Expr* expr) {
    if (auto* e = dynamic_cast<NumberExpr*>(expr)) {
        int r = allocReg();
        int k = makeConstant({0, e->value, nullptr});
        // Use LOADK
        // Check if k fits in Bx
        emit(createABx(OpCode::LOADK, r, k));
        return r;
    }
    else if (auto* e = dynamic_cast<BinaryExpr*>(expr)) {
        int left = compile(e->left.get());
        int right = compile(e->right.get());
        // Result reuses left register? Or new one?
        // Simple: reuse left, free right.
        
        OpCode op = OpCode::ADD;
        switch (e->op) {
            case TokenType::Plus: op = OpCode::ADD; break;
            case TokenType::Minus: op = OpCode::SUB; break; // etc
            case TokenType::Star: op = OpCode::MUL; break;
            case TokenType::Slash: op = OpCode::DIV; break;
            default: break;
        }
        
        emit(createABC(op, left, left, right));
        freeReg(); // Free right
        return left;
    }
    else if (auto* e = dynamic_cast<VariableExpr*>(expr)) {
        int local = resolveLocal(e->name);
        int r = allocReg();
        if (local != -1) {
            emit(createABC(OpCode::MOVE, r, local, 0));
        } else {
            // Global? Or Error. For now error or nil.
            emit(createABC(OpCode::LOADNIL, r, 0, 0));
        }
        return r;
    }
    else if (auto* e = dynamic_cast<CallExpr*>(expr)) {
        // Evaluate callee
        int funcReg = compile(e->callee.get());
        // Evaluate args 
        
        for (auto& arg : e->args) {
            compile(arg.get()); 
            // Result is in newly alloc'd reg
        }
        
        // Emit CALL funcReg, numArgs+1, 1 (0 results? or 1?)
        emit(createABC(OpCode::CALL, funcReg, (int)e->args.size() + 1, 1));
        
        // Reset stack top to funcReg (function result replaces function)
        // Free args registers
        nextReg -= (int)e->args.size();
        return funcReg;
    }

    return allocReg(); // Fallback
}

} // namespace vm
} // namespace manifast
