#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct ChartConfig {
    int width = 800;
    int height = 600;
    int padding = 20;
};

std::vector<uint8_t> framebuffer_;
std::vector<double> heatmap_;
int heat_w_ = 100, heat_h_ = 100;
ChartConfig config_;

uint32_t heatColor(double t) {
    // mock
    return (uint32_t)(t * 255) << 24;
}

void setPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= config_.width || y < 0 || y >= config_.height) return;
    int i = (y * config_.width + x) * 4;
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    framebuffer_[i] = r;
    framebuffer_[i+1] = g;
    framebuffer_[i+2] = b;
    framebuffer_[i+3] = 0xFF;
}

void render_old() {
    int w = config_.width, h = config_.height;
    double mn = 0, mx = 1;
    int pad = config_.padding;
    int pw = w - 2 * pad, ph = h - 2 * pad;

    for (int y = 0; y < ph; y++) {
        int sy = std::min(heat_h_ - 1, y * heat_h_ / std::max(1, ph));
        for (int x = 0; x < pw; x++) {
            int sx = std::min(heat_w_ - 1, x * heat_w_ / std::max(1, pw));
            double t = (heatmap_[(size_t)sy * (size_t)heat_w_ + (size_t)sx] - mn) / (mx - mn);
            setPixel(pad + x, pad + y, heatColor(t));
        }
    }
}

void render_new() {
    int w = config_.width, h = config_.height;
    double mn = 0, mx = 1;
    int pad = config_.padding;
    int pw = w - 2 * pad, ph = h - 2 * pad;

    int ph_div = std::max(1, ph);
    int pw_div = std::max(1, pw);
    double inv_diff = 1.0 / (mx - mn);
    uint32_t* ptr = reinterpret_cast<uint32_t*>(framebuffer_.data());

    std::vector<int> sx_cache(pw);
    for (int x = 0; x < pw; x++) {
        sx_cache[x] = std::min(heat_w_ - 1, x * heat_w_ / pw_div);
    }

    for (int y = 0; y < ph; y++) {
        int sy = std::min(heat_h_ - 1, y * heat_h_ / ph_div);
        size_t sy_offset = (size_t)sy * (size_t)heat_w_;
        uint32_t* row_ptr = ptr + (pad + y) * w + pad;
        for (int x = 0; x < pw; x++) {
            double t = (heatmap_[sy_offset + sx_cache[x]] - mn) * inv_diff;
            uint32_t c = heatColor(t);
            uint32_t pixel;
            uint8_t* p = reinterpret_cast<uint8_t*>(&pixel);
            p[0] = (c >> 24) & 0xFF;
            p[1] = (c >> 16) & 0xFF;
            p[2] = (c >> 8) & 0xFF;
            p[3] = 0xFF;
            row_ptr[x] = pixel;
        }
    }
}

int main() {
    config_.width = 1920;
    config_.height = 1080;
    framebuffer_.resize(config_.width * config_.height * 4);
    heatmap_.resize(heat_w_ * heat_h_, 0.5);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i=0; i<100; ++i) render_old();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Old: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << " ms\n";

    start = std::chrono::high_resolution_clock::now();
    for (int i=0; i<100; ++i) render_new();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "New: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << " ms\n";

    return 0;
}
