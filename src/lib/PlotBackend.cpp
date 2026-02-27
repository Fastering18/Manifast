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

// Minimal 5x7 Font (Subset: 0-9, A-Z, a-z, space, - , . , : )
static const uint8_t FONT_5X7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // space
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x7C, 0x12, 0x11, 0x12, 0x7C, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x09, 0x01, // F
    0x3E, 0x41, 0x49, 0x49, 0x7A, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x3F, 0x40, 0x38, 0x40, 0x3F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x07, 0x08, 0x70, 0x08, 0x07, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x00, 0x36, 0x36, 0x00, 0x00  // :
};

static const uint8_t* getCharBmp(char c) {
    if (c >= 'a' && c <= 'z') c -= ('a' - 'A'); // Simple lowercase map
    int idx = 0;
    if (c == ' ') idx = 0;
    else if (c >= '0' && c <= '9') idx = 1 + (c - '0');
    else if (c >= 'A' && c <= 'Z') idx = 11 + (c - 'A');
    else if (c == '-') idx = 37;
    else if (c == '.') idx = 38;
    else if (c == ':') idx = 39;
    return &FONT_5X7[idx * 5];
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

void PlotBackend::drawText(int x, int y, const std::string& text, uint32_t color, int scale) {
    int cur_x = x;
    for (char c : text) {
        const uint8_t* bmp = getCharBmp(c);
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (bmp[col] & (1 << row)) {
                    if (scale == 1) {
                        setPixel(cur_x + col, y + row, color);
                    } else {
                        drawRect(cur_x + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        cur_x += 6 * scale;
    }
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

    // Draw grid lines and labels
    for (int i = 0; i <= 5; i++) {
        double vx = xmin + (xmax - xmin) * i / 5.0;
        double vy = ymin + (ymax - ymin) * i / 5.0;
        int gx = px + pw * i / 5;
        int gy = py + ph - ph * i / 5;

        drawLine(gx, py, gx, py + ph, config_.grid_color);
        drawLine(px, gy, px + pw, gy, config_.grid_color);

        char buf[32];
        if (std::abs(vx) >= 1e6 || (std::abs(vx) > 0 && std::abs(vx) < 0.01))
            snprintf(buf, sizeof(buf), "%.1e", vx);
        else
            snprintf(buf, sizeof(buf), "%.2f", vx);
        drawText(gx - 10, py + ph + 10, buf, config_.axis_color);

        if (std::abs(vy) >= 1e6 || (std::abs(vy) > 0 && std::abs(vy) < 0.01))
            snprintf(buf, sizeof(buf), "%.1e", vy);
        else
            snprintf(buf, sizeof(buf), "%.2f", vy);
        drawText(px - 50, gy - 3, buf, config_.axis_color);
    }

    // Draw Title & Axis Labels
    if (!config_.title.empty()) {
        int title_scale = 2;
        int title_w = (int)config_.title.length() * 6 * title_scale;
        drawText(w / 2 - title_w / 2, 20, config_.title, config_.axis_color, title_scale);
    }
    if (!config_.xlabel.empty())
        drawText(w / 2 - (int)config_.xlabel.length() * 6 / 2, h - 25, config_.xlabel, config_.axis_color);
    if (!config_.ylabel.empty()) {
        // Simple vertical text not implemented, draw at top left for now
        drawText(10, py - 20, config_.ylabel, config_.axis_color);
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

