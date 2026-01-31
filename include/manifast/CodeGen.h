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
    void run(); // JIT Execution entry

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    // Symbol table for variables (name -> llvm::Value*)
    std::map<std::string, llvm::Value*> namedValues;

    llvm::Value* generateExpr(const Expr* expr);
    void generateStmt(const Stmt* stmt);
    
    // Visitors
    llvm::Value* visitNumberExpr(const NumberExpr* expr);
    llvm::Value* visitBinaryExpr(const BinaryExpr* expr);
    llvm::Value* visitVariableExpr(const VariableExpr* expr);
    llvm::Value* visitAssignExpr(const AssignExpr* expr);
    llvm::Value* visitCallExpr(const CallExpr* expr);
    
    // Statements
    void visitVarDeclStmt(const VarDeclStmt* stmt);
    void visitReturnStmt(const ReturnStmt* stmt);
    void visitBlockStmt(const BlockStmt* stmt);
    void visitIfStmt(const IfStmt* stmt);
    void visitWhileStmt(const WhileStmt* stmt);
    void visitForStmt(const ForStmt* stmt);
    void visitFunctionStmt(const FunctionStmt* stmt);
};

} // namespace manifast

#endif // MANIFAST_CODEGEN_H
