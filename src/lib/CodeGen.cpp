#include "manifast/CodeGen.h"
#include "manifast/Runtime.h"
#include <llvm/IR/Verifier.h>
#include <iostream>

#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>

namespace manifast {

CodeGen::CodeGen() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("ManifastModule", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    initializeTypes();
    pushScope(); // Root scope

    // Register Built-ins
    llvm::Type* anyPtrTy = builder->getPtrTy();
    
    // print(Any*) -> void
    llvm::FunctionType* printFT = llvm::FunctionType::get(builder->getVoidTy(), {anyPtrTy}, false);
    llvm::Function::Create(printFT, llvm::Function::ExternalLinkage, "manifast_print_any", module.get());
    
    // println(Any*) -> void
    llvm::Function::Create(printFT, llvm::Function::ExternalLinkage, "manifast_println_any", module.get());

    // printfmt(Any*, Any*) -> void
    llvm::FunctionType* printfmtFT = llvm::FunctionType::get(builder->getVoidTy(), {anyPtrTy, anyPtrTy}, false);
    llvm::Function::Create(printfmtFT, llvm::Function::ExternalLinkage, "manifast_printfmt", module.get());

    // input() -> Any*
    llvm::FunctionType* inputFT = llvm::FunctionType::get(anyPtrTy, {}, false);
    llvm::Function::Create(inputFT, llvm::Function::ExternalLinkage, "manifast_input", module.get());

    // assert(Any*, Any*) -> void
    llvm::FunctionType* assertFT = llvm::FunctionType::get(builder->getVoidTy(), {anyPtrTy, anyPtrTy}, false);
    llvm::Function::Create(assertFT, llvm::Function::ExternalLinkage, "manifast_assert", module.get());

    // manifast_array_len(Any*) -> double
    llvm::FunctionType* lenFT = llvm::FunctionType::get(builder->getDoubleTy(), {anyPtrTy}, false);
    llvm::Function::Create(lenFT, llvm::Function::ExternalLinkage, "manifast_array_len", module.get());

    // manifast_array_push(Any*, Any*) -> void
    llvm::FunctionType* pushFT = llvm::FunctionType::get(builder->getVoidTy(), {anyPtrTy, anyPtrTy}, false);
    llvm::Function::Create(pushFT, llvm::Function::ExternalLinkage, "manifast_array_push", module.get());

    // manifast_array_pop(Any*) -> Any*
    llvm::FunctionType* popFT = llvm::FunctionType::get(anyPtrTy, {anyPtrTy}, false);
    llvm::Function::Create(popFT, llvm::Function::ExternalLinkage, "manifast_array_pop", module.get());

    // manifast_impor(const char*) -> Any*
    llvm::FunctionType* imporFT = llvm::FunctionType::get(anyPtrTy, {builder->getPtrTy()}, false);
    llvm::Function::Create(imporFT, llvm::Function::ExternalLinkage, "manifast_impor", module.get());

    // manifast_call_dynamic(Any*, Any*, int) -> Any*
    llvm::FunctionType* callDynFT = llvm::FunctionType::get(anyPtrTy, {anyPtrTy, anyPtrTy, builder->getInt32Ty()}, false);
    llvm::Function::Create(callDynFT, llvm::Function::ExternalLinkage, "manifast_call_dynamic", module.get());
}

void CodeGen::pushScope() {
    scopes.push_back({});
}

void CodeGen::popScope() {
    if (scopes.size() > 1) {
        scopes.pop_back();
    }
}

llvm::Value* CodeGen::lookupVariable(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return (*it)[name];
        }
    }
    return nullptr;
}

void CodeGen::initializeTypes() {
    // struct Any { i32, double, i8* }
    anyType = llvm::StructType::create(*context, "Any");
    anyType->setBody({
        builder->getInt32Ty(),
        llvm::Type::getDoubleTy(*context),
        llvm::PointerType::getUnqual(*context)
    });
}

llvm::Value* CodeGen::createNumber(double value) {
    llvm::Function* func = module->getFunction("manifast_create_number");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {llvm::Type::getDoubleTy(*context)}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_number", module.get());
    }
    return builder->CreateCall(func, {llvm::ConstantFP::get(*context, llvm::APFloat(value))}, "num");
}

llvm::Value* CodeGen::boxDouble(llvm::Value* v) {
    llvm::Function* func = module->getFunction("manifast_create_number");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {llvm::Type::getDoubleTy(*context)}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_number", module.get());
    }
    return builder->CreateCall(func, {v}, "num_box");
}

llvm::Value* CodeGen::createString(const std::string& value) {
    llvm::Function* func = module->getFunction("manifast_create_string");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {builder->getPtrTy()}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_string", module.get());
    }
    llvm::Value* globalStr = builder->CreateGlobalString(value);
    return builder->CreateCall(func, {globalStr}, "str");
}

