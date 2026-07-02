#include <gtest/gtest.h>
#include "manifast/VM/VM.h"
#include "manifast/VM/Compiler.h"
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/Runtime.h"

using namespace manifast;
using namespace manifast::vm;

TEST(VMTest, SetAndGetStackSize) {
    VM vm;
    // Default stack size should be large (e.g., 1048576)
    size_t defaultSize = vm.getStackSize();
    EXPECT_GT(defaultSize, 0);

    // Set a new stack size
    size_t newSize = 1024;
    vm.setStackSize(newSize);

    // Verify getter returns the new size
    EXPECT_EQ(vm.getStackSize(), newSize);
}

TEST(VMTest, StackOverflowError) {
    std::string source =
        "fungsi testStack()\n"
        "    testStack()\n"
        "tutup\n"
        "testStack()\n";

    SyntaxConfig config;
    Lexer lexer(source, config);
    Parser parser(lexer);
    auto statements = parser.parse();

    ASSERT_FALSE(parser.hadError());

    Chunk chunk;
    Compiler compiler;
    bool compiled = compiler.compile(statements, chunk);
    ASSERT_TRUE(compiled);

    VM vm;
    // Set a very small stack size to trigger stack overflow quickly
    vm.setStackSize(128); // Minimal stack size

    EXPECT_THROW({
        vm.interpret(&chunk, source);
    }, RuntimeError);

    chunk.free();
}
