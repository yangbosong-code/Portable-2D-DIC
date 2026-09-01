#include "signal_view.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace {

bool extractValue(const p2dic::DicPoint& point, int component, double& value) {
    if (component >= 2 && component <= 4) {
        if (!point.strain_valid) return false;
    } else if (!point.valid) {
        return false;
    }
    switch (component) {
        case 0: value = point.u; break;
        case 1: value = point.v; break;
        case 2: value = point.exx; break;
        case 3: value = point.eyy; break;
        case 4: value = point.exy; break;
        case 5: value = point.quality; break;
        default: return false;
    }
    return std::isfinite(value);
}

QString componentName(int component, const QString& displacement_unit) {
    static const char* names[] = {"u [px]", "v [px]", "exx", "eyy", "exy", "quality"};
    if (component == 0) return QStringLiteral("u [%1]").arg(displacement_unit);
    if (component == 1) return QStringLiteral("v [%1]").arg(displacement_unit);
    return component >= 0 && component < 6 ? QString::fromLatin1(names[component]) : QString{};
}

void drawFrame(QPainter& painter, const QRectF& area, const QString& title) {
    painter.setPen(QColor(70, 76, 86));
    painter.drawRect(area);
    for (int division = 1; division < 4; ++division) {
        const double y = area.top() + area.height() * division / 4.0;
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
    painter.setPen(QColor(205, 210, 218));
    painter.drawText(QRectF(area.left(), area.top() - 20.0, area.width(), 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter, title);
}

void fftInPlace(std::vector<std::complex<double>>& values) {
    const std::size_t count = values.size();
    for (std::size_t index = 1, reversed = 0; index < count; ++index) {
        std::size_t bit = count >> 1U;
        while ((reversed & bit) != 0U) {
            reversed ^= bit;
            bit >>= 1U;
        }
        reversed ^= bit;
        if (index < reversed) std::swap(values[index], values[reversed]);
    }
    for (std::size_t length = 2; length <= count; length <<= 1U) {
        const std::complex<double> step = std::polar(
            1.0, -2.0 * std::numbers::pi / static_cast<double>(length));
        for (std::size_t offset = 0; offset < count; offset += length) {
            std::complex<double> factor{1.0, 0.0};
            for (std::size_t index = 0; index < length / 2; ++index) {
                const auto even = values[offset + index];
                const auto odd = values[offset + index + length / 2] * factor;
                values[offset + index] = even + odd;
                values[offset + index + length / 2] = even - odd;
                factor *= step;
            }
        }
    }
}

}  // namespace

SignalView::SignalView(QWidget* parent) : QWidget(parent) {}

void SignalView::setPointIndex(int index) {
    if (index == point_index_) return;
    point_index_ = std::max(0, index);
    samples_.clear();
    update();
}

void SignalView::setComponentIndex(int index) {
    if (index == component_index_ || index < 0 || index > 5) return;
    component_index_ = index;
    samples_.clear();
    update();
}

void SignalView::setDisplacementScale(double scale, const QString& unit) {
    if (!std::isfinite(scale) || scale <= 0.0 || unit.isEmpty()) return;
    if (scale == displacement_scale_ && unit == displacement_unit_) return;
    displacement_scale_ = scale;
    displacement_unit_ = unit;
    samples_.clear();
    update();
}

void SignalView::appendResult(std::shared_ptr<const p2dic::DicResult> result) {
    if (!result || point_index_ < 0 ||
        static_cast<std::size_t>(point_index_) >= result->points.size()) return;
    double value = 0.0;
    if (!extractValue(result->points[static_cast<std::size_t>(point_index_)],
                      component_index_, value)) return;
    if (component_index_ == 0 || component_index_ == 1) {
        value *= displacement_scale_;
    }
    samples_.push_back(Sample{result->frame_timestamp_ns, value});
    while (samples_.size() > 512) samples_.pop_front();
    update();
}

QSize SignalView::minimumSizeHint() const { return QSize(480, 180); }

void SignalView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(28, 31, 36));
    const QRectF upper = QRectF(rect()).adjusted(48.0, 28.0, -14.0, -height() * 0.52);
    const QRectF lower = QRectF(rect()).adjusted(48.0, height() * 0.56, -14.0, -18.0);
    drawFrame(painter, upper,
              tr("测点 %1 · %2 时间曲线 / Time History")
                  .arg(point_index_)
                  .arg(componentName(component_index_, displacement_unit_)));
    drawFrame(painter, lower, tr("幅值频谱 / FFT Magnitude"));
    if (samples_.size() < 2) {
        painter.setPen(QColor(170, 175, 185));
        painter.drawText(upper, Qt::AlignCenter, tr("等待测点数据 / Waiting for samples"));
        return;
    }

    double minimum = samples_.front().value;
    double maximum = minimum;
    for (const auto& sample : samples_) {
        minimum = std::min(minimum, sample.value);
        maximum = std::max(maximum, sample.value);
    }
    if (maximum - minimum < 1e-12) {
        const double padding = std::max(std::abs(maximum) * 0.05, 1e-6);
        minimum -= padding;
        maximum += padding;
    }
    QPainterPath history;
    for (std::size_t index = 0; index < samples_.size(); ++index) {
        const double x = upper.left() + upper.width() * index / (samples_.size() - 1.0);
        const double y = upper.bottom() - upper.height() *
            (samples_[index].value - minimum) / (maximum - minimum);
        if (index == 0) history.moveTo(x, y); else history.lineTo(x, y);
    }
    painter.setPen(QPen(QColor(40, 205, 230), 1.6));
    painter.drawPath(history);
    painter.setPen(QColor(190, 195, 205));
    painter.drawText(QRectF(2.0, upper.top() - 8.0, 43.0, 18.0),
                     Qt::AlignRight | Qt::AlignVCenter, QString::number(maximum, 'g', 4));
    painter.drawText(QRectF(2.0, upper.bottom() - 10.0, 43.0, 18.0),
                     Qt::AlignRight | Qt::AlignVCenter, QString::number(minimum, 'g', 4));

    if (samples_.size() < 8) return;
    std::size_t count = 1;
    while ((count << 1U) <= samples_.size()) count <<= 1U;
    const std::size_t first_sample = samples_.size() - count;
    double mean = 0.0;
    for (std::size_t index = first_sample; index < samples_.size(); ++index) {
        mean += samples_[index].value;
    }
    mean /= static_cast<double>(count);
    std::vector<std::complex<double>> spectrum_values(count);
    for (std::size_t index = 0; index < count; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi * index / static_cast<double>(count - 1));
        spectrum_values[index] =
            (samples_[first_sample + index].value - mean) * window;
    }
    fftInPlace(spectrum_values);
    std::vector<double> magnitude(count / 2, 0.0);
    for (std::size_t bin = 1; bin < magnitude.size(); ++bin) {
        magnitude[bin] = 2.0 * std::abs(spectrum_values[bin]) / count;
    }
    const double maximum_magnitude = *std::max_element(magnitude.begin(), magnitude.end());
    if (maximum_magnitude <= 0.0) return;
    QPainterPath spectrum;
    for (std::size_t bin = 1; bin < magnitude.size(); ++bin) {
        const double x = lower.left() + lower.width() * (bin - 1.0) /
            std::max(1.0, magnitude.size() - 2.0);
        const double y = lower.bottom() - lower.height() * magnitude[bin] / maximum_magnitude;
        if (bin == 1) spectrum.moveTo(x, y); else spectrum.lineTo(x, y);
    }
    painter.setPen(QPen(QColor(245, 185, 45), 1.4));
    painter.drawPath(spectrum);
    double sample_rate = 0.0;
    if (samples_.back().timestamp_ns > samples_[first_sample].timestamp_ns) {
        sample_rate = (count - 1.0) * 1.0e9 /
            (samples_.back().timestamp_ns - samples_[first_sample].timestamp_ns);
    }
    painter.setPen(QColor(190, 195, 205));
    painter.drawText(QRectF(lower.left(), lower.bottom(), lower.width(), 18.0),
                     Qt::AlignRight | Qt::AlignVCenter,
                     tr("Nyquist %1 Hz").arg(sample_rate * 0.5, 0, 'f', 2));
}
