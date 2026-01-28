#include "manifast/Lexer.h"
#include <cctype>

namespace manifast {

Lexer::Lexer(std::string_view source, const SyntaxConfig& config)
    : source(source), config(config) {}

char Lexer::advance() {
    current++;
    return source[current - 1];
}

char Lexer::peek() const {
    if (current >= source.length()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

bool Lexer::match(char expected) {
    if (peek() != expected) return false;
    current++;
    return true;
}

void Lexer::skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                line++;
                advance();
                // Reset column tracking if we were doing it granularly
                break;
            case '-':
                if (peekNext() == '-') {
                    advance(); // Consume 1st '-'
                    advance(); // Consume 2nd '-'
                    
                    // Check for multi-line comment --[[
                    if (peek() == '[' && peekNext() == '[') {
                        advance(); advance(); // consume [[
                        // Skip until ]]
                        while (!(peek() == ']' && peekNext() == ']') && peek() != '\0') {
                            if (peek() == '\n') line++;
                            advance();
                        }
                        if (peek() != '\0') { advance(); advance(); } // Consume ]]
                    } else {
                        // Single line comment until newline
                        while (peek() != '\n' && peek() != '\0') advance();
                    }
                } else {
                    return; // Standard minus token handling will occur in nextToken() via switch case usage if we returned here? 
                    // Wait, skipWhitespace is void. 
                    // Standard Lexer structure: skipWhitespace() is called first. 
                    // If it encounters '-', it checks if it's start of comment. 
                    // If NOT comment, it should RETURN so nextToken can process it as Minus.
                    return;
                }
                break;
            default:
                return;
        }
    }
}

Token Lexer::makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.lexeme = source.substr(start, current - start);
    token.location = {line, 0, current - start}; // Simplified column for now
    return token;
}

Token Lexer::errorToken(const char* message) {
    Token token;
    token.type = TokenType::Error;
    token.lexeme = std::string_view(message); // Ideally point to error string
    token.location = {line, 0, 0};
    return token;
}

Token Lexer::nextToken() {
    skipWhitespace();
    start = current;

    if (current >= source.length()) return makeToken(TokenType::EndOfFile);

    char c = advance();

    if (isalpha(c) || c == '_') return identifier();
    if (isdigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TokenType::LParen);
        case ')': return makeToken(TokenType::RParen);
        case '{': return makeToken(TokenType::LBrace);
        case '}': return makeToken(TokenType::RBrace);
        case ';': return makeToken(TokenType::Semicolon);
        case ':': return makeToken(TokenType::Colon);
        case ',': return makeToken(TokenType::Comma);
        case '.': return makeToken(TokenType::Dot);
        case '-': 
            if (match('=')) return makeToken(TokenType::MinusEqual);
            // Handle comments --
            if (peek() == '-') {
                advance(); 
                if (peek() == '[' && peekNext() == '[') {
                    advance(); advance(); 
                    while (!(peek() == ']' && peekNext() == ']') && peek() != '\0') {
                        if (peek() == '\n') line++;
                        advance();
                    }
                    if (peek() != '\0') { advance(); advance(); }
                } else {
                    while (peek() != '\n' && peek() != '\0') advance();
                }
                // Recursive call or loop? We need to return next token.
                return nextToken();
            }
            return makeToken(TokenType::Minus);

        case '+': return makeToken(match('=') ? TokenType::PlusEqual : TokenType::Plus);
        case '/': return makeToken(match('=') ? TokenType::SlashEqual : TokenType::Slash);
        case '*': return makeToken(match('=') ? TokenType::StarEqual : TokenType::Star);
        case '%': return makeToken(match('=') ? TokenType::PercentEqual : TokenType::Percent);
        
        case '&': return makeToken(TokenType::Ampersand);
        case '|': return makeToken(TokenType::Pipe);
        case '^': return makeToken(TokenType::Caret);
        case '~': return makeToken(TokenType::Tilde);
        
        case '!': return makeToken(match('=') ? TokenType::BangEqual : TokenType::Bang);
        case '=': return makeToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
        case '<': 
            if (match('<')) return makeToken(TokenType::LessLess);
            return makeToken(match('=') ? TokenType::LessEqual : TokenType::Less);
        case '>': 
            if (match('>')) return makeToken(TokenType::GreaterGreater);
            return makeToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater);
        

    }

    return errorToken("Unexpected character.");
}

Token Lexer::identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    
    std::string_view text = source.substr(start, current - start);
    TokenType type = config.lookupKeyword(text);
    
    return makeToken(type);
}

Token Lexer::number() {
    // 1. Check for Base prefixes (0x, 0b, 0o)
    if (peek() == '0') {
        if (peekNext() == 'x' || peekNext() == 'X') {
            advance(); advance(); // consume 0x
            while (isxdigit(peek()) || peek() == '_') advance();
            return makeToken(TokenType::Number);
        }
        if (peekNext() == 'b' || peekNext() == 'B') {
            advance(); advance(); // consume 0b
            while (peek() == '0' || peek() == '1' || peek() == '_') advance();
            return makeToken(TokenType::Number);
        }
        if (peekNext() == 'o' || peekNext() == 'O') { // Octal standard
            advance(); advance(); 
            while ((peek() >= '0' && peek() <= '7') || peek() == '_') advance();
            return makeToken(TokenType::Number);
        }
    }

    // 2. Decimal / Float
    while (isdigit(peek()) || peek() == '_') advance();

    // Fraction
    if (peek() == '.' && isdigit(peekNext())) {
        advance(); // consume Dot
        while (isdigit(peek()) || peek() == '_') advance();
    }

    // Scientific Notation (1.2e+3)
    if (peek() == 'e' || peek() == 'E') {
        advance(); // consume 'e'
        if (peek() == '+' || peek() == '-') advance();
        while (isdigit(peek()) || peek() == '_') advance();
    }

    return makeToken(TokenType::Number);
}

Token Lexer::string() {
    while (peek() != '"' && peek() != '\0') {
        if (peek() == '\n') line++;
        advance();
    }

    if (peek() == '\0') return errorToken("Unterminated string.");

    advance(); // Closing quote
    return makeToken(TokenType::String);
}

} // namespace manifast
