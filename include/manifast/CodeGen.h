#ifndef MANIFAST_CODEGEN_H
#define MANIFAST_CODEGEN_H

#include "AST.h"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <memory>

namespace manifast {

class CodeGen {
public:
    CodeGen();
    void compile(const std::vector<std::unique_ptr<Stmt>>& statements);
    void printIR(); 
    bool run(); // JIT Execution entry

    // AOT Emission
    void emitIR(const std::string& path);
    void emitAssembly(const std::string& path);
    void emitObject(const std::string& path);
    void addMainEntry();

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    // Scope management
    std::vector<std::map<std::string, llvm::Value*>> scopes;
    void pushScope();
    void popScope();
    llvm::Value* lookupVariable(const std::string& name);

    llvm::Value* generateExpr(const Expr* expr);
    void generateStmt(const Stmt* stmt);
    
    // Visitors
    llvm::Value* visitNumberExpr(const NumberExpr* expr);
    llvm::Value* visitBinaryExpr(const BinaryExpr* expr);
    llvm::Value* visitVariableExpr(const VariableExpr* expr);
    llvm::Value* visitBoolExpr(const BoolExpr* expr);
    llvm::Value* visitNilExpr(const NilExpr* expr);
    llvm::Value* visitUnaryExpr(const UnaryExpr* expr);
    llvm::Value* visitAssignExpr(const AssignExpr* expr);
    llvm::Value* visitCallExpr(const CallExpr* expr);
    llvm::Value* visitArrayExpr(const ArrayExpr* expr);
    llvm::Value* visitObjectExpr(const ObjectExpr* expr);
    llvm::Value* visitStringExpr(const StringExpr* expr);
    llvm::Value* visitIndexExpr(const IndexExpr* expr);
    llvm::Value* visitGetExpr(const GetExpr* expr);
    
    // Statements
    void visitVarDeclStmt(const VarDeclStmt* stmt);
    void visitReturnStmt(const ReturnStmt* stmt);
    void visitBlockStmt(const BlockStmt* stmt);
    void visitIfStmt(const IfStmt* stmt);
    void visitWhileStmt(const WhileStmt* stmt);
    void visitForStmt(const ForStmt* stmt);
    void visitFunctionStmt(const FunctionStmt* stmt);
    void visitClassStmt(const ClassStmt* stmt);
    void visitTryStmt(const TryStmt* stmt);

private:
    // Dynamic Typing Support
    llvm::StructType* anyType;
    llvm::StructType* arrayType;
    llvm::StructType* objectType;

    // Helpers
    void initializeTypes();
    llvm::Value* createNumber(double value); // Literals
    llvm::Value* createString(const std::string& value);
    llvm::Value* boxDouble(llvm::Value* v);  // Runtime values
    llvm::Value* createArray(const std::vector<llvm::Value*>& elements);
    llvm::Value* createObject(const std::vector<std::pair<std::string, llvm::Value*>>& pairs);
    llvm::Value* unboxNumber(llvm::Value* anyVal);
    llvm::Value* unboxString(llvm::Value* anyVal);
    void printAny(llvm::Value* anyVal); // Runtime helper stub
};

} // namespace manifast

#endif // MANIFAST_CODEGEN_H
