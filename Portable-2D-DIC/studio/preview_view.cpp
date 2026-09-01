#include "preview_view.hpp"

#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

PreviewView::PreviewView(QWidget* parent) : QWidget(parent) {}

void PreviewView::setPreview(PreviewFramePtr preview) {
    preview_ = std::move(preview);
    update();
}

void PreviewView::setCalibrationMarkers(const QVector<QPointF>& normalized_points) {
    calibration_markers_ = normalized_points;
    update();
}

QSize PreviewView::minimumSizeHint() const { return QSize(480, 320); }

void PreviewView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 22, 26));
    if (!preview_ || preview_->pixels.empty()) {
        image_rect_ = {};
        painter.setPen(QColor(190, 195, 205));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("等待相机预览 / Waiting for camera preview"));
        return;
    }
    const QImage image(
        preview_->pixels.data(), static_cast<int>(preview_->width),
        static_cast<int>(preview_->height), static_cast<int>(preview_->width),
        QImage::Format_Grayscale8);
    const QRect target = rect().adjusted(8, 24, -8, -8);
    const QSize scaled = image.size().scaled(target.size(), Qt::KeepAspectRatio);
    const QRect destination(
        target.center().x() - scaled.width() / 2,
        target.center().y() - scaled.height() / 2,
        scaled.width(), scaled.height());
    image_rect_ = destination;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(destination, image);
    painter.setPen(QColor(230, 232, 238));
    painter.drawText(QRect(8, 2, width() - 16, 20),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Frame %1 · %2×%3 Mono8")
                         .arg(preview_->sequence)
                         .arg(preview_->width)
                         .arg(preview_->height));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 80, 60), 2.0));
    QVector<QPointF> screen_points;
    for (const auto& marker : calibration_markers_) {
        const QPointF screen(
            image_rect_.left() + marker.x() * image_rect_.width(),
            image_rect_.top() + marker.y() * image_rect_.height());
        screen_points.push_back(screen);
        painter.drawEllipse(screen, 6.0, 6.0);
    }
    if (screen_points.size() == 2) {
        painter.drawLine(screen_points[0], screen_points[1]);
    }
}

void PreviewView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !image_rect_.isEmpty() &&
        image_rect_.contains(event->position())) {
        const QPointF normalized(
            (event->position().x() - image_rect_.left()) / image_rect_.width(),
            (event->position().y() - image_rect_.top()) / image_rect_.height());
        emit normalizedPointClicked(normalized);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}
