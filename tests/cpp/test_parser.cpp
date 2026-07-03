#include <gtest/gtest.h>
#include "manifast/Parser.h"
#include "manifast/Lexer.h"
#include <string>

TEST(ParserTest, CustomErrorCallback) {
    std::string source = "lokal x = )"; // Invalid syntax
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer, source);

    std::string captured_error = "";
    parser.setErrorCallback([&captured_error](const std::string& msg) {
        captured_error = msg;
    });

    auto stmts = parser.parse();

    EXPECT_TRUE(parser.hadError());
    EXPECT_FALSE(captured_error.empty());
    EXPECT_NE(captured_error.find("[ERROR SINTAKS]"), std::string::npos);
}
