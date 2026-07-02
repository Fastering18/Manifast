// Unit tests for manifast::Parser::error() (src/lib/Parser.cpp).
//
// These tests focus on the caret-construction logic that was changed in this
// PR:
//   - int col = std::max(0, token.location.offset - start);
//   - caret.reserve(...) before building the caret line
//   - caret.append(length, '^') instead of a manual char-by-char loop
//
// Parser::error() is private, so it is exercised exclusively through the
// public API (Parser::parse() + Parser::setErrorCallback()), which is the
// same mechanism used by the rest of the codebase (see
// src/cmd/mifast/main.cpp) to observe syntax errors.

#include <gtest/gtest.h>

#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/SyntaxConfig.h"

#include <string>
#include <vector>

using namespace manifast;

namespace {

struct ParseErrorResult {
    bool hadError = false;
    std::vector<std::string> errors; // Full formatted message per error() call.
};

// Parses `source` and collects every formatted error message produced by
// Parser::error() via the error callback.
ParseErrorResult parseAndCaptureErrors(const std::string& source) {
    ParseErrorResult result;

    SyntaxConfig config;
    Lexer lexer(source, config);
    Parser parser(lexer);

    parser.setErrorCallback([&](const std::string& msg) {
        result.errors.push_back(msg);
    });

    parser.parse();
    result.hadError = parser.hadError();
    return result;
}

// Splits `full` on '\n'. Parser::error() always builds messages of the form:
//   "\n[ERROR SINTAKS] Baris L:O\n"
//   "  {sourceLine}\n"
//   "{caret}\n"
//   "{finalMsg}\n\n"
// which, when split on '\n', yields exactly 6 elements:
//   [0] "" (leading newline)
//   [1] "[ERROR SINTAKS] Baris L:O"
//   [2] "  {sourceLine}"
//   [3] "{caret}"
//   [4] "{finalMsg}"
//   [5] "" (trailing blank line)
std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (true) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string::npos) {
            if (pos < s.size()) lines.push_back(s.substr(pos));
            break;
        }
        lines.push_back(s.substr(pos, nl - pos));
        pos = nl + 1;
    }
    return lines;
}

} // namespace

// A single-line syntax error with no tabs: the caret must consist of exactly
// `col` alignment spaces followed by exactly `token.location.length` carets.
TEST(ParserErrorTest, SingleLineNoTabs_CaretAlignsWithTokenLength) {
    // "jika benar tutup" is missing the 'maka' keyword before the if-body,
    // so consume(K_Then, ...) fails on the 'tutup' token (offset 11, length 5).
    const std::string source = "jika benar tutup";

    ParseErrorResult result = parseAndCaptureErrors(source);
    ASSERT_TRUE(result.hadError);
    ASSERT_FALSE(result.errors.empty());

    std::vector<std::string> lines = splitLines(result.errors[0]);
    ASSERT_GE(lines.size(), 5u);

    EXPECT_EQ(lines[1], "[ERROR SINTAKS] Baris 1:11");
    EXPECT_EQ(lines[2].substr(2), "jika benar tutup");

    std::string expectedCaret = "  " + std::string(11, ' ') + std::string(5, '^');
    EXPECT_EQ(lines[3], expectedCaret);

    EXPECT_EQ(lines[4],
              "-> Diharapkan 'maka' setelah kondisi 'jika', ditemukan 'tutup'");
}

// Tab characters preceding the erroring token must be preserved verbatim in
// the caret line so that terminal tab-stops keep the caret aligned.
TEST(ParserErrorTest, TabsInSource_ArePreservedInCaretLine) {
    // Same structure as above, but spaces are replaced with tabs. Tabs count
    // as a single character for offset purposes, so the offsets are unchanged.
    const std::string source = "jika\tbenar\ttutup";

    ParseErrorResult result = parseAndCaptureErrors(source);
    ASSERT_TRUE(result.hadError);
    ASSERT_FALSE(result.errors.empty());

    std::vector<std::string> lines = splitLines(result.errors[0]);
    ASSERT_GE(lines.size(), 5u);

    EXPECT_EQ(lines[1], "[ERROR SINTAKS] Baris 1:11");
    EXPECT_EQ(lines[2].substr(2), "jika\tbenar\ttutup");

    // Indices 0-3 = "jika" (spaces), 4 = '\t', 5-9 = "benar" (spaces),
    // 10 = '\t', followed by 5 carets for "tutup".
    std::string expectedCaret =
        "  " + std::string("    ") + "\t" + std::string("     ") + "\t" + std::string(5, '^');
    EXPECT_EQ(lines[3], expectedCaret);

    EXPECT_EQ(lines[4],
              "-> Diharapkan 'maka' setelah kondisi 'jika', ditemukan 'tutup'");
}

