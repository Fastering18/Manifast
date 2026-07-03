#include <gtest/gtest.h>

extern "C" {
    typedef void (*MfEventCallback)(int event_type, const char* data);
    void mf_set_event_callback(MfEventCallback cb);
}

void dummy_callback(int event_type, const char* data) {}

TEST(WasmBindingsTest, SetEventCallback) {
    // Basic test to verify the callback can be set and unset without crashing
    EXPECT_NO_THROW({
        mf_set_event_callback(dummy_callback);
        mf_set_event_callback(nullptr);
    });
}
