#include "manifast/CodeGen.h"
#include <llvm/IR/Verifier.h>
#include <iostream>

#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

namespace manifast {

CodeGen::CodeGen() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("ManifastModule", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

void CodeGen::compile(const std::vector<std::unique_ptr<Stmt>>& statements) {
    // Create a main function to hold top-level statements
    llvm::FunctionType* funcType = llvm::FunctionType::get(builder->getInt32Ty(), false);
    llvm::Function* mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entry);

    for (const auto& stmt : statements) {
        generateStmt(stmt.get());
    }

    // Default return 0
    builder->CreateRet(builder->getInt32(0));
    
    // Verify
    if (llvm::verifyFunction(*mainFunc, &llvm::errs())) {
        std::cerr << "Error: Function verification failed!\n";
    }
}

void CodeGen::printIR() {
    module->print(llvm::errs(), nullptr);
}

void CodeGen::run() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    auto JIT = llvm::orc::LLJITBuilder().create();
    if (!JIT) {
        std::cerr << "Error: Failed to create LLJIT\n";
        return;
    }

    // Move module to JIT
    auto TSM = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
    if (auto Err = JIT.get()->addIRModule(std::move(TSM))) {
        std::cerr << "Error: Failed to add IR module to JIT\n";
        return;
    }

    // Look up main
    // LLVM 18: lookup returns Expected<ExecutorSymbolDef>
    auto SymOrErr = JIT.get()->lookup("main");
    if (!SymOrErr) {
        std::cerr << "Error: main function not found in JIT\n";
        return;
    }

    // ExecutorSymbolDef -> getAddress() -> ExecutorAddr -> toPtr()
    auto MainAddr = SymOrErr->getAddress();
    int (*MainPtr)() = MainAddr.toPtr<int (*)()>();
    
    int result = MainPtr();
    std::cout << "--- Execution Result ---\n";
    std::cout << "Return code: " << result << "\n";
}

llvm::Value* CodeGen::generateExpr(const Expr* expr) {
    if (auto* num = dynamic_cast<const NumberExpr*>(expr)) return visitNumberExpr(num);
    if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) return visitBinaryExpr(bin);
    if (auto* var = dynamic_cast<const VariableExpr*>(expr)) return visitVariableExpr(var);
    if (auto* assign = dynamic_cast<const AssignExpr*>(expr)) return visitAssignExpr(assign);
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) return visitCallExpr(call);
    return nullptr;
}

void CodeGen::generateStmt(const Stmt* stmt) {
    if (auto* exprStmt = dynamic_cast<const ExprStmt*>(stmt)) {
        generateExpr(exprStmt->expression.get());
    } else if (auto* varDecl = dynamic_cast<const VarDeclStmt*>(stmt)) {
        visitVarDeclStmt(varDecl);
    } else if (auto* retStmt = dynamic_cast<const ReturnStmt*>(stmt)) {
        visitReturnStmt(retStmt);
    } else if (auto* block = dynamic_cast<const BlockStmt*>(stmt)) {
        visitBlockStmt(block);
    } else if (auto* ifStmt = dynamic_cast<const IfStmt*>(stmt)) {
        visitIfStmt(ifStmt);
    } else if (auto* whileStmt = dynamic_cast<const WhileStmt*>(stmt)) {
        visitWhileStmt(whileStmt);
    } else if (auto* forStmt = dynamic_cast<const ForStmt*>(stmt)) {
        visitForStmt(forStmt);
    } else if (auto* funcStmt = dynamic_cast<const FunctionStmt*>(stmt)) {
        visitFunctionStmt(funcStmt);
    }
}

void CodeGen::visitIfStmt(const IfStmt* stmt) {
    llvm::Value* condV = generateExpr(stmt->condition.get());
    if (!condV) return;

    // Convert condition to bool (double != 0)
    condV = builder->CreateFCmpONE(condV, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "ifcond");

    llvm::Function* func = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "then", func);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ifcont");

    if (stmt->elseBranch) {
        builder->CreateCondBr(condV, thenBB, elseBB);
    } else {
        builder->CreateCondBr(condV, thenBB, mergeBB);
    }

    // Emit then block
    builder->SetInsertPoint(thenBB);
    generateStmt(stmt->thenBranch.get());
    builder->CreateBr(mergeBB);
    thenBB = builder->GetInsertBlock();

    // Emit else block
    if (stmt->elseBranch) {
        func->insert(func->end(), elseBB);
        builder->SetInsertPoint(elseBB);
        generateStmt(stmt->elseBranch.get());
        builder->CreateBr(mergeBB);
        elseBB = builder->GetInsertBlock();
    }

    // Emit merge block
    func->insert(func->end(), mergeBB);
    builder->SetInsertPoint(mergeBB);
}

