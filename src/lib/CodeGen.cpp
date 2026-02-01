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
    initializeTypes();
}

void CodeGen::initializeTypes() {
    // struct Any {
    //   i32 type;      // 0=Number, 1=Array, 2=Object
    //   double number; // Optimization for numbers
    //   i8* ptr;       // Pointer to heap data (Array/Object)
    // }
    std::vector<llvm::Type*> anyFields = {
        llvm::Type::getInt32Ty(*context),
        llvm::Type::getDoubleTy(*context),
        llvm::PointerType::getUnqual(*context)
    };
    anyType = llvm::StructType::create(*context, anyFields, "Any");
    
    // Define other types later as needed (Array/Object internals)
}

llvm::Value* CodeGen::createNumber(double value) {
    // Create struct instance on stack (simplest for now)
    llvm::Value* alloc = builder->CreateAlloca(anyType, nullptr, "num_alloc");
    
    // Set type = 0
    llvm::Value* typePtr = builder->CreateStructGEP(anyType, alloc, 0);
    builder->CreateStore(builder->getInt32(0), typePtr);
    
    // Set number = value
    llvm::Value* numPtr = builder->CreateStructGEP(anyType, alloc, 1);
    builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(value)), numPtr);
    
    // Set ptr = null
    llvm::Value* dataPtr = builder->CreateStructGEP(anyType, alloc, 2);
    builder->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)), dataPtr);
    
    return alloc; 
}

llvm::Value* CodeGen::boxDouble(llvm::Value* v) {
    llvm::Value* alloc = builder->CreateAlloca(anyType, nullptr, "num_box");
    // Type 0
    builder->CreateStore(builder->getInt32(0), builder->CreateStructGEP(anyType, alloc, 0));
    // Number
    builder->CreateStore(v, builder->CreateStructGEP(anyType, alloc, 1));
    // Ptr null
    builder->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)), builder->CreateStructGEP(anyType, alloc, 2));
    return alloc;
}

llvm::Value* CodeGen::unboxNumber(llvm::Value* anyPtr) {
    // Assume it's a number for now (skip type check for MVP speed)
    llvm::Value* numPtr = builder->CreateStructGEP(anyType, anyPtr, 1);
    return builder->CreateLoad(llvm::Type::getDoubleTy(*context), numPtr, "unbox");
}

llvm::Value* CodeGen::createArray(const std::vector<llvm::Value*>& elements) {
    llvm::Value* alloc = builder->CreateAlloca(anyType, nullptr, "arr_alloc");
    // Type 1 = Array
    builder->CreateStore(builder->getInt32(1), builder->CreateStructGEP(anyType, alloc, 0));
    // Number = 0
    builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), builder->CreateStructGEP(anyType, alloc, 1));
    // Ptr = TODO (malloc)
    builder->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)), builder->CreateStructGEP(anyType, alloc, 2));
    return alloc;
}

llvm::Value* CodeGen::createObject(const std::vector<std::pair<std::string, llvm::Value*>>& pairs) {
    llvm::Value* alloc = builder->CreateAlloca(anyType, nullptr, "obj_alloc");
    // Type 2 = Object
    builder->CreateStore(builder->getInt32(2), builder->CreateStructGEP(anyType, alloc, 0));
    // Number = 0
    builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), builder->CreateStructGEP(anyType, alloc, 1));
    // Ptr = TODO (malloc)
    builder->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)), builder->CreateStructGEP(anyType, alloc, 2));
    return alloc;
}

void CodeGen::printAny(llvm::Value* anyVal) {
    // Stub
}

