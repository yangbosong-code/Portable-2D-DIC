#pragma once

#include "p2dic/dic_engine.hpp"

#include <QWidget>

#include <cstdint>
#include <deque>
#include <memory>

class SignalView final : public QWidget {
    Q_OBJECT

public:
    explicit SignalView(QWidget* parent = nullptr);
    void setPointIndex(int index);
    void setComponentIndex(int index);
    void setDisplacementScale(double scale, const QString& unit);
    void appendResult(std::shared_ptr<const p2dic::DicResult> result);
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Sample {
        std::uint64_t timestamp_ns{};
        double value{};
    };
    std::deque<Sample> samples_;
    int point_index_{0};
    int component_index_{0};
    double displacement_scale_{1.0};
    QString displacement_unit_{QStringLiteral("px")};
};