llvm::Value* CodeGen::unboxString(llvm::Value* anyPtr) {
    llvm::Value* ptrPtr = builder->CreateStructGEP(anyType, anyPtr, 2);
    return builder->CreateLoad(builder->getPtrTy(), ptrPtr, "unbox_str");
}

llvm::Value* CodeGen::unboxNumber(llvm::Value* anyPtr) {
    // Assume it's a number for now (skip type check for MVP speed)
    // anyPtr is Any*
    llvm::Value* numPtr = builder->CreateStructGEP(anyType, anyPtr, 1);
    return builder->CreateLoad(llvm::Type::getDoubleTy(*context), numPtr, "unbox");
}

llvm::Value* CodeGen::createArray(const std::vector<llvm::Value*>& elements) {
    llvm::Function* func = module->getFunction("manifast_create_array");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {builder->getInt32Ty()}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_array", module.get());
    }
    
    llvm::Value* arrVal = builder->CreateCall(func, {builder->getInt32(elements.size())}, "arr");
    
    // Set elements
    llvm::Function* setFunc = module->getFunction("manifast_array_set");
    if (!setFunc) {
        llvm::FunctionType* sft = llvm::FunctionType::get(builder->getVoidTy(), 
            {llvm::PointerType::getUnqual(*context), llvm::Type::getDoubleTy(*context), llvm::PointerType::getUnqual(*context)}, false);
        setFunc = llvm::Function::Create(sft, llvm::Function::ExternalLinkage, "manifast_array_set", module.get());
    }
    
    for (uint32_t i = 0; i < elements.size(); ++i) {
        // elements[i] is Any (loaded from temp)
        // We need to pass Any* to manifast_array_set
        llvm::Value* elemPtr = builder->CreateAlloca(anyType);
        builder->CreateStore(elements[i], elemPtr);
        builder->CreateCall(setFunc, {arrVal, llvm::ConstantFP::get(*context, llvm::APFloat((double)(i + 1))), elemPtr});
    }
    
    return arrVal;
}

llvm::Value* CodeGen::createObject(const std::vector<std::pair<std::string, llvm::Value*>>& pairs) {
    llvm::Function* func = module->getFunction("manifast_create_object");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_object", module.get());
    }
    
    llvm::Value* objVal = builder->CreateCall(func, {}, "obj");
    
    // Set properties
    llvm::Function* setFunc = module->getFunction("manifast_object_set");
    if (!setFunc) {
        llvm::FunctionType* sft = llvm::FunctionType::get(builder->getVoidTy(), 
            {llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, false);
        setFunc = llvm::Function::Create(sft, llvm::Function::ExternalLinkage, "manifast_object_set", module.get());
    }
    
    for (const auto& pair : pairs) {
        // pair.second is Any (loaded)
        llvm::Value* valPtr = builder->CreateAlloca(anyType);
        builder->CreateStore(pair.second, valPtr);
        
        llvm::Value* keyStr = builder->CreateGlobalString(pair.first);
        builder->CreateCall(setFunc, {objVal, keyStr, valPtr});
    }
    
    return objVal;
}

void CodeGen::printAny(llvm::Value* anyVal) {
    llvm::Function* func = module->getFunction("manifast_print_any");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_print_any", module.get());
    }
    builder->CreateCall(func, {anyVal});
}

void CodeGen::compile(const std::vector<std::unique_ptr<Stmt>>& statements) {
    // Create a main function to hold top-level statements
    // Main returns Any* (pointer to Any)
    llvm::FunctionType* funcType = llvm::FunctionType::get(builder->getPtrTy(), false);
    llvm::Function* mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "manifast_main", module.get());
    mainFunc->addFnAttr("stack-probe-size", "1048576"); 
    mainFunc->addFnAttr("no-stack-arg-probe"); // Ensure no chkstk on MinGW

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entry);

    for (const auto& stmt : statements) {
        if (builder->GetInsertBlock()->getTerminator()) break;
        generateStmt(stmt.get());
    }

    // Default return 0 (Boxed Any*) if no terminator
    if (!builder->GetInsertBlock()->getTerminator()) {
        llvm::Value* retVal = boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
        builder->CreateRet(retVal);
    }
    
    // Verify
    if (llvm::verifyFunction(*mainFunc, &llvm::errs())) {
        std::cerr << "Error: Function verification failed (manifast_main)!\n";
    }
}

