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
        
        // We want to skip JMP if condReg is true (C=0)
        emit(createABC(OpCode::TEST, condReg, 0, 0));
        
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
    else if (auto* s = dynamic_cast<WhileStmt*>(stmt)) {
        int startPos = (int)currentChunk->code.size();
        int condReg = compile(s->condition.get());
        
        // if not condReg skip body
        emit(createABC(OpCode::TEST, condReg, 0, 0));
        int jmpEndIdx = emit(createAsBx(OpCode::JMP, 0, 0));
        int bodyStart = (int)currentChunk->code.size();
        
        compile(s->body.get());
        
        // Jump back to start
        int backPos = (int)currentChunk->code.size();
        emit(createAsBx(OpCode::JMP, 0, startPos - backPos - 1));
        
        // Patch exit jump
        int endPos = (int)currentChunk->code.size();
        currentChunk->code[jmpEndIdx] = createAsBx(OpCode::JMP, 0, endPos - bodyStart);
        
        freeReg();
    }
    else if (auto* s = dynamic_cast<ForStmt*>(stmt)) {
        beginScope();
        // untuk i = start ke end loop body
        // 1. Init var
        int rVar = allocReg();
        int rStart = compile(s->start.get());
        emit(createABC(OpCode::MOVE, rVar, rStart, 0));
        freeReg(); // start temp
        
        locals.push_back({s->varName, scopeDepth, rVar});
        
        int rEnd = compile(s->end.get()); // Keep end in reg for comparison
        
        int loopTop = (int)currentChunk->code.size();
        
        // condition: var <= end
        emit(createABC(OpCode::LE, 1, rVar, rEnd));
        emit(createAsBx(OpCode::JMP, 0, 1)); // Skip body jump if false
        int jmpEndIdx = emit(createAsBx(OpCode::JMP, 0, 0));
        int bodyStart = (int)currentChunk->code.size();
        
        compile(s->body.get());
        
        // increment: i = i + 1
        int k1 = makeConstant({0, 1.0, nullptr});
        int r1 = allocReg();
        emit(createABx(OpCode::LOADK, r1, k1));
        emit(createABC(OpCode::ADD, rVar, rVar, r1));
        freeReg(); 
        
        int loopBottom = (int)currentChunk->code.size();
        emit(createAsBx(OpCode::JMP, 0, loopTop - loopBottom - 1));
        
        int endPos = (int)currentChunk->code.size();
        currentChunk->code[jmpEndIdx] = createAsBx(OpCode::JMP, 0, endPos - bodyStart);
        
        freeReg(); // rEnd
        endScope();
    }
    else if (auto* s = dynamic_cast<FunctionStmt*>(stmt)) {
        Chunk* funcChunk = compileFunctionBody(s->params, s->body.get());
        
        // Define as global
        int kName = makeConstant({1, 0.0, mf_strdup(s->name.c_str())}); // Type 1: String
        
        // Create function object
        Any funcVal;
        funcVal.type = 5; // Type 5: Bytecode Function
        funcVal.number = 0.0;
        funcVal.ptr = funcChunk; // Store direct pointer
        
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

Chunk* Compiler::compileFunctionBody(const std::vector<std::string>& params, Stmt* body) {
    auto subChunk = std::make_unique<Chunk>();
    
    int oldReg = nextReg;
    auto oldLocals = locals;
    int oldScope = scopeDepth;
    Chunk* oldChunk = currentChunk;
    
    currentChunk = subChunk.get();
    nextReg = 0;
    locals.clear();
    scopeDepth = 0;
    
    // Setup parameters as locals
    for (int i = 0; i < (int)params.size(); i++) {
        locals.push_back({params[i], 0, i});
        nextReg++;
    }
    
    compile(body);
    
    // Emit implicit return if needed
    emit(createABC(OpCode::RETURN, 0, 1, 0));
    
    // Store sub-chunk in parent chunk
    oldChunk->functions.push_back(std::move(subChunk));
    int funcIdx = (int)oldChunk->functions.size() - 1;
    
    // Restore state
    currentChunk = oldChunk;
    nextReg = oldReg;
    locals = oldLocals;
    scopeDepth = oldScope;
    
    return oldChunk->functions.back().get();
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
        int k = makeConstant({1, 0.0, mf_strdup(e->value.c_str())}); 
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
        bool flip = false;
        switch (e->op) {
            case TokenType::Plus: op = OpCode::ADD; break;
            case TokenType::Minus: op = OpCode::SUB; break;
            case TokenType::Star: op = OpCode::MUL; break;
            case TokenType::Slash: op = OpCode::DIV; break;
            case TokenType::Less: op = OpCode::LT; isCompare = true; break;
            case TokenType::Greater: op = OpCode::LT; isCompare = true; flip = true; break;
            case TokenType::LessEqual: op = OpCode::LE; isCompare = true; break;
            case TokenType::GreaterEqual: op = OpCode::LE; isCompare = true; flip = true; break;
            case TokenType::EqualEqual: op = OpCode::EQ; isCompare = true; break;
            case TokenType::BangEqual: op = OpCode::EQ; isCompare = true; break; // Handled by TEST inversion logic
            default: break;
        }
        
        if (isCompare) {
            int aVal = (e->op == TokenType::BangEqual) ? 0 : 1;
            int rL = flip ? right : left;
            int rR = flip ? left : right;
            emit(createABC(op, aVal, rL, rR));
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
            int k = makeConstant({1, 0.0, mf_strdup(e->name.c_str())}); 
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
    else if (auto* e = dynamic_cast<FunctionExpr*>(expr)) {
        Chunk* funcChunk = compileFunctionBody(e->params, e->body.get());
        
        // Create function object result
        Any funcVal;
        funcVal.type = 5; // Type 5: Bytecode Function
        funcVal.number = 0.0;
        funcVal.ptr = funcChunk;
        
        int kFunc = makeConstant(funcVal);
        int r = allocReg();
        emit(createABx(OpCode::LOADK, r, kFunc));
        return r;
    }

    return allocReg(); // Fallback
}

} // namespace vm
} // namespace manifast
