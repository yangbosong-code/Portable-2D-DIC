#include "preview_view.hpp"

#include <QApplication>
#include <QImage>
#include <QPainter>

#include <iostream>
#include <set>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    auto preview = std::make_shared<p2dic::PreviewFrame>();
    preview->width = 160;
    preview->height = 120;
    preview->sequence = 5;
    preview->pixels.resize(160 * 120);
    for (std::uint32_t y = 0; y < preview->height; ++y) {
        for (std::uint32_t x = 0; x < preview->width; ++x) {
            preview->pixels[static_cast<std::size_t>(y) * preview->width + x] =
                static_cast<std::uint8_t>((x + y) & 0xFFU);
        }
    }
    PreviewView view;
    view.resize(500, 360);
    view.setPreview(preview);
    QImage image(view.size(), QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    view.render(&painter);
    painter.end();
    std::set<QRgb> colors;
    for (int y = 0; y < image.height(); y += 5) {
        for (int x = 0; x < image.width(); x += 5) colors.insert(image.pixel(x, y));
    }
    if (colors.size() < 20) {
        std::cerr << "Preview view rendered an unexpectedly uniform image\n";
        return 1;
    }
    return 0;
}
