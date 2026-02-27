#ifndef MANIFAST_PLOT_BACKEND_H
#define MANIFAST_PLOT_BACKEND_H

#include <vector>
#include <string>
#include <cstdint>

namespace manifast {
namespace plot {

struct Series {
    std::vector<double> x, y;
    std::string label;
    uint32_t color; // RGBA packed
};

struct ChartConfig {
    int width = 800;
    int height = 600;
    std::string title;
    std::string xlabel;
    std::string ylabel;
    uint32_t bg_color = 0xFFFFFFFF;
    uint32_t axis_color = 0x333333FF;
    uint32_t grid_color = 0xDDDDDDFF;
    int padding = 60;
};

enum class ChartType { Line, Scatter, Bar };

class PlotBackend {
public:
    void addSeries(const Series& s);
    void setConfig(const ChartConfig& c);

    bool saveToFile(const std::string& path, ChartType type = ChartType::Line);
    bool showWindow(ChartType type = ChartType::Line);
    
    void renderChart(ChartType type = ChartType::Line);
    const std::vector<uint8_t>& getFramebuffer() const { return framebuffer_; }
    int getWidth() const { return config_.width; }
    int getHeight() const { return config_.height; }
    void reset() { series_.clear(); framebuffer_.clear(); current_type_ = ChartType::Line; }
    void setChartType(ChartType type) { current_type_ = type; }
    ChartType getChartType() const { return current_type_; }

private:
    std::vector<Series> series_;
    ChartConfig config_;
    std::vector<uint8_t> framebuffer_;
    ChartType current_type_ = ChartType::Line;

    void render(ChartType type);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t color);
    void drawRect(int x, int y, int w, int h, uint32_t color);
    void drawCircle(int cx, int cy, int r, uint32_t color);
    void setPixel(int x, int y, uint32_t color);
    void computeAxes(double& xmin, double& xmax, double& ymin, double& ymax);
    void drawText(int x, int y, const std::string& text, uint32_t color, int scale = 1);
    int mapX(double val, double xmin, double xmax);
    int mapY(double val, double ymin, double ymax);

    static constexpr uint32_t PALETTE[] = {
        0xE74C3CFF, 0x3498DBFF, 0x2ECC71FF, 0xF39C12FF,
        0x9B59B6FF, 0x1ABC9CFF, 0xE67E22FF, 0x34495EFF
    };
};

} // namespace plot
} // namespace manifast

#endif
