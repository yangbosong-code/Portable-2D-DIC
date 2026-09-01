#include "field_view.hpp"

#include <QApplication>
#include <QImage>
#include <QPainter>

#include <iostream>
#include <set>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    auto result = std::make_shared<p2dic::DicResult>();
    result->points = {
        p2dic::DicPoint{10, 10, -1.0F, 0.0F, 0.9F, true},
        p2dic::DicPoint{20, 10, 0.0F, 0.0F, 0.9F, true},
        p2dic::DicPoint{30, 10, 1.0F, 0.0F, 0.9F, true},
        p2dic::DicPoint{10, 20, -0.5F, 0.0F, 0.9F, true},
        p2dic::DicPoint{20, 20, 0.5F, 0.0F, 0.9F, true},
        p2dic::DicPoint{30, 20, 1.5F, 0.0F, 0.9F, true}};
    FieldView view;
    view.resize(500, 300);
    view.setResult(result);
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
        std::cerr << "Field view rendered an unexpectedly uniform image\n";
        return 1;
    }
    return 0;
}