void CodeGen::visitWhileStmt(const WhileStmt* stmt) {
    llvm::Function* func = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context, "whilecond", func);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context, "whilebody");
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "afterwhile");

    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);

    llvm::Value* condV = generateExpr(stmt->condition.get());
    if (!condV) return;
    condV = builder->CreateFCmpONE(condV, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "whilecond");
    builder->CreateCondBr(condV, bodyBB, afterBB);

    func->insert(func->end(), bodyBB);
    builder->SetInsertPoint(bodyBB);
    generateStmt(stmt->body.get());
    builder->CreateBr(condBB);

    func->insert(func->end(), afterBB);
    builder->SetInsertPoint(afterBB);
}

void CodeGen::visitForStmt(const ForStmt* stmt) {
    llvm::Function* func = builder->GetInsertBlock()->getParent();

    // 1. Initializer
    llvm::Value* startV = generateExpr(stmt->start.get());
    if (!startV) return;

    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, stmt->varName);
    builder->CreateStore(startV, alloca);
    namedValues[stmt->varName] = alloca;

    // 2. Blocks
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context, "forcond", func);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context, "forbody");
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "afterfor");

    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);

    // 3. Condition (i <= end)
    llvm::Value* currV = builder->CreateLoad(llvm::Type::getDoubleTy(*context), alloca, stmt->varName.c_str());
    llvm::Value* endV = generateExpr(stmt->end.get());
    llvm::Value* condV = builder->CreateFCmpOLE(currV, endV, "fortmp");
    builder->CreateCondBr(condV, bodyBB, afterBB);

    // 4. Body
    func->insert(func->end(), bodyBB);
    builder->SetInsertPoint(bodyBB);
    generateStmt(stmt->body.get());

    // 5. Update (i = i + step)
    llvm::Value* stepV = stmt->step ? generateExpr(stmt->step.get()) : llvm::ConstantFP::get(*context, llvm::APFloat(1.0));
    llvm::Value* nextV = builder->CreateFAdd(currV, stepV, "nextvar");
    builder->CreateStore(nextV, alloca);

    builder->CreateBr(condBB);

    // 6. After
    func->insert(func->end(), afterBB);
    builder->SetInsertPoint(afterBB);
}

llvm::Value* CodeGen::visitNumberExpr(const NumberExpr* expr) {
    return llvm::ConstantFP::get(*context, llvm::APFloat(expr->value));
}

