#include <iostream>
#include <vector>

void render() {
    int w = 100, h = 100;
    int pad = 60; // large padding makes pw/ph negative
    int pw = w - 2 * pad, ph = h - 2 * pad;

    if (pw <= 0 || ph <= 0) {
        std::cout << "Skipping rendering because plot area is too small" << std::endl;
        return;
    }

    std::vector<int> sx_cache(pw);
    for (int x = 0; x < pw; x++) {
        sx_cache[x] = x;
    }
    std::cout << "Rendered successfully" << std::endl;
}

int main() {
    render();
    return 0;
}