void CodeGen::addMainEntry() {
    llvm::Function* mainFunc = module->getFunction("manifast_main");
    if (!mainFunc) return;

    // Add standard 'main' entry point for AOT (calls manifast_main)
    llvm::FunctionType* stdMainFT = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {}, false);
    llvm::Function* stdMainFunc = llvm::Function::Create(stdMainFT, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* stdMainEntry = llvm::BasicBlock::Create(*context, "entry", stdMainFunc);
    
    llvm::IRBuilder<> mainBuilder(stdMainEntry);
    mainBuilder.CreateCall(mainFunc, {});
    mainBuilder.CreateRet(mainBuilder.getInt32(0));
}

void CodeGen::printIR() {
    module->print(llvm::errs(), nullptr);
}

void CodeGen::emitIR(const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Could not open file: " << ec.message() << std::endl;
        return;
    }
    module->print(dest, nullptr);
}

void CodeGen::emitAssembly(const std::string& path) {
    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

    if (!target) {
        std::cerr << error << std::endl;
        return;
    }

    auto cpu = "generic";
    auto features = "";

    llvm::TargetOptions opt;
    auto targetMachine = target->createTargetMachine(targetTriple, cpu, features, opt, llvm::Reloc::PIC_);

    module->setDataLayout(targetMachine->createDataLayout());
    module->setTargetTriple(targetTriple);

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);

    if (ec) {
        std::cerr << "Could not open file: " << ec.message() << std::endl;
        return;
    }

    llvm::legacy::PassManager pass;
    auto fileType = llvm::CodeGenFileType::AssemblyFile;

    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
        return;
    }

    pass.run(*module);
    dest.flush();
}

void CodeGen::emitObject(const std::string& path) {
    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

    if (!target) {
        std::cerr << error << std::endl;
        return;
    }

    auto cpu = "generic";
    auto features = "";

    llvm::TargetOptions opt;
    auto targetMachine = target->createTargetMachine(targetTriple, cpu, features, opt, llvm::Reloc::PIC_);

    module->setDataLayout(targetMachine->createDataLayout());
    module->setTargetTriple(targetTriple);

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);

    if (ec) {
        std::cerr << "Could not open file: " << ec.message() << std::endl;
        return;
    }

    llvm::legacy::PassManager pass;
    auto fileType = llvm::CodeGenFileType::ObjectFile;

    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
        return;
    }

    pass.run(*module);
    dest.flush();
}

// Static initialization - runs once per process
static struct LLVMInit {
    LLVMInit() {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
    }
} llvmInit;

bool CodeGen::run() {
    auto jit = llvm::ExitOnError()(llvm::orc::LLJITBuilder().create());
    
    // Add library search for host symbols
    jit->getMainJITDylib().addGenerator(
        llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix())));

    // Register runtime symbols
    auto& ES = jit->getExecutionSession();
    auto& JD = jit->getMainJITDylib();
    auto Mangle = llvm::orc::MangleAndInterner(ES, jit->getDataLayout());

    llvm::orc::SymbolMap RuntimeSymbols;
    
    #define REGISTER_SYM(name) \
        RuntimeSymbols[Mangle(#name)] = { llvm::orc::ExecutorAddr::fromPtr((void*)&::name), llvm::JITSymbolFlags::Exported }

    REGISTER_SYM(manifast_create_number);
    REGISTER_SYM(manifast_create_string);
    REGISTER_SYM(manifast_create_boolean);
    REGISTER_SYM(manifast_create_nil);
    REGISTER_SYM(manifast_create_array);
    REGISTER_SYM(manifast_create_object);
    REGISTER_SYM(manifast_object_set);
    REGISTER_SYM(manifast_object_get);
    REGISTER_SYM(manifast_array_set);
    REGISTER_SYM(manifast_array_get);
    REGISTER_SYM(manifast_print_any);
    REGISTER_SYM(manifast_println_any);
    REGISTER_SYM(manifast_printfmt);
    REGISTER_SYM(manifast_input);
    REGISTER_SYM(manifast_assert);
    REGISTER_SYM(manifast_create_class);
    REGISTER_SYM(manifast_create_instance);

    if (auto Err = JD.define(llvm::orc::absoluteSymbols(std::move(RuntimeSymbols)))) {
        std::cerr << "Error defining runtime symbols: " << llvm::toString(std::move(Err)) << "\n";
    }

    // Verify module
    if (llvm::verifyModule(*module, &llvm::errs())) {
        std::cerr << "Error: Module verification failed!\n";
        return false;
    }

    // Set data layout and triple
    module->setDataLayout(jit->getDataLayout());
    module->setTargetTriple(jit->getTargetTriple().getTriple());

    auto err = jit->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(context)));
    if (err) {
        std::cerr << "Error adding IR module: " << llvm::toString(std::move(err)) << "\n";
        return false;
    }

    // Look up manifast_main
    auto SymOrErr = jit->lookup("manifast_main");

    if (!SymOrErr) {
        std::cerr << "Error: manifast_main function not found in JIT. Reason: " 
                  << llvm::toString(SymOrErr.takeError()) << "\n";
        return false;
    }

    // Expected<ExecutorAddr> in LLVM 18
    auto rawAddr = (*SymOrErr).getValue();
    
    typedef Any* (*MainFuncPtr)();
    MainFuncPtr MainPtr = (MainFuncPtr)rawAddr;
    
    MainPtr();
    return true;
}

