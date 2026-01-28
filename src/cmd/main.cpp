#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/AST.h"

// --- Helpers ---

void printTokens(const std::string& source) {
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Token token;
    do {
        token = lexer.nextToken();
        std::cout << "[" << manifast::tokenTypeToString(token.type) << "] '" << token.lexeme << "'\n";
    } while (token.type != manifast::TokenType::EndOfFile);
}

void parseAndVisualize(const std::string& source) {
    // For verifying Parser (Stage 2)
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer);
    
    try {
        auto statements = parser.parse();
        std::cout << "Parsed " << statements.size() << " statements successfully.\n";
    } catch (...) {
        std::cerr << "Parse Error.\n";
    }
}

void runREPL() {
    std::cout << "Manifast 0.1.0 (REPL)\n";
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "keluar") break; 
        
        printTokens(line);
        // parseAndVisualize(line); // Enable when Parser is fully ready
    }
}

void runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Could not open file: " << path << "\n";
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    std::cout << "--- Tokens ---\n";
    printTokens(content);
    std::cout << "--- Parse (Check) ---\n";
    parseAndVisualize(content);
}

// --- Entry Point ---

int main(int argc, char *argv[]) {
    // TODO: argument parsing library integration
    if (argc > 1) {
        runFile(argv[1]);
    } else {
        runREPL();
    }
    return 0;
}
