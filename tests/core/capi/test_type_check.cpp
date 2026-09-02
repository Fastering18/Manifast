#include <gtest/gtest.h>
#include "manifast/Runtime.h"

// Helper struct for tests
struct TypeCheckTest : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TypeCheckTest, EarlyReturns) {
    // Null value should return immediately without throwing
    EXPECT_NO_THROW(manifast_type_check(nullptr, ANY_NUMBER));

    // Expected type -1 should return immediately without throwing
    Any val;
    val.type = ANY_STRING;
    EXPECT_NO_THROW(manifast_type_check(&val, -1));
}

TEST_F(TypeCheckTest, ExactTypeMatch) {
    Any val;

    val.type = ANY_STRING;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_STRING));

    val.type = ANY_ARRAY;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_ARRAY));

    val.type = ANY_NIL;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_NIL));
}

TEST_F(TypeCheckTest, CrossCompatibleNumbers) {
    Any val;

    // Test ANY_NUMBER matching ANY_INT32
    val.type = ANY_NUMBER;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_INT32));

    // Test ANY_FLOAT64 matching ANY_NUMBER
    val.type = ANY_FLOAT64;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_NUMBER));

    // Test ANY_CHAR matching ANY_INT8
    val.type = ANY_CHAR;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_INT8));

    // Test ANY_INT32 matching ANY_FLOAT32
    val.type = ANY_INT32;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_FLOAT32));

    // Test ANY_CHAR matching ANY_CHAR
    val.type = ANY_CHAR;
    EXPECT_NO_THROW(manifast_type_check(&val, ANY_CHAR));
}

TEST_F(TypeCheckTest, TypeMismatchThrows) {
    Any val;

    // String vs Number
    val.type = ANY_STRING;
    EXPECT_THROW(manifast_type_check(&val, ANY_NUMBER), manifast::RuntimeError);

    // Array vs Object
    val.type = ANY_ARRAY;
    EXPECT_THROW(manifast_type_check(&val, ANY_OBJECT), manifast::RuntimeError);

    // Number vs String
    val.type = ANY_NUMBER;
    EXPECT_THROW(manifast_type_check(&val, ANY_STRING), manifast::RuntimeError);

    // Nil vs Boolean
    val.type = ANY_NIL;
    EXPECT_THROW(manifast_type_check(&val, ANY_BOOLEAN), manifast::RuntimeError);
}

TEST_F(TypeCheckTest, ErrorMessageContent) {
    Any val;
    val.type = ANY_STRING;

    try {
        manifast_type_check(&val, ANY_NUMBER);
        FAIL() << "Expected RuntimeError to be thrown";
    } catch (const manifast::RuntimeError& e) {
        EXPECT_STREQ(e.what(), "TypeError: Diharapkan tipe angka, tapi mendapat tipe string");
    } catch (...) {
        FAIL() << "Expected manifast::RuntimeError";
    }
}
