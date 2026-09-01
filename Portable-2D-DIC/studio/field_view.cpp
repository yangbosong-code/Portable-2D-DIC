#include "field_view.hpp"

#include <QColor>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool pointValid(const p2dic::DicPoint& point, FieldView::Component component) {
    switch (component) {
        case FieldView::Component::exx:
        case FieldView::Component::eyy:
        case FieldView::Component::exy:
            return point.strain_valid;
        case FieldView::Component::u:
        case FieldView::Component::v:
        case FieldView::Component::quality:
            return point.valid;
    }
    return false;
}

double pointValue(const p2dic::DicPoint& point, FieldView::Component component) {
    switch (component) {
        case FieldView::Component::u: return point.u;
        case FieldView::Component::v: return point.v;
        case FieldView::Component::exx: return point.exx;
        case FieldView::Component::eyy: return point.eyy;
        case FieldView::Component::exy: return point.exy;
        case FieldView::Component::quality: return point.quality;
    }
    return 0.0;
}

QString componentName(FieldView::Component component, const QString& displacement_unit) {
    switch (component) {
        case FieldView::Component::u: return QStringLiteral("u [%1]").arg(displacement_unit);
        case FieldView::Component::v: return QStringLiteral("v [%1]").arg(displacement_unit);
        case FieldView::Component::exx: return QStringLiteral("exx");
        case FieldView::Component::eyy: return QStringLiteral("eyy");
        case FieldView::Component::exy: return QStringLiteral("exy");
        case FieldView::Component::quality: return QStringLiteral("quality");
    }
    return {};
}

QColor fieldColor(double normalized) {
    const double value = std::clamp(normalized, 0.0, 1.0);
    auto blend = [](const QColor& from, const QColor& to, double amount) {
        return QColor::fromRgbF(
            from.redF() + (to.redF() - from.redF()) * amount,
            from.greenF() + (to.greenF() - from.greenF()) * amount,
            from.blueF() + (to.blueF() - from.blueF()) * amount);
    };
    const QColor blue(20, 35, 180);
    const QColor cyan(25, 210, 220);
    const QColor yellow(250, 225, 35);
    const QColor red(205, 25, 35);
    if (value < 1.0 / 3.0) return blend(blue, cyan, value * 3.0);
    if (value < 2.0 / 3.0) return blend(cyan, yellow, (value - 1.0 / 3.0) * 3.0);
    return blend(yellow, red, (value - 2.0 / 3.0) * 3.0);
}

}  // namespace

FieldView::FieldView(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
}

void FieldView::setDisplacementScale(double scale, const QString& unit) {
    if (!std::isfinite(scale) || scale <= 0.0 || unit.isEmpty()) return;
    displacement_scale_ = scale;
    displacement_unit_ = unit;
    update();
}

void FieldView::setResult(std::shared_ptr<const p2dic::DicResult> result) {
    result_ = std::move(result);
    update();
}

void FieldView::setComponent(Component component) {
    component_ = component;
    update();
}

void FieldView::setComponentIndex(int index) {
    if (index >= 0 && index <= static_cast<int>(Component::quality)) {
        setComponent(static_cast<Component>(index));
    }
}

QSize FieldView::minimumSizeHint() const { return QSize(480, 280); }

