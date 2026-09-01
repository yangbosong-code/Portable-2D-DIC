#include "signal_view.hpp"

#include <QApplication>
#include <QImage>
#include <QPainter>

#include <cmath>
#include <iostream>
#include <numbers>
#include <set>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    SignalView view;
    view.resize(600, 360);
    for (int index = 0; index < 64; ++index) {
        auto result = std::make_shared<p2dic::DicResult>();
        result->frame_timestamp_ns = static_cast<std::uint64_t>(index) * 50'000'000ULL;
        const float value = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 5.0 * index / 20.0));
        result->points.push_back(p2dic::DicPoint{10, 10, value, 0.0F, 1.0F, true});
        view.appendResult(result);
    }
    QImage image(view.size(), QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    view.render(&painter);
    painter.end();
    std::set<QRgb> colors;
    for (int y = 0; y < image.height(); y += 4) {
        for (int x = 0; x < image.width(); x += 4) colors.insert(image.pixel(x, y));
    }
    if (colors.size() < 8) {
        std::cerr << "Signal/FFT view rendered an unexpectedly uniform image\n";
        return 1;
    }
    return 0;
}
