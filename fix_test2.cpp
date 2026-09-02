#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct ChartConfig {
    int padding = 20;
    int width = 800;
    int height = 600;
};
ChartConfig config_;
int heat_w_ = 100, heat_h_ = 100;
std::vector<double> heatmap_(10000, 0.5);
std::vector<uint8_t> framebuffer_(800 * 600 * 4);

uint32_t heatColor(double t) { return 0xFF0000FF; }

void render() {
    int w = 800, h = 600;
    double mn = 0.0, mx = 1.0;
        int pad = config_.padding;
        int pw = w - 2 * pad, ph = h - 2 * pad;

        if (pw > 0 && ph > 0) {
            int ph_div = std::max(1, ph);
            int pw_div = std::max(1, pw);
            double inv_diff = 1.0 / (mx - mn);

            std::vector<int> sx_cache(pw);
            for (int x = 0; x < pw; x++) {
                sx_cache[x] = std::min(heat_w_ - 1, x * heat_w_ / pw_div);
            }

            for (int y = 0; y < ph; y++) {
                int sy = std::min(heat_h_ - 1, y * heat_h_ / ph_div);
                size_t sy_offset = (size_t)sy * (size_t)heat_w_;

                size_t row_start_idx = ((size_t)(pad + y) * (size_t)w + pad) * 4;
                for (int x = 0; x < pw; x++) {
                    double t = (heatmap_[sy_offset + (size_t)sx_cache[x]] - mn) * inv_diff;
                    uint32_t c = heatColor(t);

                    size_t i = row_start_idx + x * 4;
                    framebuffer_[i]   = (c >> 24) & 0xFF;
                    framebuffer_[i+1] = (c >> 16) & 0xFF;
                    framebuffer_[i+2] = (c >> 8) & 0xFF;
                    framebuffer_[i+3] = 0xFF;
                }
            }
        }
    std::cout << "Done!" << std::endl;
}

int main() {
    render();
    return 0;
}
