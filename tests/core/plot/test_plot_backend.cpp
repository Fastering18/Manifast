#include "manifast/PlotBackend.h"
#include <cassert>
#include <iostream>

using namespace manifast::plot;

int main() {
    PlotBackend plot;

    ChartConfig config;
    config.width = 100;
    config.height = 100;
    plot.setConfig(config);

    // Call renderChart to allocate the framebuffer.
    plot.renderChart();

    // Draw lines that go completely out of bounds.
    // Without bounds checks, this would write out of the framebuffer array and segfault.
    try {
        plot.drawLine(-100, -100, -50, -50, 0xFFFFFFFF);
        plot.drawLine(200, 200, 300, 300, 0xFFFFFFFF);
        plot.drawLine(-10, 50, 150, 50, 0xFFFFFFFF);
        plot.drawLine(50, -10, 50, 150, 0xFFFFFFFF);
    } catch (...) {
        std::cerr << "Exception thrown during out-of-bounds drawLine." << std::endl;
        return 1;
    }

    std::cout << "PlotBackend drawLine edge cases tested successfully." << std::endl;
    return 0;
}
