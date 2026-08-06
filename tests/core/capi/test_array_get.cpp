#include <gtest/gtest.h>
#include "manifast/Runtime.h"

TEST(CAPITest, ArrayGetOOB) {
    Any* arr = manifast_create_array(0);
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->type, ANY_ARRAY);

    // Empty array: any positive index is OOB (1-based API)
    Any* oob = manifast_array_get(arr, 100.0);
    ASSERT_NE(oob, nullptr);
    EXPECT_EQ(oob->type, ANY_NIL);

    // Negative index (cast to large unsigned) is OOB
    Any* neg = manifast_array_get(arr, -1.0);
    ASSERT_NE(neg, nullptr);
    EXPECT_EQ(neg->type, ANY_NIL);

    // Index 0 is invalid (1-based)
    Any* zero = manifast_array_get(arr, 0.0);
    ASSERT_NE(zero, nullptr);
    EXPECT_EQ(zero->type, ANY_NIL);

    // Non-array receiver
    Any* not_arr = manifast_create_number(42.0);
    Any* invalid = manifast_array_get(not_arr, 1.0);
    ASSERT_NE(invalid, nullptr);
    EXPECT_EQ(invalid->type, ANY_NIL);
}

TEST(CAPITest, ArrayGetValid) {
    Any* arr = manifast_create_array(0);
    Any n1 = {ANY_NUMBER, 10.0, nullptr};
    Any n2 = {ANY_NUMBER, 20.0, nullptr};
    manifast_array_push(arr, &n1);
    manifast_array_push(arr, &n2);

    Any* v1 = manifast_array_get(arr, 1.0);
    ASSERT_NE(v1, nullptr);
    EXPECT_EQ(v1->type, ANY_NUMBER);
    EXPECT_DOUBLE_EQ(v1->number, 10.0);

    Any* v2 = manifast_array_get(arr, 2.0);
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ(v2->type, ANY_NUMBER);
    EXPECT_DOUBLE_EQ(v2->number, 20.0);

    Any* oob = manifast_array_get(arr, 3.0);
    ASSERT_NE(oob, nullptr);
    EXPECT_EQ(oob->type, ANY_NIL);
}