void CodeGen::compile(const std::vector<std::unique_ptr<Stmt>>& statements) {
    // Create a main function to hold top-level statements
    // Main returns Any struct
    llvm::FunctionType* funcType = llvm::FunctionType::get(anyType, false);
    llvm::Function* mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entry);

    for (const auto& stmt : statements) {
        generateStmt(stmt.get());
    }

    // Default return 0 (Boxed)
    llvm::Value* retVal = boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    // Load structure to return
    llvm::Value* retLoad = builder->CreateLoad(anyType, retVal);
    builder->CreateRet(retLoad);
    
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

    // Expected<ExecutorAddr> or Expected<ExecutorSymbolDef>
    // We try to get the address as a uint64_t for the raw cast.
    uint64_t rawAddr = 0;
    if constexpr (std::is_same_v<std::remove_reference_t<decltype(*SymOrErr)>, llvm::orc::ExecutorAddr>) {
        rawAddr = SymOrErr->getValue();
    } else {
        rawAddr = SymOrErr->getAddress().getValue();
    }
    
    // C++ Definition of Any struct to match LLVM IR
    struct Any {
        int32_t type;
        double number;
        void* ptr;
    };
    
    typedef Any (*MainFuncPtr)();
    MainFuncPtr MainPtr = (MainFuncPtr)rawAddr;
    
    Any result = MainPtr();
    std::cout << "--- Execution Result ---\n";
    if (result.type == 0) {
        std::cout << "Return: " << result.number << "\n";
    } else {
        std::cout << "Return type: " << result.type << "\n";
    }
}

llvm::Value* CodeGen::generateExpr(const Expr* expr) {
    if (auto* num = dynamic_cast<const NumberExpr*>(expr)) return visitNumberExpr(num);
    if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) return visitBinaryExpr(bin);
    if (auto* var = dynamic_cast<const VariableExpr*>(expr)) return visitVariableExpr(var);
    if (auto* assign = dynamic_cast<const AssignExpr*>(expr)) return visitAssignExpr(assign);
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) return visitCallExpr(call);
    if (auto* arr = dynamic_cast<const ArrayExpr*>(expr)) return visitArrayExpr(arr);
    if (auto* obj = dynamic_cast<const ObjectExpr*>(expr)) return visitObjectExpr(obj);
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
    } else if (auto* tryStmt = dynamic_cast<const TryStmt*>(stmt)) {
        visitTryStmt(tryStmt);
    } else if (auto* funcStmt = dynamic_cast<const FunctionStmt*>(stmt)) {
        visitFunctionStmt(funcStmt);
    }
}

void CodeGen::visitIfStmt(const IfStmt* stmt) {
    llvm::Value* condV = generateExpr(stmt->condition.get());
    if (!condV) return;

    // Unbox condition (Any* -> double)
    llvm::Value* unpacked = unboxNumber(condV);
    // Convert condition to bool (double != 0)
    unpacked = builder->CreateFCmpONE(unpacked, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "ifcond");

    llvm::Function* func = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "then", func);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ifcont");

    if (stmt->elseBranch) {
        builder->CreateCondBr(unpacked, thenBB, elseBB);
    } else {
        builder->CreateCondBr(unpacked, thenBB, mergeBB);
    }

    // Emit then block
    builder->SetInsertPoint(thenBB);
    generateStmt(stmt->thenBranch.get());
    builder->CreateBr(mergeBB);
    thenBB = builder->GetInsertBlock();

    // Emit else block
    if (stmt->elseBranch) {
        elseBB->insertInto(func);
        builder->SetInsertPoint(elseBB);
        generateStmt(stmt->elseBranch.get());
        builder->CreateBr(mergeBB);
        elseBB = builder->GetInsertBlock();
    }

    // Emit merge block
    mergeBB->insertInto(func);
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
    
    llvm::Value* unpacked = unboxNumber(condV);
    unpacked = builder->CreateFCmpONE(unpacked, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "whilecond");
    builder->CreateCondBr(unpacked, bodyBB, afterBB);

    bodyBB->insertInto(func);
    builder->SetInsertPoint(bodyBB);
    generateStmt(stmt->body.get());
    builder->CreateBr(condBB);

    afterBB->insertInto(func);
    builder->SetInsertPoint(afterBB);
}

