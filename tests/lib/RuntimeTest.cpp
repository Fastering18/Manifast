#include <gtest/gtest.h>
#include "manifast/Runtime.h"

static int captured_plot_w = 0;
static int captured_plot_h = 0;
static bool callback_called = false;

static void mock_plot_show_callback(const uint8_t* rgba, int w, int h) {
    captured_plot_w = w;
    captured_plot_h = h;
    callback_called = true;
}

TEST(RuntimeTest, ManifastPlotForTest) {
    captured_plot_w = 0;
    captured_plot_h = 0;
    callback_called = false;

    manifast_set_plot_show_callback(mock_plot_show_callback);

    Any* y_arr = manifast_create_array(3);
    Any y_val1 = {ANY_NUMBER, 1.0, nullptr};
    Any y_val2 = {ANY_NUMBER, 2.0, nullptr};
    Any y_val3 = {ANY_NUMBER, 3.0, nullptr};
    manifast_array_push(y_arr, &y_val1);
    manifast_array_push(y_arr, &y_val2);
    manifast_array_push(y_arr, &y_val3);

    Any* cfg = manifast_create_object();
    Any title = {ANY_STRING, 0.0, mf_strdup("Test C++ Plot")};
    manifast_object_set(cfg, "title", &title);

    manifast_plot_for(y_arr, nullptr, cfg);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(captured_plot_w, 800);
    EXPECT_EQ(captured_plot_h, 600);

    manifast_set_plot_show_callback(nullptr);
}
