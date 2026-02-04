#ifndef MANIFAST_AST_H
#define MANIFAST_AST_H

#include "Token.h"
#include <memory>
#include <vector>
#include <string>

namespace manifast {

class Stmt;

// Base class for all AST nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
};

// --- Expressions ---

class Expr : public ASTNode {
public:
    virtual ~Expr() = default;
};

class NumberExpr : public Expr {
public:
    double value;
    NumberExpr(double value) : value(value) {}
};

class StringExpr : public Expr {
public:
    std::string value;
    StringExpr(std::string value) : value(std::move(value)) {}
};

class VariableExpr : public Expr {
public:
    std::string name;
    VariableExpr(std::string name) : name(std::move(name)) {}
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    TokenType op;
    std::unique_ptr<Expr> right;

    BinaryExpr(std::unique_ptr<Expr> left, TokenType op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(op), right(std::move(right)) {}
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee; // Generalized callee (Expr instead of string)
    std::vector<std::unique_ptr<Expr>> args;
    
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), args(std::move(args)) {}
};

class AssignExpr : public Expr {
public:
    std::unique_ptr<Expr> target; 
    std::unique_ptr<Expr> value;
    TokenType op; // Equal, PlusEqual, etc.
    
    AssignExpr(std::unique_ptr<Expr> target, std::unique_ptr<Expr> value, TokenType op = TokenType::Equal)
        : target(std::move(target)), value(std::move(value)), op(op) {}
};

class GetExpr : public Expr {
public:
    std::unique_ptr<Expr> object;
    std::string name;
    
    GetExpr(std::unique_ptr<Expr> object, std::string name)
        : object(std::move(object)), name(std::move(name)) {}
};

class IndexExpr : public Expr {
public:
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
    
    IndexExpr(std::unique_ptr<Expr> object, std::unique_ptr<Expr> index)
        : object(std::move(object)), index(std::move(index)) {}
};

class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    
    ArrayExpr(std::vector<std::unique_ptr<Expr>> elements)
        : elements(std::move(elements)) {}
};

class FunctionExpr : public Expr {
public:
    std::vector<std::string> params;
    std::unique_ptr<Stmt> body;
    
    FunctionExpr(std::vector<std::string> params, std::unique_ptr<Stmt> body)
        : params(std::move(params)), body(std::move(body)) {}
};

class ObjectExpr : public Expr {
public:
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries;
    
    ObjectExpr(std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries)
        : entries(std::move(entries)) {}
};

// --- Statements ---

class Stmt : public ASTNode {
public:
    virtual ~Stmt() = default;
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;
    ExprStmt(std::unique_ptr<Expr> expression) : expression(std::move(expression)) {}
};

class ReturnStmt : public Stmt {
public:
    std::unique_ptr<Expr> value; // Can be null for void return
    ReturnStmt(std::unique_ptr<Expr> value) : value(std::move(value)) {}
};

class BlockStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    BlockStmt(std::vector<std::unique_ptr<Stmt>> statements) : statements(std::move(statements)) {}
};

class VarDeclStmt : public Stmt {
public:
    std::string name;
    std::unique_ptr<Expr> initializer; // Can be null
    bool isConst;

    VarDeclStmt(std::string name, std::unique_ptr<Expr> initializer, bool isConst)
        : name(std::move(name)), initializer(std::move(initializer)), isConst(isConst) {}
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch; // Can be null

    IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch)
        : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
        : condition(std::move(condition)), body(std::move(body)) {}
};

class ForStmt : public Stmt {
public:
    std::string varName;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    std::unique_ptr<Expr> step; // Can be null (default 1)
    std::unique_ptr<Stmt> body;

    ForStmt(std::string varName, std::unique_ptr<Expr> start, std::unique_ptr<Expr> end, std::unique_ptr<Expr> step, std::unique_ptr<Stmt> body)
        : varName(std::move(varName)), start(std::move(start)), end(std::move(end)), step(std::move(step)), body(std::move(body)) {}
};

class FunctionStmt : public Stmt {
public:
    std::string name;
    std::vector<std::string> params; // Just names for now
    std::unique_ptr<Stmt> body;

    FunctionStmt(std::string name, std::vector<std::string> params, std::unique_ptr<Stmt> body)
        : name(std::move(name)), params(std::move(params)), body(std::move(body)) {}
};

class TryStmt : public Stmt {
public:
    std::unique_ptr<Stmt> tryBody;
    std::string catchVar; // Name of the caught exception variable (optional)
    std::unique_ptr<Stmt> catchBody; // Can be null

    TryStmt(std::unique_ptr<Stmt> tryBody, std::string catchVar, std::unique_ptr<Stmt> catchBody)
        : tryBody(std::move(tryBody)), catchVar(std::move(catchVar)), catchBody(std::move(catchBody)) {}
};

} // namespace manifast

#endif // MANIFAST_AST_H