void CodeGen::visitForStmt(const ForStmt* stmt) {
    llvm::Function* func = builder->GetInsertBlock()->getParent();

    // 1. Initializer
    llvm::Value* startV = generateExpr(stmt->start.get()); // Any*
    if (!startV) return;

    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->varName);
    
    // Copy startV to alloca
    llvm::Value* startLoaded = builder->CreateLoad(anyType, startV);
    builder->CreateStore(startLoaded, alloca);
    
    namedValues[stmt->varName] = alloca;

    // 2. Blocks
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context, "forcond", func);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context, "forbody");
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "afterfor");

    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);

    // 3. Condition (i <= end)
    llvm::Value* currV = builder->CreateLoad(anyType, alloca, stmt->varName.c_str());
    llvm::Value* currValPtr = builder->CreateAlloca(anyType); // Temp ptr for unbox
    builder->CreateStore(currV, currValPtr);
    
    llvm::Value* currDouble = unboxNumber(currValPtr);
    
    llvm::Value* endV = generateExpr(stmt->end.get());
    llvm::Value* endDouble = unboxNumber(endV);
    
    llvm::Value* condV = builder->CreateFCmpOLE(currDouble, endDouble, "fortmp");
    builder->CreateCondBr(condV, bodyBB, afterBB);

    // 4. Body
    bodyBB->insertInto(func);
    builder->SetInsertPoint(bodyBB);
    generateStmt(stmt->body.get());

    // 5. Update (i = i + step)
    llvm::Value* stepV = stmt->step ? generateExpr(stmt->step.get()) : boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(1.0)));
    llvm::Value* stepDouble = unboxNumber(stepV);
    
    // currDouble is potentially stale, should reload? No, we are in correct block order?
    // Actually we need to reload i inside the loop/latch if it was modified?
    // Simplified: i = i + step.
    // Reload currVal
    llvm::Value* loadedCurr = builder->CreateLoad(anyType, alloca);
    builder->CreateStore(loadedCurr, currValPtr); // Reuse temp
    llvm::Value* valNow = unboxNumber(currValPtr);
    
    llvm::Value* nextDouble = builder->CreateFAdd(valNow, stepDouble, "nextvar");
    llvm::Value* nextAny = boxDouble(nextDouble); // Returns Any* (temp)
    
    llvm::Value* nextLoaded = builder->CreateLoad(anyType, nextAny);
    builder->CreateStore(nextLoaded, alloca);

    builder->CreateBr(condBB);

    // 6. After
    afterBB->insertInto(func);
    builder->SetInsertPoint(afterBB);
}

llvm::Value* CodeGen::visitNumberExpr(const NumberExpr* expr) {
    return createNumber(expr->value);
}

llvm::Value* CodeGen::visitBinaryExpr(const BinaryExpr* expr) {
    llvm::Value* L = generateExpr(expr->left.get());
    llvm::Value* R = generateExpr(expr->right.get());
    if (!L || !R) return nullptr;

    // Unbox inputs (assuming they are Any*)
    llvm::Value* LVal = unboxNumber(L);
    llvm::Value* RVal = unboxNumber(R);

    // Compute result (double or bool)
    llvm::Value* res = nullptr;
    switch (expr->op) {
        case TokenType::Plus:  res = builder->CreateFAdd(LVal, RVal, "addtmp"); break;
        case TokenType::Minus: res = builder->CreateFSub(LVal, RVal, "subtmp"); break;
        case TokenType::Star:  res = builder->CreateFMul(LVal, RVal, "multmp"); break;
        case TokenType::Slash: res = builder->CreateFDiv(LVal, RVal, "divtmp"); break;
        
        // Comparisons
        case TokenType::EqualEqual: {
            res = builder->CreateFCmpOEQ(LVal, RVal, "eqtmp");
            res = builder->CreateUIToFP(res, llvm::Type::getDoubleTy(*context), "booltmp");
            break;
        }
        case TokenType::BangEqual: {
            res = builder->CreateFCmpONE(LVal, RVal, "netmp");
            res = builder->CreateUIToFP(res, llvm::Type::getDoubleTy(*context), "booltmp");
            break;
        }
        case TokenType::Less: {
            res = builder->CreateFCmpOLT(LVal, RVal, "lttmp");
            res = builder->CreateUIToFP(res, llvm::Type::getDoubleTy(*context), "booltmp");
            break;
        }
        case TokenType::LessEqual: {
            res = builder->CreateFCmpOLE(LVal, RVal, "letmp");
            res = builder->CreateUIToFP(res, llvm::Type::getDoubleTy(*context), "booltmp");
            break;
        }
        case TokenType::Greater: {
            res = builder->CreateFCmpOGT(LVal, RVal, "gttmp");
            res = builder->CreateUIToFP(res, llvm::Type::getDoubleTy(*context), "booltmp");
            break;
        }
        case TokenType::GreaterEqual: {
            res = builder->CreateFCmpOGE(LVal, RVal, "getmp");
            res = builder->CreateUIToFP(res, llvm::Type::getDoubleTy(*context), "booltmp");
            break;
        }
        
        default: 
            std::cerr << "Unimplemented binary operator: " << tokenTypeToString(expr->op) << "\n";
            return nullptr;
    }
    
    // Box result
    return boxDouble(res);
}