void FieldView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(28, 31, 36));
    if (!result_ || result_->points.empty()) {
        painter.setPen(QColor(190, 195, 205));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("等待实时结果 / Waiting for live result"));
        return;
    }

    int min_x = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int min_y = std::numeric_limits<int>::max();
    int max_y = std::numeric_limits<int>::min();
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < result_->points.size(); ++index) {
        const auto& point = result_->points[index];
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
        if (!pointValid(point, component_)) continue;
        double value = pointValue(point, component_);
        if (component_ == Component::u || component_ == Component::v) {
            value *= displacement_scale_;
        }
        if (!std::isfinite(value)) continue;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        painter.setPen(QColor(220, 170, 90));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("当前分量没有有效点 / No valid points for this component"));
        return;
    }
    if (maximum - minimum < 1e-12) {
        const double padding = std::max(std::abs(maximum) * 0.05, 1e-6);
        minimum -= padding;
        maximum += padding;
    }

    int step_x = 1;
    int step_y = 1;
    for (std::size_t index = 1; index < result_->points.size(); ++index) {
        const auto& previous = result_->points[index - 1];
        const auto& current = result_->points[index];
        if (current.y == previous.y && current.x > previous.x) {
            step_x = current.x - previous.x;
            break;
        }
    }
    for (std::size_t index = 1; index < result_->points.size(); ++index) {
        const auto& previous = result_->points[index - 1];
        const auto& current = result_->points[index];
        if (current.y > previous.y) {
            step_y = current.y - previous.y;
            break;
        }
    }

    const QRectF plot = QRectF(rect()).adjusted(12.0, 22.0, -78.0, -14.0);
    const double span_x = std::max(1.0, static_cast<double>(max_x - min_x + step_x));
    const double span_y = std::max(1.0, static_cast<double>(max_y - min_y + step_y));
    const double cell_width = std::max(1.0, plot.width() * step_x / span_x + 0.5);
    const double cell_height = std::max(1.0, plot.height() * step_y / span_y + 0.5);
    const int columns = std::max(1, (max_x - min_x) / step_x + 1);
    const int rows = std::max(1, (max_y - min_y) / step_y + 1);
    QImage field_image(columns, rows, QImage::Format_RGB32);
    field_image.fill(QColor(75, 78, 85));
    for (std::size_t index = 0; index < result_->points.size(); ++index) {
        const auto& point = result_->points[index];
        QColor color(75, 78, 85);
        if (pointValid(point, component_)) {
            double value = pointValue(point, component_);
            if (component_ == Component::u || component_ == Component::v) {
                value *= displacement_scale_;
            }
            if (std::isfinite(value)) color = fieldColor((value - minimum) / (maximum - minimum));
        }
        const int column = (point.x - min_x) / step_x;
        const int row = (point.y - min_y) / step_y;
        if (column >= 0 && column < columns && row >= 0 && row < rows) {
            field_image.setPixelColor(column, row, color);
        }
    }
    painter.drawImage(plot, field_image);
    if (selected_index_ >= 0 &&
        static_cast<std::size_t>(selected_index_) < result_->points.size()) {
        const auto& selected = result_->points[static_cast<std::size_t>(selected_index_)];
        const double px = plot.left() + (selected.x - min_x) * plot.width() / span_x;
        const double py = plot.top() + (selected.y - min_y) * plot.height() / span_y;
        painter.setPen(QPen(QColor(255, 255, 255), 2.0));
        painter.drawRect(QRectF(px, py, cell_width, cell_height));
    }
    painter.setPen(QColor(205, 210, 218));
    painter.drawRect(plot);
    painter.drawText(QRectF(plot.left(), 2.0, plot.width(), 18.0),
                     Qt::AlignCenter, componentName(component_, displacement_unit_));

    const QRectF legend(plot.right() + 18.0, plot.top(), 16.0, plot.height());
    for (int y = 0; y < static_cast<int>(legend.height()); ++y) {
        const double normalized = 1.0 - y / std::max(1.0, legend.height() - 1.0);
        painter.setPen(fieldColor(normalized));
        painter.drawLine(QPointF(legend.left(), legend.top() + y),
                         QPointF(legend.right(), legend.top() + y));
    }
    painter.setPen(QColor(205, 210, 218));
    painter.drawRect(legend);
    painter.drawText(QRectF(legend.right() + 4.0, legend.top() - 8.0, 42.0, 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter, QString::number(maximum, 'g', 4));
    painter.drawText(QRectF(legend.right() + 4.0, legend.bottom() - 10.0, 42.0, 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter, QString::number(minimum, 'g', 4));
}

void FieldView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !result_ || result_->points.empty()) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QRectF plot = QRectF(rect()).adjusted(12.0, 22.0, -78.0, -14.0);
    if (!plot.contains(event->position())) {
        QWidget::mousePressEvent(event);
        return;
    }
    int min_x = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int min_y = std::numeric_limits<int>::max();
    int max_y = std::numeric_limits<int>::min();
    int step_x = 1;
    int step_y = 1;
    for (const auto& point : result_->points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }
    for (std::size_t index = 1; index < result_->points.size(); ++index) {
        const auto& previous = result_->points[index - 1];
        const auto& current = result_->points[index];
        if (current.y == previous.y && current.x > previous.x) {
            step_x = current.x - previous.x;
            break;
        }
    }
    for (std::size_t index = 1; index < result_->points.size(); ++index) {
        const auto& previous = result_->points[index - 1];
        const auto& current = result_->points[index];
        if (current.y > previous.y) {
            step_y = current.y - previous.y;
            break;
        }
    }
    const double span_x = std::max(1.0, static_cast<double>(max_x - min_x + step_x));
    const double span_y = std::max(1.0, static_cast<double>(max_y - min_y + step_y));
    double best_distance = std::numeric_limits<double>::infinity();
    int best_index = -1;
    for (std::size_t index = 0; index < result_->points.size(); ++index) {
        const auto& point = result_->points[index];
        const QPointF center(
            plot.left() + (point.x - min_x + step_x * 0.5) * plot.width() / span_x,
            plot.top() + (point.y - min_y + step_y * 0.5) * plot.height() / span_y);
        const double distance = std::hypot(
            center.x() - event->position().x(), center.y() - event->position().y());
        if (distance < best_distance) {
            best_distance = distance;
            best_index = static_cast<int>(index);
        }
    }
    if (best_index >= 0) {
        selected_index_ = best_index;
        emit pointSelected(best_index);
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}
