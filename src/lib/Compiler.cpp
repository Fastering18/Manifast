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
    
    for (const auto& stmt : statements) {
        compile(stmt.get());
    }
    
    // Emit return 0 at end
    emit(createABC(OpCode::RETURN, 0, 1, 0)); 
    return true;
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
        int condReg = compile(s->condition.get());
        
        // Skip next if condReg is false
        // TEST A C: if not (R(A) <=> C) then pc++
        // We want to skip JMP if condReg is true (C=1)
        emit(createABC(OpCode::TEST, condReg, 0, 1));
        
        int jmpIdx = emit(createAsBx(OpCode::JMP, 0, 0)); // Placeholder
        int startPos = (int)currentChunk->code.size();
        
        compile(s->thenBranch.get());
        
        if (s->elseBranch) {
            // Need to jump over else branch
            int outJmpIdx = emit(createAsBx(OpCode::JMP, 0, 0));
            
            // Patch first jump to else branch
            int elsePos = (int)currentChunk->code.size();
            currentChunk->code[jmpIdx] = createAsBx(OpCode::JMP, 0, elsePos - startPos);
            
            int elseStart = (int)currentChunk->code.size();
            compile(s->elseBranch.get());
            int endPos = (int)currentChunk->code.size();
            
            // Patch second jump to end
            currentChunk->code[outJmpIdx] = createAsBx(OpCode::JMP, 0, endPos - elseStart);
        } else {
            int endPos = (int)currentChunk->code.size();
            currentChunk->code[jmpIdx] = createAsBx(OpCode::JMP, 0, endPos - startPos);
        }
        
        freeReg();
    }
    else if (auto* s = dynamic_cast<FunctionStmt*>(stmt)) {
        // 1. Create sub-chunk for function body
        auto subChunk = std::make_unique<Chunk>();
        
        // 2. Compile body in a new compiler context (or save/restore state)
        // For simplicity, we use same nextReg but reset it? No, functions have their own stack frame.
        int oldReg = nextReg;
        auto oldLocals = locals;
        int oldScope = scopeDepth;
        Chunk* oldChunk = currentChunk;
        
        currentChunk = subChunk.get();
        nextReg = 0;
        locals.clear();
        scopeDepth = 0;
        
        // Setup parameters as locals
        for (int i = 0; i < (int)s->params.size(); i++) {
            locals.push_back({s->params[i], 0, i});
            nextReg++;
        }
        
        compile(s->body.get());
        
        // Emit implicit return if needed
        emit(createABC(OpCode::RETURN, 0, 1, 0));
        
        // 3. Store sub-chunk in parent chunk
        oldChunk->functions.push_back(std::move(subChunk));
        int funcIdx = (int)oldChunk->functions.size() - 1;
        
        // 4. Restore state
        currentChunk = oldChunk;
        nextReg = oldReg;
        locals = oldLocals;
        scopeDepth = oldScope;
        
        // 5. Define as global or local? 
        // For now, Manifast functions are global.
        int kName = makeConstant({3, 0.0, mf_strdup(s->name.c_str())});
        
        // Create function object
        Any funcVal;
        funcVal.type = 5; // Bytecode Function
        funcVal.number = (double)funcIdx; // Store index in sub-functions vector
        funcVal.ptr = nullptr;
        
        int kFunc = makeConstant(funcVal);
        int r = allocReg();
        emit(createABx(OpCode::LOADK, r, kFunc));
        emit(createABx(OpCode::SETGLOBAL, r, kName));
        freeReg();
    }
    else if (auto* s = dynamic_cast<ReturnStmt*>(stmt)) {
        if (s->value) {
            int r = compile(s->value.get());
            emit(createABC(OpCode::RETURN, r, 2, 0)); // Return 1 result
            freeReg();
        } else {
            emit(createABC(OpCode::RETURN, 0, 1, 0)); // Return 0 results (nil)
        }
    }
}

int Compiler::compile(Expr* expr) {
    if (auto* e = dynamic_cast<NumberExpr*>(expr)) {
        int r = allocReg();
        int k = makeConstant({0, e->value, nullptr});
        emit(createABx(OpCode::LOADK, r, k));
        return r;
    }
    else if (auto* e = dynamic_cast<StringExpr*>(expr)) {
        int r = allocReg();
        // Create string constant. Note: we need to own the string memory.
        // For MVP, relying on Runtime managing it or leaking. 
        // Ideally Runtime.h has makeString. using mf_strdup for now.
        int k = makeConstant({3, 0.0, mf_strdup(e->value.c_str())}); 
        emit(createABx(OpCode::LOADK, r, k));
        return r;
    }
    else if (auto* e = dynamic_cast<BinaryExpr*>(expr)) {
        int left = compile(e->left.get());
        int right = compile(e->right.get());
        // Result reuses left register? Or new one?
        // Simple: reuse left, free right.
        
        OpCode op = OpCode::ADD;
        bool isCompare = false;
        switch (e->op) {
            case TokenType::Plus: op = OpCode::ADD; break;
            case TokenType::Minus: op = OpCode::SUB; break;
            case TokenType::Star: op = OpCode::MUL; break;
            case TokenType::Slash: op = OpCode::DIV; break;
            case TokenType::Less: op = OpCode::LT; isCompare = true; break;
            case TokenType::EqualEqual: op = OpCode::EQ; isCompare = true; break;
            // For now, mapping > and others to basic opcodes or reversing
            default: break;
        }
        
        if (isCompare) {
            // Comparisons in Lua VM work differently (inline skip)
            // LT A B C: if ((RK(B) < RK(C)) ~= A) then pc++
            // We want to store result in a register.
            // LT 1 left right
            // JMP 0 1 (Skip R=1)
            // LOADBOOL left 0 1 (Skip R=0)
            // LOADBOOL left 1 0
            
            emit(createABC(op, 1, left, right));
            emit(createAsBx(OpCode::JMP, 0, 1));
            emit(createABC(OpCode::LOADBOOL, left, 0, 1));
            emit(createABC(OpCode::LOADBOOL, left, 1, 0));
        } else {
            emit(createABC(op, left, left, right));
        }
        freeReg(); // Free right
        return left;
    }
    else if (auto* e = dynamic_cast<VariableExpr*>(expr)) {
        int local = resolveLocal(e->name);
        int r = allocReg();
        if (local != -1) {
            emit(createABC(OpCode::MOVE, r, local, 0));
        } else {
            // Global lookup
            int k = makeConstant({3, 0.0, mf_strdup(e->name.c_str())}); 
            // Note: Leaking strdup here if we don't manage constants properly.
            // Using createABx for GETGLOBAL? 
            // VM has GETGLOBAL as iABx?
            // OpCode.h says GETGLOBAL is OpCode.
            // iABx: Op(6) A(8) Bx(18).
            
            // Wait, VM: case OpCode::GETGLOBAL: int bx = GET_Bx(i); Any key = K(bx);
            // Yes, iABx.
            emit(createABx(OpCode::GETGLOBAL, r, k));
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