llvm::Value* CodeGen::visitVariableExpr(const VariableExpr* expr) {
    llvm::Value* V = namedValues[expr->name];
    if (!V) {
        std::cerr << "Unknown variable name: " << expr->name << "\n";
        return nullptr;
    }
    // V is pointer to the variable's storage (Any).
    // Return a pointer to a COPY of the value (pass by value semantics for now).
    // Or just return V? If we return V, modifications to the return value would modify the variable.
    // In many expressions (like binary ops), we just Read.
    // But if we pass it to a function etc...
    // Let's return a copy to be safe and consistent with createNumber.
    
    llvm::Value* temp = builder->CreateAlloca(anyType, nullptr, "var_read");
    llvm::Value* val = builder->CreateLoad(anyType, V, expr->name.c_str());
    builder->CreateStore(val, temp);
    return temp;
}

llvm::Value* CodeGen::visitAssignExpr(const AssignExpr* expr) {
    auto* var = dynamic_cast<VariableExpr*>(expr->target.get());
    if (!var) {
        std::cerr << "Invalid assignment target\n";
        return nullptr;
    }

    llvm::Value* val = generateExpr(expr->value.get()); // Returns Any* (temp)
    if (!val) return nullptr;

    llvm::Value* V = namedValues[var->name];
    if (!V) {
        std::cerr << "Unknown variable name: " << var->name << "\n";
        return nullptr;
    }

    // Copy content from val (temp) to V (var storage)
    llvm::Value* loadedVal = builder->CreateLoad(anyType, val);
    builder->CreateStore(loadedVal, V);
    
    return val;
}

llvm::Value* CodeGen::visitCallExpr(const CallExpr* expr) {
    llvm::Function* func = module->getFunction(expr->callee);
    if (!func) {
        std::cerr << "Unknown function: " << expr->callee << "\n";
        return nullptr;
    }

    if (func->arg_size() != expr->arguments.size()) {
        std::cerr << "Incorrect number of arguments for " << expr->callee << "\n";
        return nullptr;
    }

    std::vector<llvm::Value*> argsV;
    for (const auto& arg : expr->arguments) {
        llvm::Value* argVal = generateExpr(arg.get()); // Any* (temp)
        if (!argVal) return nullptr;
        // Load Any struct to pass by value
        argsV.push_back(builder->CreateLoad(anyType, argVal));
    }

    // Call returns Any struct
    llvm::Value* retStruct = builder->CreateCall(func, argsV, "calltmp");
    
    // Store to temp alloca to return Any*
    llvm::Value* temp = builder->CreateAlloca(anyType, nullptr, "call_res");
    builder->CreateStore(retStruct, temp);
    return temp;
}

