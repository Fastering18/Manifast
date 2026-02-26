#include "manifast/PlotBackend.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>


// Minimal PNG writer (raw RGBA -> uncompressed PNG)
// For production, consider stb_image_write.h
namespace {

static void writePNG(const char* path, const uint8_t* data, int w, int h) {
    // Write raw BMP instead (simpler, no zlib dependency)
    FILE* f = fopen(path, "wb");
    if (!f) return;

    int row_stride = w * 3;
    int padding = (4 - (row_stride % 4)) % 4;
    int data_size = (row_stride + padding) * h;
    int file_size = 54 + data_size;

    // BMP Header
    uint8_t header[54] = {};
    header[0] = 'B'; header[1] = 'M';
    memcpy(header + 2, &file_size, 4);
    int offset = 54; memcpy(header + 10, &offset, 4);
    int dib_size = 40; memcpy(header + 14, &dib_size, 4);
    memcpy(header + 18, &w, 4);
    int neg_h = -h; memcpy(header + 22, &neg_h, 4); // top-down
    uint16_t planes = 1; memcpy(header + 26, &planes, 2);
    uint16_t bpp = 24; memcpy(header + 28, &bpp, 2);
    memcpy(header + 34, &data_size, 4);

    fwrite(header, 1, 54, f);

    uint8_t pad_bytes[3] = {0, 0, 0};
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 4;
            uint8_t bgr[3] = {data[idx + 2], data[idx + 1], data[idx + 0]};
            fwrite(bgr, 1, 3, f);
        }
        if (padding > 0) fwrite(pad_bytes, 1, padding, f);
    }
    fclose(f);
}

} // anonymous namespace

namespace manifast {
namespace plot {

void PlotBackend::addSeries(const Series& s) {
    series_.push_back(s);
}

void PlotBackend::setConfig(const ChartConfig& c) {
    config_ = c;
}

void PlotBackend::setPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= config_.width || y < 0 || y >= config_.height) return;
    int i = (y * config_.width + x) * 4;
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;
    if (a == 0xFF) {
        framebuffer_[i] = r;
        framebuffer_[i+1] = g;
        framebuffer_[i+2] = b;
        framebuffer_[i+3] = 0xFF;
    } else if (a > 0) {
        float af = a / 255.0f;
        framebuffer_[i] = (uint8_t)(r * af + framebuffer_[i] * (1 - af));
        framebuffer_[i+1] = (uint8_t)(g * af + framebuffer_[i+1] * (1 - af));
        framebuffer_[i+2] = (uint8_t)(b * af + framebuffer_[i+2] * (1 - af));
        framebuffer_[i+3] = 0xFF;
    }
}

void PlotBackend::drawLine(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void PlotBackend::drawRect(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            setPixel(x + dx, y + dy, color);
}

void PlotBackend::drawCircle(int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r)
                setPixel(cx + x, cy + y, color);
}

void PlotBackend::computeAxes(double& xmin, double& xmax, double& ymin, double& ymax) {
    xmin = ymin = 1e18;
    xmax = ymax = -1e18;
    for (auto& s : series_) {
        for (double v : s.x) { xmin = std::min(xmin, v); xmax = std::max(xmax, v); }
        for (double v : s.y) { ymin = std::min(ymin, v); ymax = std::max(ymax, v); }
    }
    if (xmin == xmax) { xmin -= 1; xmax += 1; }
    if (ymin == ymax) { ymin -= 1; ymax += 1; }
    double xpad = (xmax - xmin) * 0.05, ypad = (ymax - ymin) * 0.05;
    xmin -= xpad; xmax += xpad;
    ymin -= ypad; ymax += ypad;
}

int PlotBackend::mapX(double val, double xmin, double xmax) {
    int plot_w = config_.width - 2 * config_.padding;
    return config_.padding + (int)((val - xmin) / (xmax - xmin) * plot_w);
}

int PlotBackend::mapY(double val, double ymin, double ymax) {
    int plot_h = config_.height - 2 * config_.padding;
    return config_.height - config_.padding - (int)((val - ymin) / (ymax - ymin) * plot_h);
}