llvm::Value* CodeGen::generateExpr(const Expr* expr) {
    if (auto* num = dynamic_cast<const NumberExpr*>(expr)) return visitNumberExpr(num);
    if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) return visitBinaryExpr(bin);
    if (auto* var = dynamic_cast<const VariableExpr*>(expr)) return visitVariableExpr(var);
    if (auto* bl = dynamic_cast<const BoolExpr*>(expr)) return visitBoolExpr(bl);
    if (auto* nl = dynamic_cast<const NilExpr*>(expr)) return visitNilExpr(nl);
    if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) return visitUnaryExpr(un);
    if (auto* assign = dynamic_cast<const AssignExpr*>(expr)) return visitAssignExpr(assign);
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) return visitCallExpr(call);
    if (auto* arr = dynamic_cast<const ArrayExpr*>(expr)) return visitArrayExpr(arr);
    if (auto* obj = dynamic_cast<const ObjectExpr*>(expr)) return visitObjectExpr(obj);
    if (auto* str = dynamic_cast<const StringExpr*>(expr)) return visitStringExpr(str);
    if (auto* idx = dynamic_cast<const IndexExpr*>(expr)) return visitIndexExpr(idx);
    if (auto* get = dynamic_cast<const GetExpr*>(expr)) return visitGetExpr(get);
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
    } else if (auto* classStmt = dynamic_cast<const ClassStmt*>(stmt)) {
        visitClassStmt(classStmt);
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
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(mergeBB);
    }
    thenBB = builder->GetInsertBlock();

    // Emit else block
    if (stmt->elseBranch) {
        elseBB->insertInto(func);
        builder->SetInsertPoint(elseBB);
        generateStmt(stmt->elseBranch.get());
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(mergeBB);
        }
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
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(condBB);
    }

    afterBB->insertInto(func);
    builder->SetInsertPoint(afterBB);
}

void CodeGen::visitForStmt(const ForStmt* stmt) {
    llvm::Function* func = builder->GetInsertBlock()->getParent();

    pushScope(); // Loop scope for i

    // 1. Initializer
    llvm::Value* startV = generateExpr(stmt->start.get()); // Any*
    if (!startV) {
        popScope();
        return;
    }

    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->varName);
    
    // Copy startV to alloca
    llvm::Value* startLoaded = builder->CreateLoad(anyType, startV);
    builder->CreateStore(startLoaded, alloca);
    
    scopes.back()[stmt->varName] = alloca;

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

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(condBB);
    }

    // 6. After
    afterBB->insertInto(func);
    builder->SetInsertPoint(afterBB);
    popScope();
}

llvm::Value* CodeGen::visitNumberExpr(const NumberExpr* expr) {
    return createNumber(expr->value);
}

