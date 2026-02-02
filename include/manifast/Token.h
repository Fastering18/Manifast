#pragma once

#include <string_view>
#include <string>

namespace manifast {

enum class TokenType {
    // End of File
    EndOfFile,
    
    // Core Types
    Identifier,
    Number,
    String,
    
    // Keywords (Indonesian / Configurable mapped)
    // We keep generic names here to allow mapping "jika" -> K_If
    K_If,           // jika
    K_Then,         // maka
    K_Else,         // kecuali / sebaliknya
    K_ElseIf,       // kalau
    K_End,          // tutup
    K_Function,     // fungsi
    K_Return,       // kembali
    K_Var,          // var / let (variable)
    K_Const,        // konstan
    K_While,        // selagi / ketika
    K_For,          // untuk
    K_True,         // benar
    K_False,        // salah
    K_Null,         // nihil
    
    // New Keywords (Loop/Try)
    K_To,           // ke
    K_Step,         // langkah
    K_Do,           // lakukan
    K_Try,          // coba
    K_Catch,        // tangkap
    
    // Logical Ops (Keywords)
    K_And,          // dan
    K_Or,           // atau
    
    // Primitive Types (Future use)
    K_String,       // string
    K_Boolean,      // boolean
    K_Int32,        // int32
    

    
    // Operators
    Plus, Minus, Star, Slash, Percent,
    Equal, EqualEqual, Bang, BangEqual,
    Less, LessEqual, Greater, GreaterEqual,
    
    // Shorthand Assignment
    PlusEqual, MinusEqual, StarEqual, SlashEqual, PercentEqual,
    
    // Bitwise
    Ampersand, Pipe, Caret, Tilde,
    LessLess, GreaterGreater,
    
    // Delimiters
    LParen, RParen,     // ( )
    LBrace, RBrace,     // { }
    LBracket, RBracket, // [ ]
    Comma, Dot, Semicolon, Colon,
    
    // Error
    Error
};

struct SourceLocation {
    int line;
    int column;
    int length;
};

struct Token {
    TokenType type;
    std::string_view lexeme; // View into source std::string
    SourceLocation location;
};

// Helper for debugging/printing
inline std::string_view tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::EndOfFile: return "EOF";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Number: return "Number";
        case TokenType::String: return "String";
        case TokenType::K_If: return "If";
        case TokenType::K_Then: return "Then";
        case TokenType::K_Else: return "Else";
        case TokenType::K_ElseIf: return "ElseIf";
        case TokenType::K_End: return "End";
        case TokenType::K_Function: return "Function";
        case TokenType::K_Return: return "Return";
        case TokenType::K_Var: return "Var";
        case TokenType::K_Const: return "Const";
        case TokenType::K_While: return "While";
        case TokenType::K_For: return "For";
        case TokenType::K_True: return "True";
        case TokenType::K_False: return "False";
        case TokenType::K_Null: return "Null";
        case TokenType::K_To: return "To";
        case TokenType::K_Step: return "Step";
        case TokenType::K_Do: return "Do";
        case TokenType::K_Try: return "Try";
        case TokenType::K_Catch: return "Catch";
        case TokenType::K_And: return "And";
        case TokenType::K_Or: return "Or";
        case TokenType::K_String: return "StringToken";
        case TokenType::K_Boolean: return "BooleanToken";
        case TokenType::K_Int32: return "Int32Token";
        
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";
        
        case TokenType::Equal: return "=";
        case TokenType::EqualEqual: return "==";
        case TokenType::Bang: return "!";
        case TokenType::BangEqual: return "!=";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        
        case TokenType::PlusEqual: return "+=";
        case TokenType::MinusEqual: return "-=";
        case TokenType::StarEqual: return "*=";
        case TokenType::SlashEqual: return "/=";
        case TokenType::PercentEqual: return "%=";
        
        case TokenType::Ampersand: return "&";
        case TokenType::Pipe: return "|";
        case TokenType::Caret: return "^";
        case TokenType::Tilde: return "~";
        case TokenType::LessLess: return "<<";
        case TokenType::GreaterGreater: return ">>";
        
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::LBrace: return "{";
        case TokenType::RBrace: return "}";
        case TokenType::LBracket: return "[";
        case TokenType::RBracket: return "]";
        
        case TokenType::Comma: return ",";
        case TokenType::Dot: return ".";
        case TokenType::Semicolon: return ";";
        case TokenType::Colon: return ":";
        case TokenType::Error: return "Error";
        default: return "UnknownToken";
    }
}

} // namespace manifast
