#ifndef MANIFAST_PARSER_H
#define MANIFAST_PARSER_H

#include "Lexer.h"
#include "AST.h"
#include <vector>
#include <memory>
#include <optional>

namespace manifast {

class Parser {
public:
    Parser(Lexer& lexer, std::string_view source = "");
    bool debugMode = false;

    // Entry point
    std::vector<std::unique_ptr<Stmt>> parse();

private:
    // Statement Parsers
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Stmt> parseVarDeclaration();
    std::unique_ptr<Stmt> parseIfStatement();
    std::unique_ptr<Stmt> parseIfChain();
    std::unique_ptr<Stmt> parseWhileStatement();
    std::unique_ptr<Stmt> parseForStatement();
    std::unique_ptr<Stmt> parseFunctionStatement();
    std::unique_ptr<Stmt> parseClassStatement();
    std::unique_ptr<Stmt> parseTryStatement();
    std::unique_ptr<Stmt> parseReturnStatement();
    std::vector<std::unique_ptr<Stmt>> parseBlock(); 

    // Expression Parsers 
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAssignment(); 
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseBitwiseOr();
    std::unique_ptr<Expr> parseBitwiseXor();
    std::unique_ptr<Expr> parseBitwiseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseShift();
    std::unique_ptr<Expr> parseTerm();
    std::unique_ptr<Expr> parseFactor();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parseCall();  
    std::unique_ptr<Expr> parseFunctionExpression();
    std::unique_ptr<Expr> parsePrimary();

    // Helpers
    bool match(TokenType type);
    bool check(TokenType type);
    Token consume(TokenType type, const std::string& message);
    void error(Token token, const std::string& message);
    Token advance();
    Token peek();
    Token previous();
    void synchronize();
    
    template<typename T, typename... Args>
    std::unique_ptr<T> makeNode(const Token& token, Args&&... args) {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        node->line = token.location.line;
        node->offset = token.location.offset;
        return node;
    }

private:
    Lexer& lexer;
    std::string_view source; // Store source for error reporting
    Token currentToken;
    Token previousToken;
    bool hasError = false;
};

} // namespace manifast

#endif // MANIFAST_PARSER_H
