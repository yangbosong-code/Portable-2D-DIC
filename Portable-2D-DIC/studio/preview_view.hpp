#pragma once

#include "preview_client.hpp"

#include <QWidget>
#include <QPointF>
#include <QVector>

class PreviewView final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewView(QWidget* parent = nullptr);
    void setPreview(PreviewFramePtr preview);
    void setCalibrationMarkers(const QVector<QPointF>& normalized_points);
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void normalizedPointClicked(const QPointF& point);

private:
    PreviewFramePtr preview_;
    QVector<QPointF> calibration_markers_;
    QRectF image_rect_;
};
