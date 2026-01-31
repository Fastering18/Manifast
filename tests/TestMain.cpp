#include <gtest/gtest.h>
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/Token.h"

using namespace manifast;

TEST(LexerTest, BasicTokens) {
    SyntaxConfig config;
    Lexer lexer("lokal x = 10", config);
    
    EXPECT_EQ(lexer.nextToken().type, TokenType::K_Var);
    EXPECT_EQ(lexer.nextToken().type, TokenType::Identifier);
    EXPECT_EQ(lexer.nextToken().type, TokenType::Equal);
    EXPECT_EQ(lexer.nextToken().type, TokenType::Number);
    EXPECT_EQ(lexer.nextToken().type, TokenType::EndOfFile);
}

TEST(LexerTest, Comments) {
    SyntaxConfig config;
    Lexer lexer("-- baris tunggal\n10 --[[ baris\nbanyak ]]", config);
    
    EXPECT_EQ(lexer.nextToken().type, TokenType::Number);
    EXPECT_EQ(lexer.nextToken().type, TokenType::EndOfFile);
}

TEST(ParserTest, BasicExpression) {
    SyntaxConfig config;
    Lexer lexer("1 + 2 * 3", config);
    Parser parser(lexer);
    
    auto stmts = parser.parse();
    ASSERT_EQ(stmts.size(), 1);
    // Verified implicitly by not crashing and returning correct structure count
}

TEST(ParserTest, IfStatement) {
    SyntaxConfig config;
    Lexer lexer("jika x == 10 maka kembali benar tutup", config);
    Parser parser(lexer);
    
    auto stmts = parser.parse();
    ASSERT_EQ(stmts.size(), 1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