void CodeGen::visitVarDeclStmt(const VarDeclStmt* stmt) {
    llvm::Function* func = builder->GetInsertBlock()->getParent();
    
    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
    // Allocate Any struct
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->name);
    
    namedValues[stmt->name] = alloca;

    if (stmt->initializer) {
        llvm::Value* initVal = generateExpr(stmt->initializer.get()); // Any*
        if (initVal) {
            // Copy initVal to alloca
            llvm::Value* loaded = builder->CreateLoad(anyType, initVal);
            builder->CreateStore(loaded, alloca);
        }
    }
}

void CodeGen::visitReturnStmt(const ReturnStmt* stmt) {
    if (stmt->value) {
        llvm::Value* val = generateExpr(stmt->value.get()); // Any*
        if (val) {
            // Load Any struct to return
            llvm::Value* retVal = builder->CreateLoad(anyType, val);
            builder->CreateRet(retVal);
        }
    } else {
        // Return void/null? We must return Any. Return 0 for now.
         llvm::Value* retVal = boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
         llvm::Value* retLoad = builder->CreateLoad(anyType, retVal);
         builder->CreateRet(retLoad);
    }
}

void CodeGen::visitBlockStmt(const BlockStmt* stmt) {
    for (const auto& s : stmt->statements) {
        generateStmt(s.get());
    }
}

void CodeGen::visitFunctionStmt(const FunctionStmt* stmt) {
    // Return type Any, Args Any
    std::vector<llvm::Type*> argTypes(stmt->params.size(), anyType);
    llvm::FunctionType* ft = llvm::FunctionType::get(anyType, argTypes, false);
    llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, stmt->name, module.get());

    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context, "entry", func);
    
    llvm::BasicBlock* oldBB = builder->GetInsertBlock();
    builder->SetInsertPoint(bb);

    std::map<std::string, llvm::AllocaInst*> oldNamedValues = namedValues;
    namedValues.clear();

    unsigned idx = 0;
    for (auto& arg : func->args()) {
        // Create alloca for arg
        llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
        llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->params[idx]);
        
        // Store passed value (Any struct) into alloca
        builder->CreateStore(&arg, alloca);
        
        namedValues[stmt->params[idx]] = alloca;
        idx++;
    }

    // Body
    if (auto* bodyBlock = dynamic_cast<const BlockStmt*>(stmt->body.get())) {
        for (const auto& s : bodyBlock->statements) {
            generateStmt(s.get());
        }
    } else {
        generateStmt(stmt->body.get());
    }
    
    // Default return 0 if no return stmt
    if (!func->back().getTerminator()) {
         llvm::Value* retVal = boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
         llvm::Value* retLoad = builder->CreateLoad(anyType, retVal);
         builder->CreateRet(retLoad);
    }
    
    if (llvm::verifyFunction(*func, &llvm::errs())) {
        std::cerr << "Error verifying function " << stmt->name << "\n";
    }
    
    namedValues = oldNamedValues;
    if (oldBB) builder->SetInsertPoint(oldBB);
}

llvm::Value* CodeGen::visitArrayExpr(const ArrayExpr* expr) {
    std::vector<llvm::Value*> elements;
    for (const auto& el : expr->elements) {
        llvm::Value* val = generateExpr(el.get());
        if (val) {
            // Load value to store in array (by value)
             llvm::Value* loaded = builder->CreateLoad(anyType, val);
             elements.push_back(loaded);
        } else {
             // Handle null/error? Push a null-Any?
             // For now, skip or push zero.
        }
    }
    return createArray(elements);
}

llvm::Value* CodeGen::visitObjectExpr(const ObjectExpr* expr) {
    std::vector<std::pair<std::string, llvm::Value*>> pairs;
    for (const auto& kv : expr->entries) {
         llvm::Value* val = generateExpr(kv.second.get());
         if (val) {
             llvm::Value* loaded = builder->CreateLoad(anyType, val);
             pairs.push_back({kv.first, loaded});
         }
    }
    return createObject(pairs);
}

void CodeGen::visitTryStmt(const TryStmt* stmt) {
    // Simple implementation: just generate the try body.
    if (stmt->tryBody) {
        generateStmt(stmt->tryBody.get());
    }
}

} // namespace manifast
