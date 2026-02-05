#pragma once

#include "Token.h"
#include "SyntaxConfig.h"
#include <string_view>

namespace manifast {

class Lexer {
public:
    Lexer(std::string_view source, const SyntaxConfig& config = SyntaxConfig());

    Token nextToken();
    std::string_view getSource() const { return source; }

private:
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    void skipWhitespace();
    
    Token makeToken(TokenType type);
    Token errorToken(const char* message);

    // Scanners
    Token number();
    Token identifier();
    Token string();

private:
    std::string_view source;
    const SyntaxConfig& config;
    
    int start = 0;   // Start of current token
    int current = 0; // Current scanning position
    int line = 1;
    int column_start = 1; // Tracks column of token start
};

} // namespace manifast
