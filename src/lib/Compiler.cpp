#include "manifast/VM/Compiler.h"
#include <iostream>
#include <cstring>

namespace manifast {
namespace vm {

#define GET_OP(i) ((OpCode)((i) & 0x3F))

Compiler::Compiler() : nextReg(0), scopeDepth(0), currentChunk(nullptr) {}

bool Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& statements, Chunk& chunk, const std::string& name) {
    chunk.name = name;
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
int Compiler::emit(Instruction i, int line, int offset) {
    currentChunk->write(i, line, offset);
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
            emit(createABC(OpCode::MOVE, reg, initReg, 0), s->line, s->offset);
            freeReg(); // Free temp reg
        } else {
             emit(createABC(OpCode::LOADNIL, reg, 1, 0), s->line, s->offset);
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
        emit(createABC(OpCode::TEST, condReg, 0, 0), s->line, s->offset);
        
        int jmpIdx = emit(createAsBx(OpCode::JMP, 0, 0), s->line, s->offset); // Placeholder
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
        emit(createABC(OpCode::TEST, condReg, 0, 0), s->line, s->offset);
        int jmpEndIdx = emit(createAsBx(OpCode::JMP, 0, 0), s->line, s->offset);
        int bodyStart = (int)currentChunk->code.size();
        
        compile(s->body.get());
        
        // Jump back to start
        int backPos = (int)currentChunk->code.size();
        emit(createAsBx(OpCode::JMP, 0, startPos - backPos - 1), s->line, s->offset);
        
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
        Chunk* funcChunk = compileFunctionBody(s->params, s->body.get(), s->name);
        
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
    else if (auto* s = dynamic_cast<ClassStmt*>(stmt)) {
        int r = compileClass(s);
        freeReg(); // free the class object if it's just a statement?
        // Actually, classes are usually stored in a variable by the identifier name.
    }
    else if (auto* s = dynamic_cast<ReturnStmt*>(stmt)) {
        if (s->value) {
            int r = compile(s->value.get());
            emit(createABC(OpCode::RETURN, r, 2, 0), s->line, s->offset); // Return 1 result
            freeReg();
        } else {
            emit(createABC(OpCode::RETURN, 0, 1, 0), s->line, s->offset); // Return 0 results (nil)
        }
    }
}

Chunk* Compiler::compileFunctionBody(const std::vector<std::string>& params, Stmt* body, const std::string& name) {
    Chunk* chunk = new Chunk();
    chunk->name = name; 
    
    Compiler sub;
    sub.currentChunk = chunk;
    sub.beginScope(); // Parameters at depth 1
    
    for (const auto& p : params) {
        int r = sub.allocReg();
        sub.locals.push_back({p, sub.scopeDepth, r});
    }
    
    if (auto* b = dynamic_cast<BlockStmt*>(body)) {
        for (const auto& s : b->statements) {
            sub.compile(s.get());
        }
    } else {
        sub.compile(body);
    }
    sub.endScope();
    
    // Ensure return if not present
    if (chunk->code.empty() || GET_OP(chunk->code.back()) != OpCode::RETURN) {
        chunk->write(createABC(OpCode::RETURN, 0, 1, 0), 0, -1);
    }
    
    return chunk;
}

int Compiler::compileClass(ClassStmt* stmt) {
    int r = allocReg();
    int kName = makeConstant({1, 0.0, mf_strdup(stmt->name.c_str())});
    emit(createABx(OpCode::NEWCLASS, r, kName));
    
    for (auto& method : stmt->methods) {
        Chunk* mChunk = compileFunctionBody(method->params, method->body.get(), method->name); // Pass method name
        int kMethod = makeConstant({5, 0.0, mChunk}); // 5=Bytecode/Function
        int kMethodName = makeConstant({1, 0.0, mf_strdup(method->name.c_str())});
        
        // Use SETTABLE R(A)[K(B)] = RK(C)
        emit(createABC(OpCode::SETTABLE, r, kMethodName + 256, kMethod + 256));
    }
    
    // Store class in global variable
    int kClassName = makeConstant({1, 0.0, mf_strdup(stmt->name.c_str())});
    emit(createABx(OpCode::SETGLOBAL, r, kClassName));
    
    return r;
}

int Compiler::compile(Expr* expr) {
    if (auto* e = dynamic_cast<NumberExpr*>(expr)) {
        int r = allocReg();
        int k = makeConstant({0, e->value, nullptr});
        emit(createABx(OpCode::LOADK, r, k), e->line, e->offset);
        return r;
    }
    else if (auto* e = dynamic_cast<StringExpr*>(expr)) {
        int r = allocReg();
        std::string val = e->value;
        std::string processed;
        for (size_t i = 0; i < val.length(); i++) {
            if (val[i] == '\\' && i + 1 < val.length()) {
                i++;
                switch (val[i]) {
                    case 'n': processed += '\n'; break;
                    case 't': processed += '\t'; break;
                    case 'r': processed += '\r'; break;
                    case '\\': processed += '\\'; break;
                    case '"': processed += '\"'; break;
                    default: processed += '\\'; processed += val[i]; break;
                }
            } else {
                processed += val[i];
            }
        }
        int k = makeConstant({1, 0.0, mf_strdup(processed.c_str())}); 
        emit(createABx(OpCode::LOADK, r, k), e->line, e->offset);
        return r;
    }
    else if (auto* e = dynamic_cast<BoolExpr*>(expr)) {
        int r = allocReg();
        emit(createABC(OpCode::LOADBOOL, r, e->value ? 1 : 0, 0), e->line, e->offset);
        return r;
    }
    else if (dynamic_cast<NilExpr*>(expr)) {
        int r = allocReg();
        emit(createABC(OpCode::LOADNIL, r, 0, 0), expr->line, expr->offset);
        return r;
    }
    else if (auto* e = dynamic_cast<UnaryExpr*>(expr)) {
        int right = compile(e->right.get());
        OpCode op = OpCode::NOT;
        if (e->op == TokenType::Minus) {
             // We don't have UNM opcode yet, let's use 0 - x or add UNM
             // For now LOADK 0 then SUB
             int rZero = allocReg();
             int kZero = makeConstant({0, 0.0, nullptr});
             emit(createABx(OpCode::LOADK, rZero, kZero));
             emit(createABC(OpCode::SUB, rZero, rZero, right));
             freeReg(); // free right
             return rZero;
        } else if (e->op == TokenType::Tilde) {
             // Bitwise NOT, assume we have it or use XOR with -1
        }
        emit(createABC(op, right, right, 0));
        return right;
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
            case TokenType::Percent: op = OpCode::MOD; break;
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
            emit(createABC(op, aVal, rL, rR), e->line, e->offset);
            emit(createAsBx(OpCode::JMP, 0, 1), e->line, e->offset);
            emit(createABC(OpCode::LOADBOOL, left, 0, 1), e->line, e->offset);
            emit(createABC(OpCode::LOADBOOL, left, 1, 0), e->line, e->offset);
        } else {
            emit(createABC(op, left, left, right), e->line, e->offset);
        }
        freeReg(); // Free right
        return left;
    }
    else if (auto* e = dynamic_cast<AssignExpr*>(expr)) {
        if (auto* v = dynamic_cast<VariableExpr*>(e->target.get())) {
            int valReg = compile(e->value.get());
            int local = resolveLocal(v->name);
            if (local != -1) {
                emit(createABC(OpCode::MOVE, local, valReg, 0));
                freeReg();
                return local;
            } else {
                int k = makeConstant({1, 0.0, mf_strdup(v->name.c_str())});
                emit(createABx(OpCode::SETGLOBAL, valReg, k));
                return valReg;
            }
        } else if (auto* idx = dynamic_cast<IndexExpr*>(e->target.get())) {
            int objReg = compile(idx->object.get());
            int keyReg = compile(idx->index.get());
            int valReg = compile(e->value.get());
            emit(createABC(OpCode::SETTABLE, objReg, keyReg, valReg));
            nextReg -= 2; // free key and value (keep obj as result?)
            return valReg; // Return the assigned value
        } else if (auto* get = dynamic_cast<GetExpr*>(e->target.get())) {
            int objReg = compile(get->object.get());
            int kKey = makeConstant({1, 0.0, mf_strdup(get->name.c_str())});
            int valReg = compile(e->value.get());
            emit(createABC(OpCode::SETTABLE, objReg, kKey + 256, valReg));
            freeReg(); // free value
            return valReg;
        }
        return allocReg();
    }
    else if (auto* e = dynamic_cast<GetExpr*>(expr)) {
        int objReg = compile(e->object.get());
        int kKey = makeConstant({1, 0.0, mf_strdup(e->name.c_str())});
        // Result goes into objReg (reuse it)
        emit(createABC(OpCode::GETTABLE, objReg, objReg, kKey + 256));
        return objReg;
    }
    else if (auto* e = dynamic_cast<IndexExpr*>(expr)) {
        int objReg = compile(e->object.get());
        if (auto* slice = dynamic_cast<SliceExpr*>(e->index.get())) {
            int startReg = slice->start ? compile(slice->start.get()) : -1;
            int endReg = slice->end ? compile(slice->end.get()) : -1;
            
            // We use specialized GETSLICE R(A), R(B), RK(C), RK(D)
            // But my GETSLICE only has A, B, C, D? 
            // My OpCode.h says: GETSLICE,   // R(A) := R(B)[RK(C):RK(D)]
            // If start/end is null, we can use a special constant (nil or -1).
            
            int sVal = (startReg == -1) ? makeConstant({3, 0.0, nullptr}) + 256 : startReg;
            int eVal = (endReg == -1) ? makeConstant({3, 0.0, nullptr}) + 256 : endReg;
            
            emit(createABC(OpCode::GETSLICE, objReg, objReg, sVal));
            // Wait, GETSLICE needs 4 operands? A, B, C, D.
            // OpCode definition only has ABC. I should probably change GETSLICE format or use another.
            // Let's use 2 instructions or change the opcode.
            // For now, let's use a 4th operand in D if I have space? No, Instruction is 32bit.
            // ABC: Op=8, A=8, B=9, C=9.
            // I'll emitEnd(eVal) as the next instruction?
            emit(eVal); 
            
            if (endReg != -1) freeReg();
            if (startReg != -1) freeReg();
            return objReg;
        } else {
            int keyReg = compile(e->index.get());
            // Result goes into objReg (reuse it)
            emit(createABC(OpCode::GETTABLE, objReg, objReg, keyReg));
            freeReg(); // free keyReg
            return objReg;
        }
    }
    else if (auto* e = dynamic_cast<ArrayExpr*>(expr)) {
        int r = allocReg();
        emit(createABC(OpCode::NEWARRAY, r, (int)e->elements.size(), 0));
        for (int i = 0; i < (int)e->elements.size(); i++) {
            compile(e->elements[i].get()); // evaluates into nextReg
        }
        if (!e->elements.empty()) {
            emit(createABC(OpCode::SETLIST, r, (int)e->elements.size(), 1));
            nextReg -= (int)e->elements.size();
        }
        return r;
    }
    else if (auto* e = dynamic_cast<ObjectExpr*>(expr)) {
        int r = allocReg();
        emit(createABC(OpCode::NEWTABLE, r, 0, 0));
        for (auto& entry : e->entries) {
            int kKey = makeConstant({1, 0.0, mf_strdup(entry.first.c_str())});
            int valReg = compile(entry.second.get());
            emit(createABC(OpCode::SETTABLE, r, kKey + 256, valReg));
            freeReg();
        }
        return r;
    }
    else if (auto* e = dynamic_cast<VariableExpr*>(expr)) {
        int local = resolveLocal(e->name);
        int r = allocReg();
        if (local != -1) {
            emit(createABC(OpCode::MOVE, r, local, 0), e->line, e->offset);
        } else {
            // Global lookup
            int k = makeConstant({1, 0.0, mf_strdup(e->name.c_str())}); 
            emit(createABx(OpCode::GETGLOBAL, r, k), e->line, e->offset);
        }
        return r;
    }
    else if (auto* e = dynamic_cast<CallExpr*>(expr)) {
        if (auto* get = dynamic_cast<GetExpr*>(e->callee.get())) {
            // Method call budi.bicara()
            int objReg = compile(get->object.get());
            int kProp = makeConstant({1, 0.0, mf_strdup(get->name.c_str())});
            int funcReg = allocReg();
            emit(createABC(OpCode::GETTABLE, funcReg, objReg, kProp + 256));
            
            // Pass obj as first argument (self)
            int selfReg = allocReg();
            emit(createABC(OpCode::MOVE, selfReg, objReg, 0));
            
            for (auto& arg : e->args) {
                compile(arg.get()); 
            }
            
            emit(createABC(OpCode::CALL, funcReg, (int)e->args.size() + 2, 1), e->line, e->offset);
            
            // Result replaces budi, cleanup stack
            nextReg -= (int)e->args.size() + 1; // free args and self
            emit(createABC(OpCode::MOVE, objReg, funcReg, 0));
            freeReg(); // free funcReg
            return objReg;
        } else {
            // Normal Call
            int funcReg = compile(e->callee.get());
            for (auto& arg : e->args) {
                compile(arg.get()); 
            }
            emit(createABC(OpCode::CALL, funcReg, (int)e->args.size() + 1, 1), e->line, e->offset);
            nextReg -= (int)e->args.size();
            return funcReg;
        }
    }
    else if (auto* e = dynamic_cast<FunctionExpr*>(expr)) {
        Chunk* funcChunk = compileFunctionBody(e->params, e->body.get(), "<lambda>");
        
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
