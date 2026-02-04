#include "manifast/Parser.h"
#include <iostream>

namespace manifast {

Parser::Parser(Lexer& lexer) : lexer(lexer) {
    advance(); // Load first token
}

Token Parser::advance() {
    previousToken = currentToken;
    currentToken = lexer.nextToken();
    
    if (currentToken.type == TokenType::Error) {
        std::cerr << "Lexer Error: " << currentToken.lexeme << " at line " << currentToken.location.line << "\n";
    }
    
    return previousToken;
}

Token Parser::peek() {
    return currentToken;
}

Token Parser::previous() {
    return previousToken;
}

bool Parser::check(TokenType type) {
    if (currentToken.type == TokenType::EndOfFile) return false;
    return currentToken.type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    std::cerr << "Parser Error: " << message << " at token " << tokenTypeToString(currentToken.type) 
              << " (" << currentToken.lexeme << ") at line " << currentToken.location.line << "\n";
    // Synchronize to next statement
    synchronize();
    return currentToken;
}

void Parser::synchronize() {
    advance();
    while (currentToken.type != TokenType::EndOfFile) {
        if (previousToken.type == TokenType::Semicolon) return;

        switch (currentToken.type) {
            case TokenType::K_Function:
            case TokenType::K_If:
            case TokenType::K_While:
            case TokenType::K_For:
            case TokenType::K_Try:
            case TokenType::K_Return:
            case TokenType::K_Var:
            case TokenType::K_Do:
                return;
            default: break;
        }

        advance();
    }
}

// --- Main Parse Loop ---

std::vector<std::unique_ptr<Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (currentToken.type != TokenType::EndOfFile) {
        auto stmt = parseStatement();
        if (stmt) statements.push_back(std::move(stmt));
        else {
             synchronize();
        }
    }
    return statements;
}

