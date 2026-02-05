#include "manifast/Parser.h"
#define DEBUG_VM
#include <iostream>
#include <cstdio> // for fflush

namespace manifast {

Parser::Parser(Lexer& lexer, std::string_view source) 
    : lexer(lexer), source(source.empty() ? lexer.getSource() : source), hasError(false) {
    advance(); // Load first token
}

Token Parser::advance() {
    previousToken = currentToken;
    currentToken = lexer.nextToken();
    
    if (currentToken.type == TokenType::Error) {
        error(currentToken, std::string(currentToken.lexeme));
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
    error(currentToken, message);
    if (currentToken.type == TokenType::EndOfFile) return currentToken;
    // Synchronize to next statement
    synchronize();
    return currentToken;
}

void Parser::error(Token token, const std::string& message) {
    hasError = true;
    
    std::string found;
    switch (token.type) {
        case TokenType::Number: found = "angka"; break;
        case TokenType::String: found = "string"; break;
        case TokenType::Identifier: found = "identitas"; break;
        case TokenType::EndOfFile: found = "akhir file (EOF)"; break;
        default: found = "'" + std::string(token.lexeme) + "'"; break;
    }

    fprintf(stderr, "\n[ERROR SINTAKS] Baris %d:%d\n", token.location.line, token.location.offset);
    
    // Find start of line in source
    int start = token.location.offset;
    while (start > 0 && source[start-1] != '\n') start--;
    
    // Find end of line
    int end = token.location.offset;
    while (end < (int)source.length() && source[end] != '\n') end++;
    
    std::string lineStr = std::string(source.substr(start, end - start));
    fprintf(stderr, "  %s\n", lineStr.c_str());
    
    // Caret
    std::string caret = "  ";
    int col = token.location.offset - start;
    for (int j = 0; j < col; j++) {
        if (lineStr[j] == '\t') caret += '\t';
        else caret += ' ';
    }
    for (int j = 0; j < (token.location.length > 0 ? token.location.length : 1); j++) caret += '^';
    
    fprintf(stderr, "%s\n", caret.c_str());
    fprintf(stderr, "-> %s, ditemukan %s\n\n", message.c_str(), found.c_str());
    fflush(stderr);
}

void Parser::synchronize() {
    if (debugMode) {
        fprintf(stderr, "[DEBUG] Parser Synchronizing from token: '%s' (type %d) at line %d\n", 
                std::string(currentToken.lexeme).c_str(), (int)currentToken.type, currentToken.location.line);
        fflush(stderr);
    }
    
    if (currentToken.type == TokenType::EndOfFile) return;
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
            case TokenType::K_Const:
            case TokenType::K_Class:
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
    int iterations = 0;
    while (currentToken.type != TokenType::EndOfFile) {
        if (iterations++ > 10000) {
            fprintf(stderr, "Kesalahan Kritis: Parser terjebak dalam loop tak terbatas.\n");
            break;
        }

        Token startToken = currentToken;
        if (debugMode) {
            fprintf(stderr, "[PARSER] Parsing statement at line %d (token: '%s')\n", currentToken.location.line, std::string(currentToken.lexeme).c_str());
            fflush(stderr);
        }
        auto stmt = parseStatement();
        if (stmt) {
            statements.push_back(std::move(stmt));
        }
        
        if (currentToken.type == TokenType::EndOfFile) break;

        // Safety: If no progress was made, synchronize
        // Use offset to distinguish between identical tokens at different positions
        if (currentToken.location.offset == startToken.location.offset) {
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
    if (match(TokenType::K_Class)) return parseClassStatement();
    if (match(TokenType::K_Return)) return parseReturnStatement();
    if (match(TokenType::K_Var)) return parseVarDeclaration();
    if (match(TokenType::K_Const)) return parseVarDeclaration();
    // Block
    if (match(TokenType::LBrace)) {
        Token open = previous();
        return makeNode<BlockStmt>(open, parseBlock());
    }
    if (match(TokenType::K_Do)) {
        Token keyword = previous();
        std::vector<std::unique_ptr<Stmt>> body = parseBlock();
        consume(TokenType::K_End, "Diharapkan 'tutup' setelah blok");
        return makeNode<BlockStmt>(keyword, std::move(body));
    }
    
    auto startToken = currentToken;
    auto expr = parseExpression();
    if (!expr) {
        consume(TokenType::Semicolon, "Diharapkan ekspresi atau titik koma");
        return nullptr;
    }
    if (match(TokenType::Semicolon)) { }
    return makeNode<ExprStmt>(startToken, std::move(expr));
}

std::unique_ptr<Stmt> Parser::parseClassStatement() {
    Token keyword = previous();
    Token name = consume(TokenType::Identifier, "Diharapkan nama kelas");
    consume(TokenType::K_Then, "Diharapkan 'maka' sebelum isi kelas");
    
    std::vector<std::unique_ptr<FunctionStmt>> methods;
    while (!check(TokenType::K_End) && !check(TokenType::EndOfFile)) {
        if (match(TokenType::K_Function)) {
            auto func_stmt = parseFunctionStatement();
            FunctionStmt* func = static_cast<FunctionStmt*>(func_stmt.get());
            // Inject 'self' as first parameter
            func->params.insert(func->params.begin(), "self");
            methods.push_back(std::unique_ptr<FunctionStmt>(static_cast<FunctionStmt*>(func_stmt.release())));
        } else {
             advance();
        }
    }
    
    consume(TokenType::K_End, "Diharapkan 'tutup' setelah isi kelas");
    return makeNode<ClassStmt>(keyword, std::string(name.lexeme), std::move(methods));
}

std::unique_ptr<Stmt> Parser::parseFunctionStatement() {
    Token keyword = previous();
    Token name = consume(TokenType::Identifier, "Diharapkan nama fungsi");
    consume(TokenType::LParen, "Diharapkan '(' setelah nama fungsi");
    
    std::vector<std::string> params;
    if (!check(TokenType::RParen)) {
        do {
            Token param = consume(TokenType::Identifier, "Diharapkan nama parameter");
            params.push_back(std::string(param.lexeme));
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Diharapkan ')' setelah parameter");
    
    Token body_start = currentToken;
    std::vector<std::unique_ptr<Stmt>> body = parseBlock(); 
    consume(TokenType::K_End, "Diharapkan 'tutup' setelah isi fungsi");
    
    return makeNode<FunctionStmt>(keyword, std::string(name.lexeme), std::move(params), makeNode<BlockStmt>(body_start, std::move(body)));
}

std::unique_ptr<Stmt> Parser::parseIfStatement() {
    Token keyword = previous();
    auto cond = parseExpression();
    consume(TokenType::K_Then, "Diharapkan 'maka' setelah kondisi 'jika'");
    
    Token then_start = currentToken;
    std::vector<std::unique_ptr<Stmt>> thenStmts = parseBlock();
    auto thenBranch = makeNode<BlockStmt>(then_start, std::move(thenStmts));
    
    std::unique_ptr<Stmt> elseBranch = nullptr;
    
    if (match(TokenType::K_ElseIf)) {
        elseBranch = parseIfChain();
    } else if (match(TokenType::K_Else)) {
        Token else_token = previous();
        std::vector<std::unique_ptr<Stmt>> elseStmts = parseBlock();
        elseBranch = makeNode<BlockStmt>(else_token, std::move(elseStmts));
    }
    
    consume(TokenType::K_End, "Diharapkan 'tutup' setelah blok 'jika'");
    return makeNode<IfStmt>(keyword, std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Stmt> Parser::parseIfChain() {
    auto startToken = currentToken;
    auto cond = parseExpression();
    consume(TokenType::K_Then, "Diharapkan 'maka' setelah kondisi 'kalau'");
    
    std::vector<std::unique_ptr<Stmt>> thenStmts = parseBlock();
    auto thenBranch = makeNode<BlockStmt>(startToken, std::move(thenStmts));
    
    std::unique_ptr<Stmt> elseBranch = nullptr;
    
    if (match(TokenType::K_ElseIf)) {
        elseBranch = parseIfChain();
    } else if (match(TokenType::K_Else)) {
        Token elseToken = previous();
        std::vector<std::unique_ptr<Stmt>> elseStmts = parseBlock();
        elseBranch = makeNode<BlockStmt>(elseToken, std::move(elseStmts));
    }
    
    return makeNode<IfStmt>(startToken, std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Stmt> Parser::parseWhileStatement() {
    Token keyword = previous();
    auto cond = parseExpression();
    consume(TokenType::K_Do, "Diharapkan 'lakukan' setelah kondisi 'selagi'"); 
    Token body_start = currentToken;
    std::vector<std::unique_ptr<Stmt>> bodyStmts = parseBlock();
    consume(TokenType::K_End, "Diharapkan 'tutup' setelah blok 'selagi'");
    return makeNode<WhileStmt>(keyword, std::move(cond), makeNode<BlockStmt>(body_start, std::move(bodyStmts)));
}

std::unique_ptr<Stmt> Parser::parseForStatement() {
    Token keyword = previous();
    Token varToken = consume(TokenType::Identifier, "Diharapkan nama variabel setelah 'untuk'");
    consume(TokenType::Equal, "Diharapkan '=' setelah nama variabel");
    auto start = parseExpression();
    
    consume(TokenType::K_To, "Diharapkan 'ke' setelah nilai awal");
    auto end = parseExpression();
    
    std::unique_ptr<Expr> step = nullptr;
    if (match(TokenType::K_Step)) {
        step = parseExpression();
    }
    
    consume(TokenType::K_Do, "Diharapkan 'lakukan' sebelum isi pengulangan");
    
    Token body_start = currentToken;
    std::vector<std::unique_ptr<Stmt>> bodyStmts = parseBlock();
    consume(TokenType::K_End, "Diharapkan 'tutup' setelah pengulangan 'untuk'");
    return makeNode<ForStmt>(keyword, std::string(varToken.lexeme), std::move(start), std::move(end), std::move(step), makeNode<BlockStmt>(body_start, std::move(bodyStmts)));
}

std::unique_ptr<Stmt> Parser::parseTryStatement() {
    Token keyword = previous();
    Token body_start = currentToken;
    std::vector<std::unique_ptr<Stmt>> tryStmts;
    while (!check(TokenType::K_Catch) && !check(TokenType::K_End) && !check(TokenType::EndOfFile)) {
        tryStmts.push_back(parseStatement());
    }
    
    std::unique_ptr<Stmt> catchBranch = nullptr;
    std::string catchVar = "";

    if (match(TokenType::K_Catch)) {
        Token catch_token = previous();
        if (check(TokenType::Identifier)) {
            Token varName = consume(TokenType::Identifier, "Diharapkan nama variabel eksepsi");
            catchVar = std::string(varName.lexeme);
            if (check(TokenType::K_Then)) advance(); 
        }
        Token catch_body_start = currentToken;
        std::vector<std::unique_ptr<Stmt>> catchStmts = parseBlock(); 
        catchBranch = makeNode<BlockStmt>(catch_body_start, std::move(catchStmts));
    }
    
    consume(TokenType::K_End, "Diharapkan 'tutup' setelah blok coba/tangkap");
    return makeNode<TryStmt>(keyword, makeNode<BlockStmt>(body_start, std::move(tryStmts)), catchVar, std::move(catchBranch));
}

std::unique_ptr<Stmt> Parser::parseReturnStatement() {
    Token keyword = previous();
    std::unique_ptr<Expr> value = nullptr;
    if (!check(TokenType::Semicolon) && !check(TokenType::K_End) && !check(TokenType::K_Else) && !check(TokenType::K_Catch)) {
        value = parseExpression();
    }
    match(TokenType::Semicolon); 
    return makeNode<ReturnStmt>(keyword, std::move(value));
}

std::unique_ptr<Stmt> Parser::parseVarDeclaration() {
    Token name = consume(TokenType::Identifier, "Diharapkan nama variabel");
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
             return makeNode<AssignExpr>(opToken, std::move(expr), std::move(value), op);
        }
        error(opToken, "Lokasi penugasan tidak sah."); 
    }
    
    return expr;
}

// LogicOr -> LogicAnd -> BitwiseOr -> BitwiseXor -> BitwiseAnd -> Equality -> Comparison -> Shift -> Term -> Factor

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (match(TokenType::K_Or)) {
        Token opToken = previous();
        TokenType op = opToken.type;
        auto right = parseLogicalAnd();
        expr = makeNode<BinaryExpr>(opToken, std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto expr = parseBitwiseOr();
    while (match(TokenType::K_And)) {
        Token opToken = previous();
        TokenType op = opToken.type;
        auto right = parseBitwiseOr();
        expr = makeNode<BinaryExpr>(opToken, std::move(expr), op, std::move(right));
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
    if (match(TokenType::Bang) || match(TokenType::Minus) || match(TokenType::Tilde)) {
        Token opToken = previous();
        TokenType op = opToken.type;
        auto right = parseUnary();
        return makeNode<UnaryExpr>(opToken, op, std::move(right));
    }
    return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
    auto expr = parsePrimary();
    while (true) {
        if (match(TokenType::LParen)) {
            Token open = previous();
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RParen)) {
                do { args.push_back(parseExpression()); } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Diharapkan ')'");
            expr = makeNode<CallExpr>(open, std::move(expr), std::move(args));
        } else if (match(TokenType::Dot)) {
            Token dotToken = previous();
            Token name = consume(TokenType::Identifier, "Diharapkan nama properti");
             expr = makeNode<GetExpr>(dotToken, std::move(expr), std::string(name.lexeme));
        } else if (match(TokenType::LBracket)) {
            Token open = previous();
            std::unique_ptr<Expr> index = nullptr;
            if (match(TokenType::Colon)) {
                // [:end]
                std::unique_ptr<Expr> end = nullptr;
                if (!check(TokenType::RBracket)) { // Check if there's an expression after ':'
                    end = parseExpression();
                }
                index = std::make_unique<SliceExpr>(nullptr, std::move(end));
            } else {
                auto first = parseExpression();
                if (match(TokenType::Colon)) {
                    // [start:] or [start:end]
                    std::unique_ptr<Expr> end = nullptr;
                    if (!check(TokenType::RBracket)) { // Check if there's an expression after ':'
                        end = parseExpression();
                    }
                    index = std::make_unique<SliceExpr>(std::move(first), std::move(end));
                } else {
                    // [index]
                    index = std::move(first);
                }
            }
            consume(TokenType::RBracket, "Diharapkan ']' setelah indeks");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseFunctionExpression() {
    consume(TokenType::LParen, "Expect '(' after 'fungsi'");
    
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
    
    return std::make_unique<FunctionExpr>(std::move(params), std::make_unique<BlockStmt>(std::move(body)));
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenType::K_False)) return makeNode<BoolExpr>(previous(), false);
    if (match(TokenType::K_True)) return makeNode<BoolExpr>(previous(), true);
    if (match(TokenType::K_Null)) return makeNode<NilExpr>(previous()); 
    if (match(TokenType::Number)) return makeNode<NumberExpr>(previous(), std::stod(std::string(previous().lexeme)));
    if (match(TokenType::String)) {
        Token str = previous();
        std::string lex = std::string(str.lexeme);
        // Remove quotes
        return makeNode<StringExpr>(str, lex.substr(1, lex.length() - 2));
    }
    if (match(TokenType::Identifier)) return makeNode<VariableExpr>(previous(), std::string(previous().lexeme));
    if (match(TokenType::K_Self)) return makeNode<VariableExpr>(previous(), "self");
    
    if (match(TokenType::LParen)) {
        auto expr = parseExpression();
        consume(TokenType::RParen, "Diharapkan ')'");
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
        consume(TokenType::RBracket, "Diharapkan ']' setelah elemen array");
        return std::make_unique<ArrayExpr>(std::move(elements));
    }
    
    // Object Literal { key: value, key2: value2 }
    if (match(TokenType::LBrace)) {
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries;
        if (!check(TokenType::RBrace)) {
            do {
                Token key = consume(TokenType::Identifier, "Diharapkan kunci objek");
                consume(TokenType::Colon, "Diharapkan ':' setelah kunci");
                auto value = parseExpression();
                entries.push_back({std::string(key.lexeme), std::move(value)});
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RBrace, "Diharapkan '}' setelah isi objek");
        return std::make_unique<ObjectExpr>(std::move(entries));
    }
    
    if (match(TokenType::K_Function)) {
        return parseFunctionExpression();
    }
    
    return nullptr;
}

} // namespace manifast