llvm::Value* CodeGen::visitBinaryExpr(const BinaryExpr* expr) {
    if (expr->op == TokenType::K_And || expr->op == TokenType::K_Or) {
        bool isAnd = (expr->op == TokenType::K_And);
        llvm::Function* func = builder->GetInsertBlock()->getParent();
        
        llvm::Value* L = generateExpr(expr->left.get());
        llvm::BasicBlock* originBB = builder->GetInsertBlock();
        llvm::Value* LVal = unboxNumber(L);
        llvm::Value* LCond = builder->CreateFCmpONE(LVal, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "logical_cond");
        
        llvm::BasicBlock* rightBB = llvm::BasicBlock::Create(*context, "logical_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "logical_merge", func);
        
        if (isAnd) builder->CreateCondBr(LCond, rightBB, mergeBB);
        else builder->CreateCondBr(LCond, mergeBB, rightBB);
        
        // Eval Right
        builder->SetInsertPoint(rightBB);
        llvm::Value* R = generateExpr(expr->right.get());
        builder->CreateBr(mergeBB);
        rightBB = builder->GetInsertBlock();
        
        // Merge
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder->CreatePHI(builder->getPtrTy(), 2, "logical_result");
        phi->addIncoming(L, originBB); // Origin of skip
        phi->addIncoming(R, rightBB);
        
        return phi;
    }

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
        case TokenType::Percent: res = builder->CreateFRem(LVal, RVal, "remtmp"); break;
        
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

        // Bitwise Operators
        case TokenType::Ampersand: {
            llvm::Value* LInt = builder->CreateFPToSI(LVal, llvm::Type::getInt64Ty(*context), "lint");
            llvm::Value* RInt = builder->CreateFPToSI(RVal, llvm::Type::getInt64Ty(*context), "rint");
            res = builder->CreateAnd(LInt, RInt, "andtmp");
            res = builder->CreateSIToFP(res, llvm::Type::getDoubleTy(*context), "floatres");
            break;
        }
        case TokenType::Pipe: {
            llvm::Value* LInt = builder->CreateFPToSI(LVal, llvm::Type::getInt64Ty(*context), "lint");
            llvm::Value* RInt = builder->CreateFPToSI(RVal, llvm::Type::getInt64Ty(*context), "rint");
            res = builder->CreateOr(LInt, RInt, "ortmp");
            res = builder->CreateSIToFP(res, llvm::Type::getDoubleTy(*context), "floatres");
            break;
        }
        case TokenType::Caret: {
            llvm::Value* LInt = builder->CreateFPToSI(LVal, llvm::Type::getInt64Ty(*context), "lint");
            llvm::Value* RInt = builder->CreateFPToSI(RVal, llvm::Type::getInt64Ty(*context), "rint");
            res = builder->CreateXor(LInt, RInt, "xortmp");
            res = builder->CreateSIToFP(res, llvm::Type::getDoubleTy(*context), "floatres");
            break;
        }
        case TokenType::LessLess: {
            llvm::Value* LInt = builder->CreateFPToSI(LVal, llvm::Type::getInt64Ty(*context), "lint");
            llvm::Value* RInt = builder->CreateFPToSI(RVal, llvm::Type::getInt64Ty(*context), "rint");
            res = builder->CreateShl(LInt, RInt, "shltmp");
            res = builder->CreateSIToFP(res, llvm::Type::getDoubleTy(*context), "floatres");
            break;
        }
        case TokenType::GreaterGreater: {
            llvm::Value* LInt = builder->CreateFPToSI(LVal, llvm::Type::getInt64Ty(*context), "lint");
            llvm::Value* RInt = builder->CreateFPToSI(RVal, llvm::Type::getInt64Ty(*context), "shrtmp");
            res = builder->CreateAShr(LInt, RInt, "ashrtmp");
            res = builder->CreateSIToFP(res, llvm::Type::getDoubleTy(*context), "floatres");
            break;
        }
        
        default: 
            std::cerr << "Unimplemented binary operator: " << tokenTypeToString(expr->op) << "\n";
            return nullptr;
    }
    
    // Box result
    return boxDouble(res);
}

llvm::Value* CodeGen::visitBoolExpr(const BoolExpr* expr) {
    llvm::Function* func = module->getFunction("manifast_create_boolean");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), {builder->getInt1Ty()}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_boolean", module.get());
    }
    return builder->CreateCall(func, {builder->getInt1(expr->value)});
}

llvm::Value* CodeGen::visitNilExpr(const NilExpr* expr) {
    llvm::Function* func = module->getFunction("manifast_create_nil");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), {}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_nil", module.get());
    }
    return builder->CreateCall(func, {});
}

llvm::Value* CodeGen::visitUnaryExpr(const UnaryExpr* expr) {
    llvm::Value* v = generateExpr(expr->right.get());
    if (!v) return nullptr;
    
    llvm::Value* val = unboxNumber(v);
    
    if (expr->op == TokenType::Minus) {
        llvm::Value* res = builder->CreateFNeg(val, "negtmp");
        return boxDouble(res);
    } else if (expr->op == TokenType::Bang) {
        // Truthiness: 0 is false, others are true.
        // !v => (v == 0) ? 1 : 0
        llvm::Value* cond = builder->CreateFCmpOEQ(val, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "nottmp");
        
        llvm::Function* func = module->getFunction("manifast_create_boolean");
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), {builder->getInt1Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_boolean", module.get());
        }
        return builder->CreateCall(func, {cond});
    }
    return v;
}

