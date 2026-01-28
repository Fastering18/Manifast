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
    Parser(Lexer& lexer);

    // Entry point
    std::vector<std::unique_ptr<Stmt>> parse();

private:
    // Statement Parsers
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Stmt> parseVarDeclaration();
    std::unique_ptr<Stmt> parseIfStatement();
    std::unique_ptr<Stmt> parseWhileStatement();
    std::unique_ptr<Stmt> parseForStatement();
    std::unique_ptr<Stmt> parseFunctionStatement();
    std::unique_ptr<Stmt> parseTryStatement();
    std::unique_ptr<Stmt> parseReturnStatement();
    std::vector<std::unique_ptr<Stmt>> parseBlock(); 

    // Expression Parsers 
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAssignment(); 
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
    std::unique_ptr<Expr> parsePrimary();

    // Helpers
    bool match(TokenType type);
    bool check(TokenType type);
    Token consume(TokenType type, const std::string& message);
    Token advance();
    Token peek();
    Token previous();

private:
    Lexer& lexer;
    Token currentToken;
    Token previousToken;
};

} // namespace manifast

#endif // MANIFAST_PARSER_H
