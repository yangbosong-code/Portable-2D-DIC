#pragma once

#include "edge_client.hpp"
#include "result_client.hpp"
#include "preview_client.hpp"

#include <QMainWindow>
#include <QPointF>
#include <QVector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QTimer;
class QStackedWidget;
class QListWidget;
class QTabWidget;
class QToolButton;
class FieldView;
class SignalView;
class PreviewView;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void ping();
    void refreshStatus();
    void startMeasurement();
    void stopMeasurement();
    void togglePauseMeasurement();
    void resetFault();
    void requestConfiguration();
    void applyConfiguration();
    void saveConfiguration();
    void beginScaleCalibration();
    void clearScaleCalibration();
    void handleCalibrationPoint(const QPointF& normalized_point);
    void handleResponse(const QString& response);
    void handleFailure(const QString& message);
    void handleResult(DicResultPtr result);
    void handleResultStreamState(bool online);
    void handleResultStreamFailure(const QString& message);
    void handlePreview(PreviewFramePtr preview);
    void handlePreviewStreamState(bool online);
    void handlePreviewStreamFailure(const QString& message);

private:
    void send(const QString& command);
    void appendLog(const QString& level, const QString& message);
    void invalidateConnection();
    void applyStudioTheme();
    void setExpertMode(bool enabled);
    void showWorkspace(const QString& project_name);
    void showProjectCenter();
    QString currentHost() const;
    quint16 currentPort() const;
    quint16 currentResultPort() const;
    quint16 currentPreviewPort() const;

    EdgeClient client_;
    ResultClient result_client_;
    PreviewClient preview_client_;
    QLineEdit* host_{};
    QSpinBox* port_{};
    QSpinBox* result_port_{};
    QSpinBox* preview_port_{};
    QLineEdit* session_id_{};
    QLabel* connection_state_{};
    QLabel* result_stream_state_{};
    QLabel* preview_stream_state_{};
    QLabel* measurement_state_{};
    QLabel* metrics_{};
    QLabel* performance_metrics_{};
    QLabel* result_frame_{};
    QLabel* result_points_{};
    QLabel* valid_ratio_{};
    QLabel* mean_displacement_{};
    QLabel* mean_strain_{};
    QLabel* processing_time_{};
    QLabel* backend_summary_{};
    QDoubleSpinBox* exposure_us_{};
    QDoubleSpinBox* gain_db_{};
    QCheckBox* external_trigger_{};
    QSpinBox* roi_offset_x_{};
    QSpinBox* roi_offset_y_{};
    QSpinBox* image_width_{};
    QSpinBox* image_height_{};
    QDoubleSpinBox* image_fps_{};
    QSpinBox* subset_radius_{};
    QSpinBox* grid_step_{};
    QSpinBox* search_radius_{};
    QDoubleSpinBox* quality_threshold_{};
    QDoubleSpinBox* known_distance_mm_{};
    QLabel* calibration_state_{};
    QComboBox* field_component_{};
    FieldView* field_view_{};
    QSpinBox* point_index_{};
    SignalView* signal_view_{};
    PreviewView* preview_view_{};
    QPushButton* ping_button_{};
    QPushButton* start_button_{};
    QPushButton* stop_button_{};
    QPushButton* pause_button_{};
    QPushButton* reset_button_{};
    QPushButton* read_config_button_{};
    QPushButton* apply_config_button_{};
    QPushButton* save_config_button_{};
    QPushButton* calibrate_button_{};
    QPushButton* clear_calibration_button_{};
    QPlainTextEdit* log_{};
    QTimer* status_timer_{};
    QStackedWidget* application_pages_{};
    QListWidget* workflow_navigation_{};
    QTabWidget* view_tabs_{};
    QTabWidget* property_tabs_{};
    QTabWidget* analysis_tabs_{};
    QToolButton* mode_button_{};
    QToolButton* theme_button_{};
    QWidget* expert_parameters_{};
    QLabel* current_project_{};
    QLabel* edge_status_{};
    QLabel* stream_status_{};
    QLabel* frame_status_{};
    QLabel* latency_status_{};
    QLabel* dropped_status_{};
    QVector<QPointF> calibration_points_;
    bool calibration_collecting_{false};
    bool connected_{false};
    bool expert_mode_{false};
    bool dark_theme_{false};
    double millimeters_per_pixel_{0.0};
};