llvm::Value* CodeGen::visitVariableExpr(const VariableExpr* expr) {
    llvm::Value* V = lookupVariable(expr->name);
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
    llvm::Value* val = generateExpr(expr->value.get()); // Returns Any* (temp)
    if (!val) return nullptr;

    if (auto* var = dynamic_cast<VariableExpr*>(expr->target.get())) {
        llvm::Value* V = lookupVariable(var->name);
        if (!V) {
            std::cerr << "Unknown variable name: " << var->name << "\n";
            return nullptr;
        }

        if (expr->op != TokenType::Equal) {
            llvm::Value* LVal = unboxNumber(V);
            llvm::Value* RVal = unboxNumber(val);
            llvm::Value* res = nullptr;
            switch (expr->op) {
                case TokenType::PlusEqual:    res = builder->CreateFAdd(LVal, RVal, "addtmp"); break;
                case TokenType::MinusEqual:   res = builder->CreateFSub(LVal, RVal, "subtmp"); break;
                case TokenType::StarEqual:    res = builder->CreateFMul(LVal, RVal, "multmp"); break;
                case TokenType::SlashEqual:   res = builder->CreateFDiv(LVal, RVal, "divtmp"); break;
                case TokenType::PercentEqual: res = builder->CreateFRem(LVal, RVal, "remtmp"); break;
                default: break;
            }
            if (res) val = boxDouble(res);
        }

        llvm::Value* loadedVal = builder->CreateLoad(anyType, val);
        builder->CreateStore(loadedVal, V);
        return val;
    } else if (auto* get = dynamic_cast<GetExpr*>(expr->target.get())) {
        llvm::Value* obj = generateExpr(get->object.get());
        llvm::Value* keyStr = builder->CreateGlobalString(get->name);
        
        llvm::Function* func = module->getFunction("manifast_object_set");
        if (!func) {
             llvm::FunctionType* ft = llvm::FunctionType::get(builder->getVoidTy(), {builder->getPtrTy(), builder->getPtrTy(), builder->getPtrTy()}, false);
             func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_object_set", module.get());
        }
        builder->CreateCall(func, {obj, keyStr, val});
        return val;
    } else if (auto* idx = dynamic_cast<IndexExpr*>(expr->target.get())) {
        llvm::Value* obj = generateExpr(idx->object.get());
        llvm::Value* indexVal = unboxNumber(generateExpr(idx->index.get()));
        
        llvm::Function* func = module->getFunction("manifast_array_set");
        if (!func) {
             llvm::FunctionType* ft = llvm::FunctionType::get(builder->getVoidTy(), {builder->getPtrTy(), builder->getDoubleTy(), builder->getPtrTy()}, false);
             func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_array_set", module.get());
        }
        builder->CreateCall(func, {obj, indexVal, val});
        return val;
    }

    std::cerr << "Invalid assignment target\n";
    return nullptr;
}

