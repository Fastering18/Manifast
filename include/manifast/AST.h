#ifndef MANIFAST_AST_H
#define MANIFAST_AST_H

#include "Token.h"
#include <memory>
#include <vector>
#include <string>

namespace manifast {

// --- Types ---

enum class TypeKind {
    Any,
    Int8, Int16, Int32, Int64,
    Float32, Float64,
    Char,
    Bool,
    String,
    Void,
    Array,
    Pointer,
    Function,
    Struct,
    Alias
};

struct Type {
    TypeKind kind;
    std::shared_ptr<Type> baseType; // For Array/Pointer
    std::vector<Type> params; // For Function
    std::shared_ptr<Type> returnType; // For Function

    struct Field {
        std::string name;
        std::shared_ptr<Type> type;
    };
    std::vector<Field> fields; // For Struct
    std::string aliasName; // For Alias

    Type(TypeKind k = TypeKind::Any) : kind(k) {}
    
    static Type makeArray(Type base) {
        Type t(TypeKind::Array);
        t.baseType = std::make_shared<Type>(std::move(base));
        return t;
    }

    static Type makePointer(Type base) {
        Type t(TypeKind::Pointer);
        t.baseType = std::make_shared<Type>(std::move(base));
        return t;
    }

    std::string toString() const {
        switch (kind) {
            case TypeKind::Any: return "any";
            case TypeKind::Int8: return "i8";
            case TypeKind::Int16: return "i16";
            case TypeKind::Int32: return "i32";
            case TypeKind::Int64: return "i64";
            case TypeKind::Float32: return "f32";
            case TypeKind::Float64: return "f64";
            case TypeKind::Char: return "char";
            case TypeKind::Bool: return "boolean";
            case TypeKind::String: return "string";
            case TypeKind::Void: return "void";
            case TypeKind::Array: return baseType->toString() + "[]";
            case TypeKind::Pointer: return "*" + baseType->toString();
            case TypeKind::Alias: return aliasName;
            case TypeKind::Struct: {
                std::string s = "{";
                for (size_t i = 0; i < fields.size(); ++i) {
                    s += fields[i].name + ": " + fields[i].type->toString();
                    if (i < fields.size() - 1) s += ", ";
                }
                s += "}";
                return s;
            }
            case TypeKind::Function: {
                std::string s = "fungsi(";
                for (size_t i = 0; i < params.size(); ++i) {
                    s += params[i].toString();
                    if (i < params.size() - 1) s += ", ";
                }
                s += "): " + returnType->toString();
                return s;
            }
        }
        return "unknown";
    }
};

class Stmt;

// Base class for all AST nodes
class ASTNode {
public:
    int line = 0;
    int offset = -1;
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

class BoolExpr : public Expr {
public:
    bool value;
    BoolExpr(bool value) : value(value) {}
};

class CharExpr : public Expr {
public:
    char value;
    CharExpr(char value) : value(value) {}
};

class NilExpr : public Expr {
public:
    NilExpr() {}
};

class VariableExpr : public Expr {
public:
    std::string name;
    VariableExpr(std::string name) : name(std::move(name)) {}
};

class UnaryExpr : public Expr {
public:
    TokenType op;
    std::unique_ptr<Expr> right;

    UnaryExpr(TokenType op, std::unique_ptr<Expr> right)
        : op(op), right(std::move(right)) {}
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

struct Parameter {
    std::string name;
    Type type;
    int line = 0;
    int offset = -1;
};

class FunctionExpr : public Expr {
public:
    std::vector<Parameter> params;
    Type returnType;
    std::unique_ptr<Stmt> body;
    
    FunctionExpr(std::vector<Parameter> params, Type returnType, std::unique_ptr<Stmt> body)
        : params(std::move(params)), returnType(std::move(returnType)), body(std::move(body)) {}
};

class ObjectExpr : public Expr {
public:
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries;
    
    ObjectExpr(std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries)
        : entries(std::move(entries)) {}
};

class SliceExpr : public Expr {
public:
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    
    SliceExpr(std::unique_ptr<Expr> start, std::unique_ptr<Expr> end)
        : start(std::move(start)), end(std::move(end)) {}
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
    Type typeAnnotation; // Use Type instead of string
    std::unique_ptr<Expr> initializer; // Can be null
    bool isConst;

    VarDeclStmt(std::string name, Type typeAnnotation, std::unique_ptr<Expr> initializer, bool isConst)
        : name(std::move(name)), typeAnnotation(std::move(typeAnnotation)), initializer(std::move(initializer)), isConst(isConst) {}
};

class TypeAliasStmt : public Stmt {
public:
    std::string name;
    Type type;
    TypeAliasStmt(std::string name, Type type) : name(std::move(name)), type(std::move(type)) {}
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
    std::vector<Parameter> params;
    Type returnType;
    std::unique_ptr<Stmt> body;

    FunctionStmt(std::string name, std::vector<Parameter> params, Type returnType, std::unique_ptr<Stmt> body)
        : name(std::move(name)), params(std::move(params)), returnType(std::move(returnType)), body(std::move(body)) {}
};

class ClassStmt : public Stmt {
public:
    std::string name;
    std::vector<std::unique_ptr<FunctionStmt>> methods;
    
    ClassStmt(std::string name, std::vector<std::unique_ptr<FunctionStmt>> methods)
        : name(std::move(name)), methods(std::move(methods)) {}
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