// A zero-length token (an "Unexpected character" token produced by the
// Lexer) must fall back to a single caret character.
TEST(ParserErrorTest, ZeroLengthToken_FallsBackToSingleCaret) {
    // 'jika' is a valid first token; the '@' after it is rejected by the
    // Lexer, producing a TokenType::Error token with location.length == 0.
    const std::string source = "jika @";

    ParseErrorResult result = parseAndCaptureErrors(source);
    ASSERT_TRUE(result.hadError);
    ASSERT_FALSE(result.errors.empty());

    std::vector<std::string> lines = splitLines(result.errors[0]);
    ASSERT_GE(lines.size(), 5u);

    EXPECT_EQ(lines[1], "[ERROR SINTAKS] Baris 1:6");
    EXPECT_EQ(lines[2].substr(2), "jika @");

    std::string expectedCaret = "  " + std::string(6, ' ') + "^";
    EXPECT_EQ(lines[3], expectedCaret);

    EXPECT_EQ(lines[4],
              "-> Unexpected character., ditemukan 'Unexpected character.'");
}

// A long token must produce exactly as many carets as its length, verifying
// caret.append(length, '^') behaves correctly for large counts (this
// replaces a manual char-by-char loop in the original implementation).
TEST(ParserErrorTest, LongToken_CaretLengthMatchesTokenLength) {
    const std::string longIdentifier(50, 'x');
    const std::string source = "jika benar " + longIdentifier + " tutup";

    ParseErrorResult result = parseAndCaptureErrors(source);
    ASSERT_TRUE(result.hadError);
    ASSERT_FALSE(result.errors.empty());

    std::vector<std::string> lines = splitLines(result.errors[0]);
    ASSERT_GE(lines.size(), 5u);

    EXPECT_EQ(lines[1], "[ERROR SINTAKS] Baris 1:11");
    EXPECT_EQ(lines[2].substr(2), source);

    std::string expectedCaret = "  " + std::string(11, ' ') + std::string(50, '^');
    EXPECT_EQ(lines[3], expectedCaret);

    EXPECT_EQ(lines[4],
              "-> Diharapkan 'maka' setelah kondisi 'jika', ditemukan identitas");
}

// Boundary case: the erroring token sits at offset 0 (start of file), so
// `col` must be exactly 0 and the alignment loop must run zero iterations,
// producing a caret line with no leading spaces beyond the fixed 2-space
// prefix.
TEST(ParserErrorTest, TokenAtStartOfFile_ProducesZeroColumnCaret) {
    // "= 5;" starts with an invalid assignment target ('=' has no preceding
    // lvalue), which is reported via the dedicated assignment-target check.
    const std::string source = "= 5;";

    ParseErrorResult result = parseAndCaptureErrors(source);
    ASSERT_TRUE(result.hadError);
    ASSERT_EQ(result.errors.size(), 1u);

    std::vector<std::string> lines = splitLines(result.errors[0]);
    ASSERT_GE(lines.size(), 5u);

    EXPECT_EQ(lines[1], "[ERROR SINTAKS] Baris 1:0");
    EXPECT_EQ(lines[2].substr(2), "= 5;");

    // col == 0, so the caret line is just the 2-space prefix plus 1 caret.
    EXPECT_EQ(lines[3], "  ^");

    EXPECT_EQ(lines[4], "-> Lokasi assignment tidak sah., ditemukan '='");
}

// Multi-line input: the erroring token is on the second line. `start` (the
// beginning of the offending line within the full source buffer) is
// non-zero here, which exercises `col = token.location.offset - start` with
// a realistic, larger `start` value.
TEST(ParserErrorTest, MultiLineSource_CaretIsRelativeToOwningLine) {
    // Line 1 opens an if-block ("jika benar maka"); line 2 ("kembali 5") is
    // a valid statement, but the block is never closed with 'tutup', so the
    // final consume(K_End, ...) fails at EndOfFile (line 2, offset 25).
    const std::string source = "jika benar maka\nkembali 5";

    ParseErrorResult result = parseAndCaptureErrors(source);
    ASSERT_TRUE(result.hadError);
    ASSERT_FALSE(result.errors.empty());

    std::vector<std::string> lines = splitLines(result.errors[0]);
    ASSERT_GE(lines.size(), 5u);

    EXPECT_EQ(lines[1], "[ERROR SINTAKS] Baris 2:25");
    // Only line 2's content should be extracted, not the whole source.
    EXPECT_EQ(lines[2].substr(2), "kembali 5");

    std::string expectedCaret = "  " + std::string(9, ' ') + "^";
    EXPECT_EQ(lines[3], expectedCaret);

    EXPECT_EQ(lines[4],
              "-> Diharapkan 'tutup' setelah blok 'jika', ditemukan akhir file (EOF)");
}

// Negative/regression case: valid, fully closed syntax must not report any
// error and must never invoke the error callback.
TEST(ParserErrorTest, ValidSource_ProducesNoErrors) {
    const std::string source = "jika benar maka\nkembali 5\ntutup";

    ParseErrorResult result = parseAndCaptureErrors(source);
    EXPECT_FALSE(result.hadError);
    EXPECT_TRUE(result.errors.empty());
}