llvm::Value* CodeGen::visitCallExpr(const CallExpr* expr) {
    auto* var = dynamic_cast<VariableExpr*>(expr->callee.get());
    if (!var) {
        auto* get = dynamic_cast<GetExpr*>(expr->callee.get());
        if (get) {
            llvm::Value* obj = generateExpr(get->object.get()); // Any*
            std::string methodName = get->name;

            if (methodName == "push") {
                if (expr->args.size() != 1) {
                    std::cerr << "push() requires 1 argument\n";
                    return nullptr;
                }
                llvm::Value* arg = generateExpr(expr->args[0].get()); // Any*
                builder->CreateCall(module->getFunction("manifast_array_push"), {obj, arg});
                return boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
            } else if (methodName == "pop") {
                return builder->CreateCall(module->getFunction("manifast_array_pop"), {obj});
            } else {
                // Potential dynamic method call e.g. math.sin(0.5)
                llvm::Value* calleeVal = generateExpr(get); // Evaluates to Any*
                llvm::Value* argsArr = builder->CreateAlloca(anyType, builder->getInt32(expr->args.size() + 1));
                
                // Pack self (obj) as first argument
                llvm::Value* objVal = builder->CreateLoad(anyType, obj);
                builder->CreateStore(objVal, argsArr);
                
                for (size_t i = 0; i < expr->args.size(); ++i) {
                    llvm::Value* argVal = generateExpr(expr->args[i].get()); 
                    llvm::Value* argObj = builder->CreateLoad(anyType, argVal);
                    llvm::Value* slot = builder->CreateGEP(anyType, argsArr, {builder->getInt32(i + 1)});
                    builder->CreateStore(argObj, slot);
                }
                return builder->CreateCall(module->getFunction("manifast_call_dynamic"), 
                    {calleeVal, argsArr, builder->getInt32(expr->args.size() + 1)});
            }
        }

        // General dynamic call
        llvm::Value* calleeVal = generateExpr(expr->callee.get());
        llvm::Value* argsArr = builder->CreateAlloca(anyType, builder->getInt32(expr->args.size()));
        for (size_t i = 0; i < expr->args.size(); ++i) {
            llvm::Value* argVal = generateExpr(expr->args[i].get()); 
            llvm::Value* argObj = builder->CreateLoad(anyType, argVal);
            llvm::Value* slot = builder->CreateGEP(anyType, argsArr, {builder->getInt32(i)});
            builder->CreateStore(argObj, slot);
        }
        return builder->CreateCall(module->getFunction("manifast_call_dynamic"), 
            {calleeVal, argsArr, builder->getInt32(expr->args.size())});
    }

    std::string funcName = var->name;
    
    llvm::Function* func = nullptr;
    if (funcName == "print") {
        func = module->getFunction("manifast_print_any");
    } else if (funcName == "println") {
        func = module->getFunction("manifast_println_any");
    } else if (funcName == "printfmt") {
        func = module->getFunction("manifast_printfmt");
    } else if (funcName == "input") {
        func = module->getFunction("manifast_input");
    } else if (funcName == "assert") {
        func = module->getFunction("manifast_assert");
    } else if (funcName == "impor") {
        if (expr->args.size() != 1) {
            std::cerr << "impor() requires 1 argument\n";
            return nullptr;
        }
        llvm::Value* argVal = generateExpr(expr->args[0].get());
        llvm::Value* strPtr = unboxString(argVal);
        return builder->CreateCall(module->getFunction("manifast_impor"), {strPtr}, "impor_res");
    } else if (funcName == "len") {
        if (expr->args.size() != 1) {
            std::cerr << "len() requires 1 argument\n";
            return nullptr;
        }
        llvm::Value* arg = generateExpr(expr->args[0].get());
        llvm::Value* lenVal = builder->CreateCall(module->getFunction("manifast_array_len"), {arg});
        return boxDouble(lenVal);
    } else {
        func = module->getFunction(funcName);
    }

    if (!func) {
        std::cerr << "Unknown function: " << var->name << "\n";
        return nullptr;
    }

    bool isPrint = (funcName == "print" || funcName == "println");

    if (!isPrint && func->arg_size() != expr->args.size()) {
        std::cerr << "Incorrect number of arguments for " << var->name << "\n";
        return nullptr;
    }

    if (isPrint) {
        llvm::Value* res = nullptr;
        for (size_t i = 0; i < expr->args.size(); ++i) {
            if (i > 0) {
                // Print tab
                llvm::Value* tab = createString("\t");
                builder->CreateCall(module->getFunction("manifast_print_any"), {tab});
            }
            
            llvm::Value* argVal = generateExpr(expr->args[i].get());
            if (!argVal) return nullptr;
            
            if (i == expr->args.size() - 1 && funcName == "println") {
                builder->CreateCall(module->getFunction("manifast_println_any"), {argVal});
            } else {
                builder->CreateCall(module->getFunction("manifast_print_any"), {argVal});
            }
        }

        if (expr->args.empty() && funcName == "println") {
             // Print just newline
             llvm::Value* empty = createString("");
             builder->CreateCall(module->getFunction("manifast_println_any"), {empty});
        }

        return boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }

    std::vector<llvm::Value*> argsV;
    for (const auto& arg : expr->args) {
        llvm::Value* argVal = generateExpr(arg.get()); // Any*
        if (!argVal) return nullptr;
        argsV.push_back(argVal);
    }

    llvm::Value* callRes = builder->CreateCall(func, argsV);
    
    if (func->getReturnType()->isVoidTy()) {
        return boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }
    
    return callRes;
}

void CodeGen::visitVarDeclStmt(const VarDeclStmt* stmt) {
    if (scopes.size() == 1) { // Module-level Global
        auto* gVar = new llvm::GlobalVariable(*module, anyType, false,
                                             llvm::GlobalValue::InternalLinkage,
                                             llvm::ConstantAggregateZero::get(anyType),
                                             stmt->name);
        scopes.back()[stmt->name] = gVar;
        
        if (stmt->initializer) {
            llvm::Value* initVal = generateExpr(stmt->initializer.get());
            if (initVal) {
                builder->CreateStore(builder->CreateLoad(anyType, initVal), gVar);
            }
        }
    } else {
        llvm::Function* func = builder->GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
        llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->name);
        
        scopes.back()[stmt->name] = alloca;
        
        if (stmt->initializer) {
            llvm::Value* initVal = generateExpr(stmt->initializer.get());
            if (initVal) {
                builder->CreateStore(builder->CreateLoad(anyType, initVal), alloca);
            }
        }
    }
}

void CodeGen::visitReturnStmt(const ReturnStmt* stmt) {
    if (stmt->value) {
        llvm::Value* val = generateExpr(stmt->value.get()); // Any*
        if (val) {
            builder->CreateRet(val);
        }
    } else {
        // Return 0 for now.
         llvm::Value* retVal = boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
         builder->CreateRet(retVal);
    }
}

void CodeGen::visitBlockStmt(const BlockStmt* stmt) {
    pushScope();
    for (const auto& s : stmt->statements) {
        if (builder->GetInsertBlock()->getTerminator()) break;
        generateStmt(s.get());
    }
    popScope();
}

void CodeGen::visitTryStmt(const TryStmt* stmt) {
    // Basic try/catch stub: just execute the try body for now.
    // Full LLVM Exception Handling (Personality, LandingPads) is TBD.
    if (stmt->tryBody) {
        generateStmt(stmt->tryBody.get());
    }
}