void PlotBackend::render(ChartType type) {
    int w = config_.width, h = config_.height;
    framebuffer_.resize(w * h * 4);

    // Fill background
    uint8_t br = (config_.bg_color >> 24) & 0xFF;
    uint8_t bg = (config_.bg_color >> 16) & 0xFF;
    uint8_t bb = (config_.bg_color >> 8) & 0xFF;
    for (int i = 0; i < w * h; i++) {
        framebuffer_[i*4] = br;
        framebuffer_[i*4+1] = bg;
        framebuffer_[i*4+2] = bb;
        framebuffer_[i*4+3] = 0xFF;
    }

    double xmin, xmax, ymin, ymax;
    computeAxes(xmin, xmax, ymin, ymax);

    int px = config_.padding, py = config_.padding;
    int pw = w - 2*px, ph = h - 2*py;

    // Draw grid lines (5 horizontal + 5 vertical)
    for (int i = 0; i <= 5; i++) {
        int gx = px + pw * i / 5;
        int gy = py + ph * i / 5;
        drawLine(gx, py, gx, py + ph, config_.grid_color);
        drawLine(px, gy, px + pw, gy, config_.grid_color);
    }

    // Draw axes border
    drawLine(px, py, px, py + ph, config_.axis_color);
    drawLine(px, py + ph, px + pw, py + ph, config_.axis_color);

    // Draw data
    for (size_t si = 0; si < series_.size(); si++) {
        uint32_t color = series_[si].color;
        if (color == 0) color = PALETTE[si % 8];
        auto& sx = series_[si].x;
        auto& sy = series_[si].y;
        size_t n = std::min(sx.size(), sy.size());

        if (type == ChartType::Line) {
            for (size_t i = 1; i < n; i++) {
                int x0 = mapX(sx[i-1], xmin, xmax), y0 = mapY(sy[i-1], ymin, ymax);
                int x1 = mapX(sx[i], xmin, xmax),   y1 = mapY(sy[i], ymin, ymax);
                drawLine(x0, y0, x1, y1, color);
                // Thicken
                drawLine(x0, y0-1, x1, y1-1, color);
                drawLine(x0, y0+1, x1, y1+1, color);
            }
        } else if (type == ChartType::Scatter) {
            for (size_t i = 0; i < n; i++) {
                int px_ = mapX(sx[i], xmin, xmax);
                int py_ = mapY(sy[i], ymin, ymax);
                drawCircle(px_, py_, 4, color);
            }
        } else if (type == ChartType::Bar) {
            int bar_w = std::max(2, pw / (int)(n * series_.size() + (int)n) );
            for (size_t i = 0; i < n; i++) {
                int bx = mapX(sx[i], xmin, xmax) - bar_w/2 + (int)si * (bar_w + 1);
                int by_bottom = mapY(0, ymin, ymax);
                int by_top = mapY(sy[i], ymin, ymax);
                if (by_top > by_bottom) std::swap(by_top, by_bottom);
                drawRect(bx, by_top, bar_w, by_bottom - by_top, color);
            }
        }
    }
}

void PlotBackend::renderChart(ChartType type) {
    render(type);
}

bool PlotBackend::saveToFile(const std::string& path, ChartType type) {
    render(type);
    writePNG(path.c_str(), framebuffer_.data(), config_.width, config_.height);
    return true;
}

bool PlotBackend::showWindow(ChartType type) {
#ifdef __EMSCRIPTEN__
    return false; // No window in WASM — use save() instead
#else
    render(type);
    std::string tmpPath = "manifast_plot_tmp.bmp";
    writePNG(tmpPath.c_str(), framebuffer_.data(), config_.width, config_.height);
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + tmpPath + "\"";
#elif __APPLE__
    std::string cmd = "open \"" + tmpPath + "\"";
#else
    std::string cmd = "xdg-open \"" + tmpPath + "\" &";
#endif
    return system(cmd.c_str()) == 0;
#endif
}

constexpr uint32_t PlotBackend::PALETTE[];

} // namespace plot
} // namespace manifast