// --- Statements ---

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (match(TokenType::K_Function)) return parseFunctionStatement();
    if (match(TokenType::K_If)) return parseIfStatement();
    if (match(TokenType::K_While)) return parseWhileStatement();
    if (match(TokenType::K_For)) return parseForStatement();
    if (match(TokenType::K_Try)) return parseTryStatement();
    if (match(TokenType::K_Return)) return parseReturnStatement();
    if (match(TokenType::K_Var)) return parseVarDeclaration();
    if (match(TokenType::K_Do)) {
        std::vector<std::unique_ptr<Stmt>> body = parseBlock();
        consume(TokenType::K_End, "Expect 'tutup' after block");
        return std::make_unique<BlockStmt>(std::move(body));
    }
    
    auto expr = parseExpression();
    if (match(TokenType::Semicolon)) { }
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<Stmt> Parser::parseFunctionStatement() {
    Token name = consume(TokenType::Identifier, "Expect function name");
    consume(TokenType::LParen, "Expect '(' after function name");
    
    std::vector<std::string> params;
    if (!check(TokenType::RParen)) {
        do {
            Token param = consume(TokenType::Identifier, "Expect parameter name");
            params.push_back(std::string(param.lexeme));
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expect ')' after parameters");
    
    std::vector<std::unique_ptr<Stmt>> body = parseBlock(); 
    consume(TokenType::K_End, "Expect 'tutup' after function body");
    
    return std::make_unique<FunctionStmt>(std::string(name.lexeme), std::move(params), std::make_unique<BlockStmt>(std::move(body)));
}

std::unique_ptr<Stmt> Parser::parseIfStatement() {
    auto cond = parseExpression();
    consume(TokenType::K_Then, "Expect 'maka' after condition");
    
    std::vector<std::unique_ptr<Stmt>> thenStmts = parseBlock();
    auto thenBranch = std::make_unique<BlockStmt>(std::move(thenStmts));
    
    std::unique_ptr<Stmt> elseBranch = nullptr;
    
    if (match(TokenType::K_ElseIf)) {
        elseBranch = parseIfChain();
    } else if (match(TokenType::K_Else)) {
        std::vector<std::unique_ptr<Stmt>> elseStmts = parseBlock();
        elseBranch = std::make_unique<BlockStmt>(std::move(elseStmts));
    }
    
    consume(TokenType::K_End, "Expect 'tutup' after if-chain");
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Stmt> Parser::parseIfChain() {
    auto cond = parseExpression();
    consume(TokenType::K_Then, "Expect 'maka' after elseif condition");
    
    std::vector<std::unique_ptr<Stmt>> thenStmts = parseBlock();
    auto thenBranch = std::make_unique<BlockStmt>(std::move(thenStmts));
    
    std::unique_ptr<Stmt> elseBranch = nullptr;
    
    if (match(TokenType::K_ElseIf)) {
        elseBranch = parseIfChain();
    } else if (match(TokenType::K_Else)) {
        std::vector<std::unique_ptr<Stmt>> elseStmts = parseBlock();
        elseBranch = std::make_unique<BlockStmt>(std::move(elseStmts));
    }
    
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Stmt> Parser::parseWhileStatement() {
    auto cond = parseExpression();
    consume(TokenType::K_Do, "Expect 'lakukan' after condition"); 
    std::vector<std::unique_ptr<Stmt>> bodyStmts = parseBlock();
    consume(TokenType::K_End, "Expect 'tutup' after while-block");
    return std::make_unique<WhileStmt>(std::move(cond), std::make_unique<BlockStmt>(std::move(bodyStmts)));
}

std::unique_ptr<Stmt> Parser::parseForStatement() {
    Token varToken = consume(TokenType::Identifier, "Expect variable name after 'untuk'");
    consume(TokenType::Equal, "Expect '=' after variable name");
    auto start = parseExpression();
    
    consume(TokenType::K_To, "Expect 'ke' after start value");
    auto end = parseExpression();
    
    std::unique_ptr<Expr> step = nullptr;
    if (match(TokenType::K_Step)) {
        step = parseExpression();
    }
    
    consume(TokenType::K_Do, "Expect 'lakukan' before loop body");
    
    std::vector<std::unique_ptr<Stmt>> bodyStmts = parseBlock();
    consume(TokenType::K_End, "Expect 'tutup' after for-loop");
    return std::make_unique<ForStmt>(std::string(varToken.lexeme), std::move(start), std::move(end), std::move(step), std::make_unique<BlockStmt>(std::move(bodyStmts)));
}

std::unique_ptr<Stmt> Parser::parseTryStatement() {
    std::vector<std::unique_ptr<Stmt>> tryStmts;
    while (!check(TokenType::K_Catch) && !check(TokenType::K_End) && !check(TokenType::EndOfFile)) {
        tryStmts.push_back(parseStatement());
    }
    
    std::unique_ptr<Stmt> catchBranch = nullptr;
    std::string catchVar = "";

    if (match(TokenType::K_Catch)) {
        if (check(TokenType::Identifier)) {
            Token varName = consume(TokenType::Identifier, "Expect exception variable name");
            catchVar = std::string(varName.lexeme);
            if (check(TokenType::K_Then)) advance(); 
        }
        std::vector<std::unique_ptr<Stmt>> catchStmts = parseBlock(); 
        catchBranch = std::make_unique<BlockStmt>(std::move(catchStmts));
    }
    
    consume(TokenType::K_End, "Expect 'tutup' after try/catch block");
    return std::make_unique<TryStmt>(std::make_unique<BlockStmt>(std::move(tryStmts)), catchVar, std::move(catchBranch));
}

std::unique_ptr<Stmt> Parser::parseReturnStatement() {
    std::unique_ptr<Expr> value = nullptr;
    if (!check(TokenType::Semicolon) && !check(TokenType::K_End) && !check(TokenType::K_Else) && !check(TokenType::K_Catch)) {
        value = parseExpression();
    }
    match(TokenType::Semicolon); 
    return std::make_unique<ReturnStmt>(std::move(value));
}

std::unique_ptr<Stmt> Parser::parseVarDeclaration() {
    Token name = consume(TokenType::Identifier, "Expect variable name");
    std::unique_ptr<Expr> initializer = nullptr;
    if (match(TokenType::Equal)) {
        initializer = parseExpression();
    }
    match(TokenType::Semicolon);
    return std::make_unique<VarDeclStmt>(std::string(name.lexeme), std::move(initializer), false);
}

std::vector<std::unique_ptr<Stmt>> Parser::parseBlock() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TokenType::K_End) && !check(TokenType::K_Else) && !check(TokenType::K_ElseIf) && !check(TokenType::K_Catch) && 
           !check(TokenType::EndOfFile)) {
        statements.push_back(parseStatement());
    }
    return statements;
}

// --- Expressions ---

std::unique_ptr<Expr> Parser::parseExpression() {
    return parseAssignment();
}

std::unique_ptr<Expr> Parser::parseAssignment() {
    auto expr = parseLogicalOr(); // Start of precedence chain (assignment is lowest)
    
    // Check for assignment tokens
    // =, +=, -=, *=, /=, %=
    if (check(TokenType::Equal) || check(TokenType::PlusEqual) || check(TokenType::MinusEqual) ||
        check(TokenType::StarEqual) || check(TokenType::SlashEqual) || check(TokenType::PercentEqual)) {
        
        TokenType op = currentToken.type;
        advance();
        Token opToken = previous();
        
        auto value = parseAssignment();
        
        if (dynamic_cast<VariableExpr*>(expr.get()) ||
            dynamic_cast<GetExpr*>(expr.get()) ||
            dynamic_cast<IndexExpr*>(expr.get())) {
             return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        std::cerr << "Invalid assignment target.\n"; 
    }
    
    return expr;
}

// LogicOr -> LogicAnd -> BitwiseOr -> BitwiseXor -> BitwiseAnd -> Equality -> Comparison -> Shift -> Term -> Factor

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (match(TokenType::K_Or)) {
        TokenType op = previous().type;
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto expr = parseBitwiseOr();
    while (match(TokenType::K_And)) {
        TokenType op = previous().type;
        auto right = parseBitwiseOr();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseBitwiseOr() {
    auto expr = parseBitwiseXor();
    while (match(TokenType::Pipe)) {
        TokenType op = previous().type;
        auto right = parseBitwiseXor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseBitwiseXor() {
    auto expr = parseBitwiseAnd();
    while (match(TokenType::Caret)) {
        TokenType op = previous().type;
        auto right = parseBitwiseAnd();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseBitwiseAnd() {
    auto expr = parseEquality();
    while (match(TokenType::Ampersand)) {
        TokenType op = previous().type;
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto expr = parseComparison();
    while (match(TokenType::BangEqual) || match(TokenType::EqualEqual)) {
        TokenType op = previous().type;
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto expr = parseShift();
    while (match(TokenType::Greater) || match(TokenType::GreaterEqual) ||
           match(TokenType::Less) || match(TokenType::LessEqual)) {
        TokenType op = previous().type;
        auto right = parseShift();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseShift() {
    auto expr = parseTerm();
    while (match(TokenType::LessLess) || match(TokenType::GreaterGreater)) {
        TokenType op = previous().type;
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto expr = parseFactor();
    while (match(TokenType::Minus) || match(TokenType::Plus)) {
        TokenType op = previous().type;
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseFactor() {
    auto expr = parseUnary();
    while (match(TokenType::Slash) || match(TokenType::Star) || match(TokenType::Percent)) {
        TokenType op = previous().type;
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (match(TokenType::Bang) || match(TokenType::Minus) || match(TokenType::Tilde)) { return parseUnary(); }
    return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
    auto expr = parsePrimary();
    while (true) {
        if (match(TokenType::LParen)) {
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RParen)) {
                do { args.push_back(parseExpression()); } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expect ')'");
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
        } else if (match(TokenType::Dot)) {
            Token name = consume(TokenType::Identifier, "Expect property name");
             expr = std::make_unique<GetExpr>(std::move(expr), std::string(name.lexeme));
        } else if (match(TokenType::LBracket)) {
            auto index = parseExpression();
            consume(TokenType::RBracket, "Expect ']'");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
        } else { break; }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenType::K_False)) return std::make_unique<NumberExpr>(0);
    if (match(TokenType::K_True)) return std::make_unique<NumberExpr>(1);
    if (match(TokenType::K_Null)) return std::make_unique<NumberExpr>(0); 
    if (match(TokenType::Number)) return std::make_unique<NumberExpr>(std::stod(std::string(previous().lexeme)));
    if (match(TokenType::String)) {
        std::string lex = std::string(previous().lexeme);
        // Remove quotes
        return std::make_unique<StringExpr>(lex.substr(1, lex.length() - 2));
    }
    if (match(TokenType::Identifier)) return std::make_unique<VariableExpr>(std::string(previous().lexeme));
    
    if (match(TokenType::LParen)) {
        auto expr = parseExpression();
        consume(TokenType::RParen, "Expect ')'");
        return expr;
    }
    
    // Array Literal
    if (match(TokenType::LBracket)) {
        std::vector<std::unique_ptr<Expr>> elements;
        if (!check(TokenType::RBracket)) {
            do {
                elements.push_back(parseExpression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RBracket, "Expect ']' after array elements");
        return std::make_unique<ArrayExpr>(std::move(elements));
    }
    
    // Object Literal { key: value, key2: value2 }
    if (match(TokenType::LBrace)) {
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries;
        if (!check(TokenType::RBrace)) {
            do {
                Token key = consume(TokenType::Identifier, "Expect object key");
                consume(TokenType::Colon, "Expect ':' after key");
                auto value = parseExpression();
                entries.push_back({std::string(key.lexeme), std::move(value)});
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RBrace, "Expect '}' after object entries");
        return std::make_unique<ObjectExpr>(std::move(entries));
    }
    
    return nullptr;
}

} // namespace manifast