void CodeGen::visitFunctionStmt(const FunctionStmt* stmt) {
    // Return type Any*, Args Any*
    std::vector<llvm::Type*> argTypes(stmt->params.size(), builder->getPtrTy());
    llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), argTypes, false);
    llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, stmt->name, module.get());
    func->addFnAttr("stack-probe-size", "1048576"); // Disable stack probing
    func->addFnAttr("no-stack-arg-probe");

    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context, "entry", func);
    
    llvm::BasicBlock* oldBB = builder->GetInsertBlock();
    builder->SetInsertPoint(bb);

    pushScope();
    
    uint32_t idx = 0;
    for (auto& arg : func->args()) {
        // Create alloca for arg (Type Any)
        llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
        llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->params[idx]);
        
        // Load Any struct from the passed pointer and store into alloca
        llvm::Value* loadedArg = builder->CreateLoad(anyType, &arg);
        builder->CreateStore(loadedArg, alloca);
        
        scopes.back()[stmt->params[idx]] = alloca;
        idx++;
    }

    // Body
    generateStmt(stmt->body.get());
    
    // Default return 0 if no return stmt
    if (!builder->GetInsertBlock()->getTerminator()) {
         llvm::Value* retVal = boxDouble(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
         builder->CreateRet(retVal);
    }
    
    popScope();

    if (llvm::verifyFunction(*func, &llvm::errs())) {
        std::cerr << "Function verification failed for " << stmt->name << "\n";
    }
    
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

llvm::Value* CodeGen::visitStringExpr(const StringExpr* expr) {
    return createString(expr->value);
}

llvm::Value* CodeGen::visitIndexExpr(const IndexExpr* expr) {
    llvm::Value* obj = generateExpr(expr->object.get());
    llvm::Value* idx = generateExpr(expr->index.get());
    if (!obj || !idx) return nullptr;

    // idx is Any*, we need the double value
    llvm::Value* idxVal = unboxNumber(idx);

    llvm::Function* func = module->getFunction("manifast_array_get");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), {builder->getPtrTy(), builder->getDoubleTy()}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_array_get", module.get());
    }
    return builder->CreateCall(func, {obj, idxVal}, "index_res");
}

llvm::Value* CodeGen::visitGetExpr(const GetExpr* expr) {
    llvm::Value* obj = generateExpr(expr->object.get());
    if (!obj) return nullptr;

    llvm::Function* func = module->getFunction("manifast_object_get");
    if (!func) {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), {builder->getPtrTy(), builder->getPtrTy()}, false);
        func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_object_get", module.get());
    }
    llvm::Value* keyStr = builder->CreateGlobalString(expr->name);
    return builder->CreateCall(func, {obj, keyStr}, "get_res");
}

void CodeGen::visitClassStmt(const ClassStmt* stmt) {
    llvm::Function* createClassFunc = module->getFunction("manifast_create_class");
    if (!createClassFunc) {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getPtrTy(), {builder->getPtrTy()}, false);
        createClassFunc = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "manifast_create_class", module.get());
    }
    
    llvm::Value* classNameStr = builder->CreateGlobalString(stmt->name);
    llvm::Value* klassAny = builder->CreateCall(createClassFunc, {classNameStr});
    
    // Define class in scope
    llvm::Function* currentFunc = builder->GetInsertBlock()->getParent();
    llvm::IRBuilder<> tmpBuilder(&currentFunc->getEntryBlock(), currentFunc->getEntryBlock().begin());
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(anyType, nullptr, stmt->name);
    scopes.back()[stmt->name] = alloca;
    
    llvm::Value* loadedKlass = builder->CreateLoad(anyType, klassAny);
    builder->CreateStore(loadedKlass, alloca);

    // Compile methods
    for (const auto& method : stmt->methods) {
        std::string mangledName = stmt->name + "." + method->name;
        
        // Temporarily change name to compile
        std::string originalName = method->name;
        // In reality, visitFunctionStmt uses method->name.
        // I should probably have a move-based or argument-based visitFunctionStmt.
        // For now, let's just use the current visitFunctionStmt and Rename the function after creation.
        
        visitFunctionStmt(method.get());
        llvm::Function* methodFunc = module->getFunction(method->name);
        if (methodFunc) {
            methodFunc->setName(mangledName);
            
            // Register method in class.methods object
            // Need to wrap the function in an Any (type 5 for bytecode? No, maybe 4 for native-like or 5)
            // Actually, Runtime.h doesn't have a manifast_create_function for LLVM functions.
            // For now, let's just skip registering in methods object if we don't have the wrapper.
            // The interpreter uses a pointer to Closure/Function.
        }
    }
}

} // namespace manifast