llvm::Value* CodeGen::visitBinaryExpr(const BinaryExpr* expr) {
    llvm::Value* L = generateExpr(expr->left.get());
    llvm::Value* R = generateExpr(expr->right.get());
    if (!L || !R) return nullptr;

    switch (expr->op) {
        case TokenType::Plus:  return builder->CreateFAdd(L, R, "addtmp");
        case TokenType::Minus: return builder->CreateFSub(L, R, "subtmp");
        case TokenType::Star:  return builder->CreateFMul(L, R, "multmp");
        case TokenType::Slash: return builder->CreateFDiv(L, R, "divtmp"); 
        
        // Comparisons
        case TokenType::EqualEqual: {
            L = builder->CreateFCmpOEQ(L, R, "eqtmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        }
        case TokenType::BangEqual: {
            L = builder->CreateFCmpONE(L, R, "netmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        }
        case TokenType::Less: {
            L = builder->CreateFCmpOLT(L, R, "lttmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        }
        case TokenType::LessEqual: {
            L = builder->CreateFCmpOLE(L, R, "letmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        }
        case TokenType::Greater: {
            L = builder->CreateFCmpOGT(L, R, "gttmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        }
        case TokenType::GreaterEqual: {
            L = builder->CreateFCmpOGE(L, R, "getmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        }

        // TODO: Logical And/Or with short-circuiting

        default: 
            std::cerr << "Unimplemented binary operator: " << tokenTypeToString(expr->op) << "\n";
            return nullptr;
    }
}

llvm::Value* CodeGen::visitVariableExpr(const VariableExpr* expr) {
    llvm::Value* V = namedValues[expr->name];
    if (!V) {
        std::cerr << "Unknown variable name: " << expr->name << "\n";
        return nullptr;
    }
    // Load the value from the alloca
    return builder->CreateLoad(llvm::Type::getDoubleTy(*context), V, expr->name.c_str());
}

llvm::Value* CodeGen::visitAssignExpr(const AssignExpr* expr) {
    // Assuming target is a VariableExpr for now
    auto* var = dynamic_cast<VariableExpr*>(expr->target.get());
    if (!var) {
        std::cerr << "Invalid assignment target\n";
        return nullptr;
    }

    llvm::Value* val = generateExpr(expr->value.get());
    if (!val) return nullptr;

    llvm::Value* V = namedValues[var->name];
    if (!V) {
        std::cerr << "Unknown variable name: " << var->name << "\n";
        return nullptr;
    }

    builder->CreateStore(val, V);
    return val;
}

llvm::Value* CodeGen::visitCallExpr(const CallExpr* expr) {
    // Assuming callee is a VariableExpr (function name)
    auto* var = dynamic_cast<VariableExpr*>(expr->callee.get());
    if (!var) {
        std::cerr << "Only simple function calls are supported now\n";
        return nullptr;
    }

    llvm::Function* func = module->getFunction(var->name);
    if (!func) {
        std::cerr << "Unknown function: " << var->name << "\n";
        return nullptr;
    }

    if (func->arg_size() != expr->args.size()) {
        std::cerr << "Incorrect number of arguments for " << var->name << "\n";
        return nullptr;
    }

    std::vector<llvm::Value*> argsV;
    for (const auto& arg : expr->args) {
        argsV.push_back(generateExpr(arg.get()));
        if (!argsV.back()) return nullptr;
    }

    return builder->CreateCall(func, argsV, "calltmp");
}

void CodeGen::visitVarDeclStmt(const VarDeclStmt* stmt) {
    llvm::Function* func = builder->GetInsertBlock()->getParent();
    
    // Create an alloca for the variable in the entry block of the current function
    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, stmt->name);
    
    namedValues[stmt->name] = alloca;

    if (stmt->initializer) {
        llvm::Value* initVal = generateExpr(stmt->initializer.get());
        if (initVal) {
            builder->CreateStore(initVal, alloca);
        }
    }
}

void CodeGen::visitReturnStmt(const ReturnStmt* stmt) {
    if (stmt->value) {
        llvm::Value* val = generateExpr(stmt->value.get());
        // For now, our 'main' returns i32, but our expressions are double.
        // We might need to cast or change main's return type.
        // Let's assume double return for now for consistency if needed, 
        // or just cast to i32 for main's sake.
        if (val) {
            llvm::Value* retVal = builder->CreateFPToSI(val, builder->getInt32Ty());
            builder->CreateRet(retVal);
        }
    } else {
        builder->CreateRet(builder->getInt32(0));
    }
}

void CodeGen::visitBlockStmt(const BlockStmt* stmt) {
    for (const auto& s : stmt->statements) {
        generateStmt(s.get());
    }
}

void CodeGen::visitFunctionStmt(const FunctionStmt* stmt) {
    std::vector<llvm::Type*> doubles(stmt->params.size(), llvm::Type::getDoubleTy(*context));
    llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getDoubleTy(*context), doubles, false);
    llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, stmt->name, module.get());
    
    llvm::BasicBlock* entryBuf = llvm::BasicBlock::Create(*context, "entry", func);
    auto* oldBB = builder->GetInsertBlock();
    builder->SetInsertPoint(entryBuf);
    
    auto oldNamedValues = namedValues; 
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        std::string argName = stmt->params[idx++];
        arg.setName(argName);
        llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
        llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, argName);
        builder->CreateStore(&arg, alloca);
        namedValues[argName] = alloca;
    }
    
    generateStmt(stmt->body.get());
    
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }
    
    if (llvm::verifyFunction(*func, &llvm::errs())) {
        std::cerr << "Error: Function " << stmt->name << " verification failed!\n";
    }
    
    namedValues = oldNamedValues;
    if (oldBB) builder->SetInsertPoint(oldBB);
}

} // namespace manifast
