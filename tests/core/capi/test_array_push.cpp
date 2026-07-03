#include <gtest/gtest.h>
#include "manifast/Runtime.h"

TEST(CAPITest, ArrayPush) {
    Any* arr = manifast_create_array(0);
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->type, ANY_ARRAY);

    ManifastArray* internal_arr = (ManifastArray*)arr->ptr;
    EXPECT_EQ(internal_arr->size, 0);
    EXPECT_EQ(internal_arr->capacity, 4);

    Any* num = manifast_create_number(42.0);
    manifast_array_push(arr, num);

    EXPECT_EQ(internal_arr->size, 1);

    // Array indices in manifast start at 1
    Any* res = manifast_array_get(arr, 1.0);
    EXPECT_EQ(res->type, ANY_NUMBER);
    EXPECT_EQ(res->number, 42.0);

    // push more elements to exceed initial capacity and test scaling
    for (int i = 0; i < 4; i++) {
        manifast_array_push(arr, manifast_create_number(i));
    }

    EXPECT_EQ(internal_arr->size, 5);
    // capacity should have scaled
    EXPECT_EQ(internal_arr->capacity, 8);
}
