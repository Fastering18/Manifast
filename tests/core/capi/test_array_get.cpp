#include <gtest/gtest.h>
#include "manifast/Runtime.h"

TEST(CAPITest, ArrayGetOOB) {
    Any* arr = manifast_create_array(0);
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->type, ANY_ARRAY);

    // Test out of bounds index (positive)
    Any* oob = manifast_array_get(arr, 100.0);
    EXPECT_NE(oob, nullptr);
    EXPECT_EQ(oob->type, ANY_NIL);

    // Test out of bounds index (negative)
    Any* neg = manifast_array_get(arr, -1.0);
    EXPECT_NE(neg, nullptr);
    EXPECT_EQ(neg->type, ANY_NIL);

    // Test with invalid array object
    Any* not_arr = manifast_create_number(42.0);
    Any* invalid = manifast_array_get(not_arr, 100.0);
    EXPECT_NE(invalid, nullptr);
    EXPECT_EQ(invalid->type, ANY_NIL);
}
