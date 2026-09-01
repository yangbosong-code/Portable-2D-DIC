#pragma once

#include "p2dic/dic_engine.hpp"

#include <QWidget>

#include <memory>

class FieldView final : public QWidget {
    Q_OBJECT

public:
    enum class Component { u, v, exx, eyy, exy, quality };

    explicit FieldView(QWidget* parent = nullptr);
    void setResult(std::shared_ptr<const p2dic::DicResult> result);
    void setComponent(Component component);
    void setComponentIndex(int index);
    void setDisplacementScale(double scale, const QString& unit);
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void pointSelected(int index);

private:
    std::shared_ptr<const p2dic::DicResult> result_;
    Component component_{Component::u};
    double displacement_scale_{1.0};
    QString displacement_unit_{QStringLiteral("px")};
    int selected_index_{-1};
};
