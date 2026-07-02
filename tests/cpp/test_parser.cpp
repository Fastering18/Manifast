#include "manifast/Parser.h"
#include "manifast/Lexer.h"
#include <iostream>
#include <string>

void test_error_callback() {
    std::string source = "lokal x = )"; // Invalid syntax
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer, source);

    std::string captured_error = "";
    parser.setErrorCallback([&captured_error](const std::string& msg) {
        captured_error = msg;
    });

    auto stmts = parser.parse();

    if (parser.hadError() && !captured_error.empty() && captured_error.find("[ERROR SINTAKS]") != std::string::npos) {
        std::cout << "Test passed: Error callback captured expected syntax error.\n";
    } else {
        std::cerr << "Test failed: Error callback did not capture the expected error.\n";
        std::cerr << "Had error flag: " << parser.hadError() << "\n";
        std::cerr << "Captured error message: '" << captured_error << "'\n";
        exit(1);
    }
}

int main() {
    std::cout << "Running Parser Tests...\n";
    test_error_callback();
    std::cout << "All tests passed!\n";
    return 0;
}
