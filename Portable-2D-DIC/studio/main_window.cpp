#include "main_window.hpp"
#include "field_view.hpp"
#include "signal_view.hpp"
#include "preview_view.hpp"

#include "p2dic/result_summary.hpp"

#include <QApplication>
#include <QDateTime>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace {

QString studio_style_sheet(bool dark) {
    QString style = QStringLiteral(R"QSS(
        QWidget {
            background: %BG%;
            color: %TEXT%;
            font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
            font-size: 13px;
        }
        QMainWindow, QStackedWidget { background: %BG%; }
        QLabel { background: transparent; }
        QFrame#appHeader, QFrame#commandBar, QFrame#statusStrip,
        QFrame#projectHero, QFrame#projectCard {
            background: %SURFACE%;
            border: 1px solid %BORDER%;
            border-radius: 8px;
        }
        QFrame#appHeader { border-radius: 0; border-width: 0 0 1px 0; }
        QFrame#commandBar { border-radius: 0; border-width: 0 0 1px 0; }
        QFrame#sideBar { background: %SIDEBAR%; border-right: 1px solid %BORDER%; }
        QLabel#productTitle { font-size: 20px; font-weight: 650; color: %TITLE%; }
        QLabel#projectName { font-size: 12px; color: %MUTED%; }
        QLabel#heroTitle { font-size: 28px; font-weight: 700; color: %TITLE%; }
        QLabel#heroSubtitle { font-size: 14px; color: %MUTED%; }
        QLabel#sectionTitle { font-size: 15px; font-weight: 650; color: %TITLE%; }
        QLabel#metricValue { font-size: 16px; font-weight: 650; color: %ACCENT%; }
        QLabel#mutedLabel { color: %MUTED%; }
        QLabel#statusChip {
            background: %CHIP%; border: 1px solid %BORDER%; border-radius: 10px;
            padding: 3px 9px; color: %TEXT%;
        }
        QPushButton, QToolButton {
            background: %CONTROL%; border: 1px solid %BORDER%; border-radius: 5px;
            padding: 6px 12px; min-height: 20px;
        }
        QPushButton:hover, QToolButton:hover { border-color: %ACCENT%; background: %HOVER%; }
        QPushButton:pressed, QToolButton:pressed { background: %PRESSED%; }
        QPushButton:disabled, QToolButton:disabled { color: %DISABLED%; background: %DISABLED_BG%; }
        QPushButton#primaryButton { background: %ACCENT%; color: white; border-color: %ACCENT%; font-weight: 600; }
        QPushButton#primaryButton:hover { background: %ACCENT_HOVER%; }
        QPushButton#dangerButton { color: %DANGER%; border-color: %DANGER%; }
        QPushButton#dangerButton:hover { color: white; background: %DANGER%; }
        QToolButton:checked { background: %ACCENT_SOFT%; color: %ACCENT%; border-color: %ACCENT%; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit {
            background: %INPUT%; border: 1px solid %BORDER%; border-radius: 4px;
            padding: 5px 7px; selection-background-color: %ACCENT%;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,
        QPlainTextEdit:focus { border: 1px solid %ACCENT%; }
        QGroupBox {
            background: %SURFACE%; border: 1px solid %BORDER%; border-radius: 7px;
            margin-top: 14px; padding-top: 8px; font-weight: 600;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: %TITLE%; }
        QTabWidget::pane { border: 1px solid %BORDER%; background: %SURFACE%; }
        QTabBar::tab { background: %CONTROL%; border: 1px solid %BORDER%; padding: 8px 14px; }
        QTabBar::tab:selected { background: %SURFACE%; color: %ACCENT%; border-bottom: 2px solid %ACCENT%; }
        QListWidget#workflowNav { background: transparent; border: 0; outline: 0; padding: 8px; }
        QListWidget#workflowNav::item { border-radius: 6px; padding: 11px 10px; margin: 2px 0; color: %TEXT%; }
        QListWidget#workflowNav::item:hover { background: %HOVER%; }
        QListWidget#workflowNav::item:selected { background: %ACCENT_SOFT%; color: %ACCENT%; font-weight: 650; }
        QScrollArea { border: 0; background: transparent; }
        QSplitter::handle { background: %BORDER%; }
        QStatusBar { background: %SURFACE%; border-top: 1px solid %BORDER%; }
        QStatusBar::item { border: 0; }
    )QSS");
    const auto replace = [&style](const QString& key, const QString& value) {
        style.replace(key, value);
    };
    replace(QStringLiteral("%BG%"), dark ? QStringLiteral("#15191f") : QStringLiteral("#eef2f6"));
    replace(QStringLiteral("%SURFACE%"), dark ? QStringLiteral("#20262e") : QStringLiteral("#ffffff"));
    replace(QStringLiteral("%SIDEBAR%"), dark ? QStringLiteral("#1b2027") : QStringLiteral("#f7f9fb"));
    replace(QStringLiteral("%TEXT%"), dark ? QStringLiteral("#dfe6ee") : QStringLiteral("#263241"));
    replace(QStringLiteral("%TITLE%"), dark ? QStringLiteral("#f4f7fb") : QStringLiteral("#142033"));
    replace(QStringLiteral("%MUTED%"), dark ? QStringLiteral("#98a6b6") : QStringLiteral("#66758a"));
    replace(QStringLiteral("%BORDER%"), dark ? QStringLiteral("#37414d") : QStringLiteral("#d5dde7"));
    replace(QStringLiteral("%CONTROL%"), dark ? QStringLiteral("#29313a") : QStringLiteral("#f8fafc"));
    replace(QStringLiteral("%INPUT%"), dark ? QStringLiteral("#171c22") : QStringLiteral("#ffffff"));
    replace(QStringLiteral("%HOVER%"), dark ? QStringLiteral("#313b46") : QStringLiteral("#edf4ff"));
    replace(QStringLiteral("%PRESSED%"), dark ? QStringLiteral("#3a4653") : QStringLiteral("#dceaff"));
    replace(QStringLiteral("%CHIP%"), dark ? QStringLiteral("#29313a") : QStringLiteral("#f2f5f8"));
    replace(QStringLiteral("%DISABLED%"), dark ? QStringLiteral("#6d7886") : QStringLiteral("#9aa6b4"));
    replace(QStringLiteral("%DISABLED_BG%"), dark ? QStringLiteral("#242a31") : QStringLiteral("#edf0f3"));
    replace(QStringLiteral("%ACCENT%"), QStringLiteral("#1769e0"));
    replace(QStringLiteral("%ACCENT_HOVER%"), QStringLiteral("#0f5bc8"));
    replace(QStringLiteral("%ACCENT_SOFT%"), dark ? QStringLiteral("#17375d") : QStringLiteral("#e5f0ff"));
    replace(QStringLiteral("%DANGER%"), dark ? QStringLiteral("#ff6b78") : QStringLiteral("#c93645"));
    return style;
}

QScrollArea* make_scroll_panel(QWidget* content, QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    return scroll;
}

QWidget* make_pair_widget(QWidget* first, QWidget* second, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(first);
    layout->addWidget(second);
    return widget;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), status_timer_(new QTimer(this)) {
    setWindowTitle(QStringLiteral("Portable 2D-DIC Measurement System"));
    resize(1480, 920);
    setMinimumSize(1100, 720);

    application_pages_ = new QStackedWidget(this);

    // Project center ---------------------------------------------------------
    auto* project_center = new QWidget(application_pages_);
    auto* project_root = new QVBoxLayout(project_center);
    project_root->setContentsMargins(44, 34, 44, 34);
    project_root->setSpacing(20);

    auto* project_hero = new QFrame(project_center);
    project_hero->setObjectName(QStringLiteral("projectHero"));
    auto* hero_layout = new QVBoxLayout(project_hero);
    hero_layout->setContentsMargins(28, 24, 28, 24);
    auto* hero_title = new QLabel(QStringLiteral("Portable 2D-DIC Measurement System"), project_hero);
    hero_title->setObjectName(QStringLiteral("heroTitle"));
    auto* hero_subtitle = new QLabel(
        tr("便携式二维数字图像相关测量平台 · 项目、采集、实时分析与报告"), project_hero);
    hero_subtitle->setObjectName(QStringLiteral("heroSubtitle"));
    hero_layout->addWidget(hero_title);
    hero_layout->addWidget(hero_subtitle);

    auto* action_grid = new QGridLayout;
    action_grid->setHorizontalSpacing(14);
    action_grid->setVerticalSpacing(14);
    auto* new_project_button = new QPushButton(tr("新建项目\nNew Project"), project_center);
    new_project_button->setObjectName(QStringLiteral("primaryButton"));
    new_project_button->setMinimumHeight(66);
    auto* open_project_button = new QPushButton(tr("打开项目\nOpen Project"), project_center);
    open_project_button->setMinimumHeight(66);
    auto* quick_demo_button = new QPushButton(tr("快速模拟演示\nQuick Demo"), project_center);
    quick_demo_button->setMinimumHeight(66);
    action_grid->addWidget(new_project_button, 0, 0);
    action_grid->addWidget(open_project_button, 0, 1);
    action_grid->addWidget(quick_demo_button, 0, 2);

    auto* center_columns = new QHBoxLayout;
    center_columns->setSpacing(16);
    auto* recent_card = new QFrame(project_center);
    recent_card->setObjectName(QStringLiteral("projectCard"));
    auto* recent_layout = new QVBoxLayout(recent_card);
    auto* recent_title = new QLabel(tr("最近项目 / Recent Projects"), recent_card);
    recent_title->setObjectName(QStringLiteral("sectionTitle"));
    auto* recent_list = new QListWidget(recent_card);
    recent_list->addItem(tr("尚无最近项目 · 新建项目开始测量"));
    recent_list->item(0)->setFlags(Qt::NoItemFlags);
    recent_layout->addWidget(recent_title);
    recent_layout->addWidget(recent_list, 1);

    auto* device_card = new QFrame(project_center);
    device_card->setObjectName(QStringLiteral("projectCard"));
    auto* device_layout = new QVBoxLayout(device_card);
    auto* device_title = new QLabel(tr("计算盒 / DIC Edge"), device_card);
    device_title->setObjectName(QStringLiteral("sectionTitle"));
    auto* discovery_status = new QLabel(tr("● 等待发现局域网计算盒"), device_card);
    discovery_status->setObjectName(QStringLiteral("metricValue"));
    auto* device_hint = new QLabel(
        tr("支持自动发现、最近设备和手动IP。快速演示使用本机 127.0.0.1。"), device_card);
    device_hint->setWordWrap(true);
    device_hint->setObjectName(QStringLiteral("mutedLabel"));
    device_layout->addWidget(device_title);
    device_layout->addSpacing(12);
    device_layout->addWidget(discovery_status);
    device_layout->addWidget(device_hint);
    device_layout->addStretch(1);
    center_columns->addWidget(recent_card, 3);
    center_columns->addWidget(device_card, 2);

    auto* standards_note = new QLabel(
        tr("设计参考：iDICs Good Practices Guide (Edition 2) · VDI/VDE 2626 Part 1\n"
           "当前软件状态：工程开发版；真实相机与整机计量性能仍需硬件验收。"), project_center);
    standards_note->setWordWrap(true);
    standards_note->setObjectName(QStringLiteral("mutedLabel"));
    project_root->addWidget(project_hero);
    project_root->addLayout(action_grid);
    project_root->addLayout(center_columns, 1);
    project_root->addWidget(standards_note);

    // Measurement workspace -------------------------------------------------
    auto* workspace = new QWidget(application_pages_);
    auto* workspace_root = new QVBoxLayout(workspace);
    workspace_root->setContentsMargins(0, 0, 0, 0);
    workspace_root->setSpacing(0);

    auto* app_header = new QFrame(workspace);
    app_header->setObjectName(QStringLiteral("appHeader"));
    auto* header_layout = new QHBoxLayout(app_header);
    header_layout->setContentsMargins(14, 9, 14, 9);
    auto* project_center_button = new QToolButton(app_header);
    project_center_button->setText(tr("项目中心"));
    project_center_button->setToolTip(tr("返回项目中心 / Project Center"));
    auto* title_stack = new QVBoxLayout;
    title_stack->setSpacing(0);
    auto* product_title = new QLabel(QStringLiteral("Portable 2D-DIC Measurement System"), app_header);
    product_title->setObjectName(QStringLiteral("productTitle"));
    current_project_ = new QLabel(tr("未命名项目 / Untitled Project"), app_header);
    current_project_->setObjectName(QStringLiteral("projectName"));
    title_stack->addWidget(product_title);
    title_stack->addWidget(current_project_);
    mode_button_ = new QToolButton(app_header);
    mode_button_->setCheckable(true);
    theme_button_ = new QToolButton(app_header);
    header_layout->addWidget(project_center_button);
    header_layout->addSpacing(8);
    header_layout->addLayout(title_stack);
    header_layout->addStretch(1);
    header_layout->addWidget(mode_button_);
    header_layout->addWidget(theme_button_);

    auto* command_bar = new QFrame(workspace);
    command_bar->setObjectName(QStringLiteral("commandBar"));
    auto* command_layout = new QHBoxLayout(command_bar);
    command_layout->setContentsMargins(14, 7, 14, 7);
    command_layout->setSpacing(8);
    session_id_ = new QLineEdit(
        QStringLiteral("test-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")),
        command_bar);
    session_id_->setMinimumWidth(220);
    start_button_ = new QPushButton(tr("开始 / Start"), command_bar);
    start_button_->setObjectName(QStringLiteral("primaryButton"));
    pause_button_ = new QPushButton(tr("暂停记录 / Pause"), command_bar);
    stop_button_ = new QPushButton(tr("停止 / Stop"), command_bar);
    stop_button_->setObjectName(QStringLiteral("dangerButton"));
    reset_button_ = new QPushButton(tr("故障复位 / Reset"), command_bar);
    auto* freeze_button = new QPushButton(tr("冻结显示 / Freeze View"), command_bar);
    freeze_button->setCheckable(true);
    freeze_button->setToolTip(tr("第一阶段仅冻结界面刷新，Edge继续测量"));
    auto* reference_button = new QPushButton(tr("设为参考 / Set Reference"), command_bar);
    reference_button->setEnabled(false);
    reference_button->setToolTip(tr("参考帧管理将在第二阶段启用"));
    stop_button_->setEnabled(false);
    pause_button_->setEnabled(false);
    reset_button_->setEnabled(false);
    command_layout->addWidget(new QLabel(QStringLiteral("Session ID"), command_bar));
    command_layout->addWidget(session_id_, 1);
    command_layout->addWidget(start_button_);
    command_layout->addWidget(pause_button_);
    command_layout->addWidget(stop_button_);
    command_layout->addWidget(freeze_button);
    command_layout->addWidget(reference_button);
    command_layout->addWidget(reset_button_);

    auto* body_splitter = new QSplitter(Qt::Horizontal, workspace);
    body_splitter->setChildrenCollapsible(false);

    auto* sidebar = new QFrame(body_splitter);
    sidebar->setObjectName(QStringLiteral("sideBar"));
    sidebar->setMinimumWidth(190);
    sidebar->setMaximumWidth(230);
    auto* sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(5, 10, 5, 10);
    auto* workflow_title = new QLabel(tr("测量流程 / Workflow"), sidebar);
    workflow_title->setObjectName(QStringLiteral("sectionTitle"));
    workflow_title->setContentsMargins(12, 4, 0, 6);
    workflow_navigation_ = new QListWidget(sidebar);
    workflow_navigation_->setObjectName(QStringLiteral("workflowNav"));
    workflow_navigation_->addItems({
        tr("01  项目 / Project"), tr("02  设备 / Device"),
        tr("03  图像与标定 / Setup"), tr("04  采集 / Acquire"),
        tr("05  实时分析 / Analyze"), tr("06  结果与报告 / Report")});
    sidebar_layout->addWidget(workflow_title);
    sidebar_layout->addWidget(workflow_navigation_, 1);
    auto* validation_badge = new QLabel(tr("工程开发版\nHardware validation pending"), sidebar);
    validation_badge->setWordWrap(true);
    validation_badge->setObjectName(QStringLiteral("mutedLabel"));
    validation_badge->setContentsMargins(12, 8, 8, 8);
    sidebar_layout->addWidget(validation_badge);

    // Reusable connection controls.
    auto* connection_page = new QWidget(body_splitter);
    auto* connection_page_layout = new QVBoxLayout(connection_page);
    auto* connection_group = new QGroupBox(tr("计算盒连接 / Edge Connection"), connection_page);
    auto* connection_form = new QFormLayout(connection_group);
    host_ = new QLineEdit(QStringLiteral("192.168.1.10"), connection_group);
    host_->setPlaceholderText(QStringLiteral("IP address"));
    port_ = new QSpinBox(connection_group);
    port_->setRange(1, 65535);
    port_->setValue(17840);
    result_port_ = new QSpinBox(connection_group);
    result_port_->setRange(1, 65535);
    result_port_->setValue(17841);
    preview_port_ = new QSpinBox(connection_group);
    preview_port_->setRange(0, 65535);
    preview_port_->setValue(17842);
    ping_button_ = new QPushButton(tr("测试连接 / Ping"), connection_group);
    ping_button_->setObjectName(QStringLiteral("primaryButton"));
    connection_state_ = new QLabel(tr("未连接 / Offline"), connection_group);
    result_stream_state_ = new QLabel(tr("数据流离线 / Stream Offline"), connection_group);
    preview_stream_state_ = new QLabel(tr("预览离线 / Preview Offline"), connection_group);
    connection_form->addRow(QStringLiteral("Host"), host_);
    connection_form->addRow(QStringLiteral("Control Port"), port_);
    connection_form->addRow(QStringLiteral("Result Port"), result_port_);
    connection_form->addRow(QStringLiteral("Preview Port"), preview_port_);
    connection_form->addRow(ping_button_);
    auto* channel_status = new QGroupBox(tr("通道状态 / Channels"), connection_page);
    auto* channel_layout = new QVBoxLayout(channel_status);
    channel_layout->addWidget(connection_state_);
    channel_layout->addWidget(result_stream_state_);
    channel_layout->addWidget(preview_stream_state_);
    auto* discovery_group = new QGroupBox(tr("设备发现 / Discovery"), connection_page);
    auto* discovery_layout = new QVBoxLayout(discovery_group);
    auto* discovery_planned = new QLabel(
        tr("局域网自动发现、最近设备和版本检查将在第二阶段启用。\n当前可使用手动IP或快速模拟演示。"),
        discovery_group);
    discovery_planned->setWordWrap(true);
    discovery_planned->setObjectName(QStringLiteral("mutedLabel"));
    discovery_layout->addWidget(discovery_planned);
    connection_page_layout->addWidget(connection_group);
    connection_page_layout->addWidget(channel_status);
    connection_page_layout->addWidget(discovery_group);
    connection_page_layout->addStretch(1);

    auto* measurement_page = new QWidget(body_splitter);
    auto* measurement_page_layout = new QVBoxLayout(measurement_page);
    auto* experiment_group = new QGroupBox(tr("测量状态 / Measurement"), measurement_page);
    auto* experiment_layout = new QVBoxLayout(experiment_group);
    measurement_state_ = new QLabel(tr("状态 / State: unknown"), experiment_group);
    measurement_state_->setObjectName(QStringLiteral("metricValue"));
    metrics_ = new QLabel(
        QStringLiteral("captured=0  processed=0  dropped=0  timeouts=0  processing_ms=0"),
        experiment_group);
    metrics_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    metrics_->setWordWrap(true);
    metrics_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    performance_metrics_ = new QLabel(
        QStringLiteral("Pipeline P50/P95/P99: 0/0/0 ms | GPU H2D/Kernel/D2H: 0/0/0 ms | Writer: 0/0"),
        experiment_group);
    performance_metrics_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    performance_metrics_->setWordWrap(true);
    performance_metrics_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    experiment_layout->addWidget(measurement_state_);
    experiment_layout->addWidget(metrics_);
    experiment_layout->addWidget(performance_metrics_);

    auto* live_group = new QGroupBox(tr("实时计算摘要 / Live DIC Summary"), measurement_page);
    auto* live_layout = new QFormLayout(live_group);
    result_frame_ = new QLabel(QStringLiteral("0"), live_group);
    result_points_ = new QLabel(QStringLiteral("0"), live_group);
    valid_ratio_ = new QLabel(QStringLiteral("0.0 %"), live_group);
    mean_displacement_ = new QLabel(QStringLiteral("U=0.000 px, V=0.000 px"), live_group);
    mean_strain_ = new QLabel(QStringLiteral("εₓₓ=0.000000, εᵧᵧ=0.000000, εₓᵧ=0.000000"), live_group);
    processing_time_ = new QLabel(QStringLiteral("0.000 ms"), live_group);
    live_layout->addRow(tr("结果帧 / Result Frame"), result_frame_);
    live_layout->addRow(tr("网格点 / Grid Points"), result_points_);
    live_layout->addRow(tr("有效率 / Valid Ratio"), valid_ratio_);
    live_layout->addRow(tr("平均位移 / Mean Displacement"), mean_displacement_);
    live_layout->addRow(tr("平均应变 / Mean Strain"), mean_strain_);
    live_layout->addRow(tr("计算耗时 / Processing Time"), processing_time_);
    measurement_page_layout->addWidget(experiment_group);
    measurement_page_layout->addWidget(live_group);
    measurement_page_layout->addStretch(1);

    auto* config_page = new QWidget(body_splitter);
    auto* config_page_layout = new QVBoxLayout(config_page);
    auto* config_group = new QGroupBox(tr("相机参数 / Camera"), config_page);
    auto* config_layout = new QFormLayout(config_group);
    backend_summary_ = new QLabel(tr("Camera/DIC backend: unknown"), config_group);
    backend_summary_->setWordWrap(true);
    exposure_us_ = new QDoubleSpinBox(config_group);
    exposure_us_->setRange(11.0, 1000000.0);
    exposure_us_->setDecimals(1);
    exposure_us_->setValue(2000.0);
    exposure_us_->setSuffix(QStringLiteral(" us"));
    gain_db_ = new QDoubleSpinBox(config_group);
    gain_db_->setRange(0.0, 16.0);
    gain_db_->setDecimals(2);
    gain_db_->setSuffix(QStringLiteral(" dB"));
    external_trigger_ = new QCheckBox(tr("外部触发 / External Trigger"), config_group);
    roi_offset_x_ = new QSpinBox(config_group);
    roi_offset_x_->setRange(0, 4503);
    roi_offset_y_ = new QSpinBox(config_group);
    roi_offset_y_->setRange(0, 4095);
    image_width_ = new QSpinBox(config_group);
    image_width_->setRange(64, 4504);
    image_width_->setValue(4504);
    image_height_ = new QSpinBox(config_group);
    image_height_->setRange(64, 4096);
    image_height_->setValue(4096);
    image_fps_ = new QDoubleSpinBox(config_group);
    image_fps_->setRange(0.1, 1000.0);
    image_fps_->setDecimals(3);
    image_fps_->setValue(21.4);
    subset_radius_ = new QSpinBox(config_group);
    subset_radius_->setRange(2, 512);
    subset_radius_->setValue(20);
    grid_step_ = new QSpinBox(config_group);
    grid_step_->setRange(1, 2048);
    grid_step_->setValue(32);
    search_radius_ = new QSpinBox(config_group);
    search_radius_->setRange(0, 512);
    search_radius_->setValue(8);
    quality_threshold_ = new QDoubleSpinBox(config_group);
    quality_threshold_->setRange(-1.0, 1.0);
    quality_threshold_->setDecimals(4);
    quality_threshold_->setSingleStep(0.01);
    quality_threshold_->setValue(0.8);
    read_config_button_ = new QPushButton(tr("读取 / Read"), config_group);
    apply_config_button_ = new QPushButton(tr("空闲时应用 / Apply While Idle"), config_group);
    save_config_button_ = new QPushButton(tr("保存到 Edge / Save"), config_group);
    config_layout->addRow(backend_summary_);
    config_layout->addRow(QStringLiteral("ExposureTime"), exposure_us_);
    config_layout->addRow(QStringLiteral("Gain"), gain_db_);
    config_layout->addRow(external_trigger_);
    config_layout->addRow(QStringLiteral("OffsetX / OffsetY"),
                          make_pair_widget(roi_offset_x_, roi_offset_y_, config_group));
    config_layout->addRow(QStringLiteral("Width / Height"),
                          make_pair_widget(image_width_, image_height_, config_group));
    config_layout->addRow(QStringLiteral("FPS"), image_fps_);

    expert_parameters_ = new QGroupBox(tr("DIC专家参数 / Expert Parameters"), config_page);
    auto* expert_form = new QFormLayout(expert_parameters_);
    expert_form->addRow(QStringLiteral("SubsetRadius"), subset_radius_);
    expert_form->addRow(QStringLiteral("GridStep"), grid_step_);
    expert_form->addRow(QStringLiteral("SearchRadius"), search_radius_);
    expert_form->addRow(QStringLiteral("QualityThreshold"), quality_threshold_);
    auto* config_actions = new QWidget(config_page);
    auto* config_actions_layout = new QVBoxLayout(config_actions);
    config_actions_layout->setContentsMargins(0, 0, 0, 0);
    config_actions_layout->addWidget(read_config_button_);
    config_actions_layout->addWidget(apply_config_button_);
    config_actions_layout->addWidget(save_config_button_);
    config_page_layout->addWidget(config_group);
    config_page_layout->addWidget(expert_parameters_);
    config_page_layout->addWidget(config_actions);
    config_page_layout->addStretch(1);

    auto* tools_page = new QWidget(body_splitter);
    auto* tools_page_layout = new QVBoxLayout(tools_page);
    auto* display_group = new QGroupBox(tr("显示与探针 / Display & Probe"), tools_page);
    auto* display_form = new QFormLayout(display_group);
    field_component_ = new QComboBox(display_group);
    field_component_->addItems({
        QStringLiteral("U [px]"), QStringLiteral("V [px]"),
        QStringLiteral("εₓₓ"), QStringLiteral("εᵧᵧ"),
        QStringLiteral("εₓᵧ (tensor)"), QStringLiteral("ρ / quality")});
    point_index_ = new QSpinBox(display_group);
    point_index_->setRange(0, 0);
    display_form->addRow(tr("显示分量 / Component"), field_component_);
    display_form->addRow(tr("网格点 / Point Index"), point_index_);
    auto* overlay_note = new QLabel(
        tr("默认：云图半透明覆盖原图。专业色标、范围锁定、ROI与虚拟量规将在第二阶段启用。"),
        display_group);
    overlay_note->setWordWrap(true);
    overlay_note->setObjectName(QStringLiteral("mutedLabel"));
    display_form->addRow(overlay_note);

    auto* calibration_group = new QGroupBox(tr("比例标定 / Scale Calibration"), tools_page);
    auto* calibration_form = new QFormLayout(calibration_group);
    known_distance_mm_ = new QDoubleSpinBox(calibration_group);
    known_distance_mm_->setRange(0.001, 1000000.0);
    known_distance_mm_->setDecimals(4);
    known_distance_mm_->setValue(10.0);
    known_distance_mm_->setSuffix(QStringLiteral(" mm"));
    calibrate_button_ = new QPushButton(tr("两点标定 / Two-point Scale"), calibration_group);
    clear_calibration_button_ = new QPushButton(tr("清除标定 / Clear"), calibration_group);
    calibration_state_ = new QLabel(tr("未标定，位移单位为 px / Not calibrated"), calibration_group);
    calibration_state_->setWordWrap(true);
    calibration_form->addRow(tr("已知距离 / Known Distance"), known_distance_mm_);
    calibration_form->addRow(calibrate_button_);
    calibration_form->addRow(clear_calibration_button_);
    calibration_form->addRow(calibration_state_);
    auto* planned_tools = new QGroupBox(tr("专业工具 / Planned"), tools_page);
    auto* planned_layout = new QVBoxLayout(planned_tools);
    auto* planned_label = new QLabel(
        tr("ROI与蒙版 · 散斑质量 · 点/线/区域探针 · 虚拟引伸计 · 应变花 · 色标锁定 · 报告"),
        planned_tools);
    planned_label->setWordWrap(true);
    planned_label->setObjectName(QStringLiteral("mutedLabel"));
    planned_layout->addWidget(planned_label);
    tools_page_layout->addWidget(display_group);
    tools_page_layout->addWidget(calibration_group);
    tools_page_layout->addWidget(planned_tools);
    tools_page_layout->addStretch(1);

    property_tabs_ = new QTabWidget(body_splitter);
    property_tabs_->setMinimumWidth(340);
    property_tabs_->setMaximumWidth(430);
    property_tabs_->addTab(make_scroll_panel(connection_page, property_tabs_), tr("设备"));
    property_tabs_->addTab(make_scroll_panel(measurement_page, property_tabs_), tr("测量"));
    property_tabs_->addTab(make_scroll_panel(config_page, property_tabs_), tr("相机/DIC"));
    property_tabs_->addTab(make_scroll_panel(tools_page, property_tabs_), tr("工具"));

    auto* center_splitter = new QSplitter(Qt::Vertical, body_splitter);
    center_splitter->setChildrenCollapsible(false);
    view_tabs_ = new QTabWidget(center_splitter);
    auto* preview_page = new QWidget(view_tabs_);
    auto* preview_layout = new QVBoxLayout(preview_page);
    preview_layout->setContentsMargins(6, 6, 6, 6);
    preview_view_ = new PreviewView(preview_page);
    preview_layout->addWidget(preview_view_, 1);
    auto* field_page = new QWidget(view_tabs_);
    auto* field_layout = new QVBoxLayout(field_page);
    field_layout->setContentsMargins(6, 6, 6, 6);
    field_view_ = new FieldView(field_page);
    field_layout->addWidget(field_view_, 1);
    view_tabs_->addTab(preview_page, tr("原图与预览 / Image"));
    view_tabs_->addTab(field_page, tr("云图 / Field Map"));

    analysis_tabs_ = new QTabWidget(center_splitter);
    signal_view_ = new SignalView(analysis_tabs_);
    log_ = new QPlainTextEdit(analysis_tabs_);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setPlaceholderText(tr("英文参数名和原始日志保留在这里 / Raw protocol logs"));
    analysis_tabs_->addTab(signal_view_, tr("测点曲线与 FFT / History & FFT"));
    analysis_tabs_->addTab(log_, tr("事件与协议日志 / Event Log"));
    center_splitter->addWidget(view_tabs_);
    center_splitter->addWidget(analysis_tabs_);
    center_splitter->setStretchFactor(0, 7);
    center_splitter->setStretchFactor(1, 3);
    center_splitter->setSizes({560, 240});

    body_splitter->addWidget(sidebar);
    body_splitter->addWidget(center_splitter);
    body_splitter->addWidget(property_tabs_);
    body_splitter->setStretchFactor(0, 0);
    body_splitter->setStretchFactor(1, 1);
    body_splitter->setStretchFactor(2, 0);
    body_splitter->setSizes({205, 900, 375});

    workspace_root->addWidget(app_header);
    workspace_root->addWidget(command_bar);
    workspace_root->addWidget(body_splitter, 1);

    application_pages_->addWidget(project_center);
    application_pages_->addWidget(workspace);
    setCentralWidget(application_pages_);

    auto add_status = [this](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("statusChip"));
        statusBar()->addWidget(label);
        return label;
    };
    statusBar()->setSizeGripEnabled(false);
    edge_status_ = add_status(tr("Edge：离线"));
    stream_status_ = add_status(tr("Streams：离线"));
    frame_status_ = add_status(tr("Frames: 0"));
    latency_status_ = add_status(tr("P95: -- ms"));
    dropped_status_ = add_status(tr("Dropped: 0"));
    statusBar()->addPermanentWidget(new QLabel(tr("Disk: --  |  Thermal: --  |  DAQ: Simulated"), this));

    connect(ping_button_, &QPushButton::clicked, this, &MainWindow::ping);
    connect(host_, &QLineEdit::textEdited, this,
            [this](const QString&) { invalidateConnection(); });
    connect(start_button_, &QPushButton::clicked, this, &MainWindow::startMeasurement);
    connect(stop_button_, &QPushButton::clicked, this, &MainWindow::stopMeasurement);
    connect(pause_button_, &QPushButton::clicked, this, &MainWindow::togglePauseMeasurement);
    connect(reset_button_, &QPushButton::clicked, this, &MainWindow::resetFault);
    connect(read_config_button_, &QPushButton::clicked,
            this, &MainWindow::requestConfiguration);
    connect(apply_config_button_, &QPushButton::clicked,
            this, &MainWindow::applyConfiguration);
    connect(save_config_button_, &QPushButton::clicked,
            this, &MainWindow::saveConfiguration);
    connect(calibrate_button_, &QPushButton::clicked,
            this, &MainWindow::beginScaleCalibration);
    connect(clear_calibration_button_, &QPushButton::clicked,
            this, &MainWindow::clearScaleCalibration);
    connect(preview_view_, &PreviewView::normalizedPointClicked,
            this, &MainWindow::handleCalibrationPoint);
    connect(&client_, &EdgeClient::responseReceived, this, &MainWindow::handleResponse);
    connect(&client_, &EdgeClient::requestFailed, this, &MainWindow::handleFailure);
    connect(&result_client_, &ResultClient::resultReceived, this, &MainWindow::handleResult);
    connect(&result_client_, &ResultClient::onlineChanged,
            this, &MainWindow::handleResultStreamState);
    connect(&result_client_, &ResultClient::streamFailed,
            this, &MainWindow::handleResultStreamFailure);
    connect(&preview_client_, &PreviewClient::previewReceived,
            this, &MainWindow::handlePreview);
    connect(&preview_client_, &PreviewClient::onlineChanged,
            this, &MainWindow::handlePreviewStreamState);
    connect(&preview_client_, &PreviewClient::streamFailed,
            this, &MainWindow::handlePreviewStreamFailure);
    connect(field_component_, &QComboBox::currentIndexChanged,
            field_view_, &FieldView::setComponentIndex);
    connect(field_component_, &QComboBox::currentIndexChanged,
            signal_view_, &SignalView::setComponentIndex);
    connect(point_index_, &QSpinBox::valueChanged,
            signal_view_, &SignalView::setPointIndex);
    connect(field_view_, &FieldView::pointSelected,
            point_index_, &QSpinBox::setValue);
    connect(project_center_button, &QToolButton::clicked, this, &MainWindow::showProjectCenter);
    connect(new_project_button, &QPushButton::clicked, this, [this] {
        bool accepted = false;
        const QString name = QInputDialog::getText(
            this, tr("新建项目 / New Project"), tr("项目名称 / Project Name"),
            QLineEdit::Normal,
            QStringLiteral("DIC-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmm")),
            &accepted).trimmed();
        if (accepted && !name.isEmpty()) showWorkspace(name);
    });
    connect(open_project_button, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(
            this, tr("打开项目目录 / Open Project Directory"));
        if (!path.isEmpty()) showWorkspace(QFileInfo(path).fileName());
    });
    connect(quick_demo_button, &QPushButton::clicked, this, [this] {
        showWorkspace(tr("快速模拟演示 / Quick Demo"));
        host_->setText(QStringLiteral("127.0.0.1"));
        appendLog(QStringLiteral("INFO"), tr("快速演示已选择本机DIC Edge"));
        QTimer::singleShot(100, this, &MainWindow::ping);
    });
    connect(mode_button_, &QToolButton::toggled, this, &MainWindow::setExpertMode);
    connect(theme_button_, &QToolButton::clicked, this, [this] {
        dark_theme_ = !dark_theme_;
        applyStudioTheme();
    });
    connect(freeze_button, &QPushButton::toggled, this, [this](bool frozen) {
        view_tabs_->setUpdatesEnabled(!frozen);
        analysis_tabs_->setUpdatesEnabled(!frozen);
        appendLog(QStringLiteral("INFO"), frozen ? tr("Studio显示已冻结，Edge继续测量")
                                                  : tr("Studio显示已恢复"));
    });
    connect(workflow_navigation_, &QListWidget::currentRowChanged, this, [this](int row) {
        switch (row) {
            case 0: showProjectCenter(); break;
            case 1: view_tabs_->setCurrentIndex(0); property_tabs_->setCurrentIndex(0); break;
            case 2: view_tabs_->setCurrentIndex(0); property_tabs_->setCurrentIndex(3); break;
            case 3: view_tabs_->setCurrentIndex(0); property_tabs_->setCurrentIndex(1); break;
            case 4: view_tabs_->setCurrentIndex(1); property_tabs_->setCurrentIndex(1); break;
            case 5: view_tabs_->setCurrentIndex(1); property_tabs_->setCurrentIndex(3);
                    analysis_tabs_->setCurrentIndex(0); break;
            default: break;
        }
    });
    connect(status_timer_, &QTimer::timeout, this, &MainWindow::refreshStatus);
    status_timer_->start(1000);

    QSettings settings;
    dark_theme_ = settings.value(QStringLiteral("appearance/darkTheme"), false).toBool();
    expert_mode_ = settings.value(QStringLiteral("appearance/expertMode"), false).toBool();
    applyStudioTheme();
    setExpertMode(expert_mode_);
    workflow_navigation_->setCurrentRow(1);
    application_pages_->setCurrentIndex(0);
    if (qApp->property("studioSnapshotWorkspace").toBool()) {
        showWorkspace(tr("界面验收项目 / UI Review"));
    }
}

void MainWindow::applyStudioTheme() {
    qApp->setStyleSheet(studio_style_sheet(dark_theme_));
    theme_button_->setText(dark_theme_ ? tr("浅色 / Light") : tr("深色 / Dark"));
    theme_button_->setToolTip(
        dark_theme_ ? tr("切换到浅色专业测量界面") : tr("切换到深色低照度界面"));
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/darkTheme"), dark_theme_);
}

void MainWindow::setExpertMode(bool enabled) {
    expert_mode_ = enabled;
    mode_button_->blockSignals(true);
    mode_button_->setChecked(enabled);
    mode_button_->setText(enabled ? tr("专家模式 / Expert") : tr("操作员模式 / Operator"));
    mode_button_->blockSignals(false);
    if (expert_parameters_) expert_parameters_->setVisible(enabled);
    if (property_tabs_) property_tabs_->setTabVisible(2, enabled);
    if (!enabled && property_tabs_ && property_tabs_->currentIndex() == 2) {
        property_tabs_->setCurrentIndex(1);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/expertMode"), enabled);
}

void MainWindow::showWorkspace(const QString& project_name) {
    current_project_->setText(project_name.isEmpty() ? tr("未命名项目 / Untitled Project")
                                                     : project_name);
    application_pages_->setCurrentIndex(1);
    if (workflow_navigation_->currentRow() <= 0) workflow_navigation_->setCurrentRow(1);
}

void MainWindow::showProjectCenter() {
    application_pages_->setCurrentIndex(0);
    workflow_navigation_->blockSignals(true);
    workflow_navigation_->setCurrentRow(0);
    workflow_navigation_->blockSignals(false);
}

void MainWindow::ping() {
    connected_ = false;
    send(QStringLiteral("PING"));
}
void MainWindow::refreshStatus() {
    if (connected_ && !client_.busy()) send(QStringLiteral("STATUS"));
}
void MainWindow::startMeasurement() {
    const QString session = session_id_->text().trimmed();
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    if (!allowed.match(session).hasMatch()) {
        handleFailure(tr("Session ID 只能包含字母、数字、点、下划线和连字符"));
        return;
    }
    QString command = QStringLiteral("START session_id=%1").arg(session);
    if (millimeters_per_pixel_ > 0.0) {
        command += QStringLiteral(" mm_per_pixel=%1")
                       .arg(millimeters_per_pixel_, 0, 'g', 12);
    }
    send(command);
}
void MainWindow::stopMeasurement() { send(QStringLiteral("STOP")); }
void MainWindow::togglePauseMeasurement() {
    if (!pause_button_->property("paused").toBool()) {
        send(QStringLiteral("PAUSE"));
        return;
    }
    const auto choice = QMessageBox::warning(
        this, tr("恢复记录 / Resume Recording"),
        tr("暂停期间如果试样、相机或夹具发生移动，原参考关系可能失效。\n\n"
           "确认未发生移动后再继续原参考；否则取消并建立新的测量分段。"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice == QMessageBox::Yes) send(QStringLiteral("RESUME"));
}
void MainWindow::resetFault() { send(QStringLiteral("RESET")); }
void MainWindow::requestConfiguration() { send(QStringLiteral("CONFIG")); }
void MainWindow::applyConfiguration() {
    const auto choice = QMessageBox::question(
        this, tr("应用测量参数 / Apply Parameters"),
        tr("将把当前相机 ROI、曝光和 DIC 参数发送到 Edge，并在空闲状态重建流水线。\n"
           "参数变化会影响后续结果的可比性，是否继续？"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) return;
    const QString command = QStringLiteral(
        "CONFIG camera.exposure_us=%1 camera.gain_db=%2 camera.external_trigger=%3 "
        "camera.offset_x=%4 camera.offset_y=%5 image.width=%6 image.height=%7 "
        "image.fps=%8 dic.subset_radius=%9 dic.grid_step=%10 dic.search_radius=%11 "
        "dic.quality_threshold=%12")
        .arg(exposure_us_->value(), 0, 'g', 12)
        .arg(gain_db_->value(), 0, 'g', 12)
        .arg(external_trigger_->isChecked() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(roi_offset_x_->value())
        .arg(roi_offset_y_->value())
        .arg(image_width_->value())
        .arg(image_height_->value())
        .arg(image_fps_->value(), 0, 'g', 12)
        .arg(subset_radius_->value())
        .arg(grid_step_->value())
        .arg(search_radius_->value())
        .arg(quality_threshold_->value(), 0, 'g', 12);
    send(command);
}
void MainWindow::saveConfiguration() {
    const auto choice = QMessageBox::question(
        this, tr("保存 Edge 配置 / Save Edge Configuration"),
        tr("将把当前参数写为 Edge 的下次启动配置，并保留上一版 .bak。是否继续？"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice == QMessageBox::Yes) send(QStringLiteral("CONFIG SAVE"));
}

void MainWindow::beginScaleCalibration() {
    calibration_points_.clear();
    preview_view_->setCalibrationMarkers(calibration_points_);
    calibration_collecting_ = true;
    calibration_state_->setText(
        tr("请在预览图点击第 1 个点 / Click calibration point 1"));
}

void MainWindow::clearScaleCalibration() {
    calibration_collecting_ = false;
    calibration_points_.clear();
    preview_view_->setCalibrationMarkers(calibration_points_);
    millimeters_per_pixel_ = 0.0;
    field_view_->setDisplacementScale(1.0, QStringLiteral("px"));
    signal_view_->setDisplacementScale(1.0, QStringLiteral("px"));
    field_component_->setItemText(0, QStringLiteral("U [px]"));
    field_component_->setItemText(1, QStringLiteral("V [px]"));
    calibration_state_->setText(tr("未标定，位移单位为 px / Not calibrated"));
}

void MainWindow::handleCalibrationPoint(const QPointF& normalized_point) {
    if (!calibration_collecting_) return;
    calibration_points_.push_back(normalized_point);
    preview_view_->setCalibrationMarkers(calibration_points_);
    if (calibration_points_.size() == 1) {
        calibration_state_->setText(
            tr("请点击第 2 个点 / Click calibration point 2"));
        return;
    }
    calibration_collecting_ = false;
    const double dx = (calibration_points_[1].x() - calibration_points_[0].x()) *
                      image_width_->value();
    const double dy = (calibration_points_[1].y() - calibration_points_[0].y()) *
                      image_height_->value();
    const double pixel_distance = std::hypot(dx, dy);
    if (pixel_distance < 1.0) {
        clearScaleCalibration();
        handleFailure(tr("标定点距离过小，请重新选择 / Calibration points are too close"));
        return;
    }
    millimeters_per_pixel_ = known_distance_mm_->value() / pixel_distance;
    field_view_->setDisplacementScale(millimeters_per_pixel_, QStringLiteral("mm"));
    signal_view_->setDisplacementScale(millimeters_per_pixel_, QStringLiteral("mm"));
    field_component_->setItemText(0, QStringLiteral("U [mm]"));
    field_component_->setItemText(1, QStringLiteral("V [mm]"));
    calibration_state_->setText(
        tr("标定完成 / Calibrated: %1 mm/px (%2 px)")
            .arg(millimeters_per_pixel_, 0, 'g', 8)
            .arg(pixel_distance, 0, 'f', 2));
}

void MainWindow::handleResponse(const QString& response) {
    appendLog(response.startsWith(QStringLiteral("OK ")) ? QStringLiteral("INFO")
                                                         : QStringLiteral("ERROR"), response);
    connected_ = response.startsWith(QStringLiteral("OK "));
    connection_state_->setText(connected_ ? tr("已连接 / Online")
                                          : tr("连接异常 / Error"));
    edge_status_->setText(connected_ ? tr("Edge：在线") : tr("Edge：异常"));
    const QRegularExpression result_port_expression(
        QStringLiteral("(?:^| )result_port=([0-9]+)"));
    const auto result_port_match = result_port_expression.match(response);
    if (result_port_match.hasMatch()) {
        const int advertised_port = result_port_match.captured(1).toInt();
        if (advertised_port >= 1 && advertised_port <= 65535) {
            result_port_->setValue(advertised_port);
        }
    }
    const QRegularExpression preview_port_expression(
        QStringLiteral("(?:^| )preview_port=([0-9]+)"));
    const auto preview_port_match = preview_port_expression.match(response);
    if (preview_port_match.hasMatch()) {
        const int advertised_port = preview_port_match.captured(1).toInt();
        if (advertised_port >= 0 && advertised_port <= 65535) {
            preview_port_->setValue(advertised_port);
        }
    }
    result_client_.start(currentHost(), currentResultPort());
    if (currentPreviewPort() != 0) {
        preview_client_.start(currentHost(), currentPreviewPort());
    } else {
        preview_client_.stop();
    }
    if (response.startsWith(QStringLiteral("OK STATUS "))) {
        const QString fields = response.mid(QStringLiteral("OK STATUS ").size());
        const QRegularExpression state_expression(QStringLiteral("(?:^| )state=([^ ]+)"));
        const auto match = state_expression.match(fields);
        if (match.hasMatch()) {
            const QString state = match.captured(1);
            measurement_state_->setText(tr("状态 / State: %1").arg(state));
            start_button_->setEnabled(state == QStringLiteral("idle"));
            const bool active = state == QStringLiteral("measuring") ||
                                state == QStringLiteral("paused");
            stop_button_->setEnabled(active);
            pause_button_->setEnabled(active);
            const bool paused = state == QStringLiteral("paused");
            pause_button_->setProperty("paused", paused);
            pause_button_->setText(paused ? tr("继续记录 / Resume")
                                          : tr("暂停记录 / Pause Recording"));
            reset_button_->setEnabled(state == QStringLiteral("faulted"));
            apply_config_button_->setEnabled(state == QStringLiteral("idle"));
            save_config_button_->setEnabled(state == QStringLiteral("idle"));
        }
        const auto value = [&fields](const QString& name) {
            const QRegularExpression expression(
                QStringLiteral("(?:^| )%1=([^ ]+)").arg(QRegularExpression::escape(name)));
            const auto field_match = expression.match(fields);
            return field_match.hasMatch() ? field_match.captured(1) : QString{};
        };
        metrics_->setText(
            tr("帧 / Frames  captured: %1, processed: %2, dropped: %3, timeouts: %4 | "
               "当前处理 / Processing: %5 ms")
                .arg(value(QStringLiteral("captured")),
                     value(QStringLiteral("processed")),
                     value(QStringLiteral("dropped")),
                     value(QStringLiteral("timeouts")),
                     value(QStringLiteral("processing_ms"))));
        performance_metrics_->setText(
            tr("性能 / Performance  Pipeline P50/P95/P99: %1/%2/%3 ms | "
               "GPU H2D/Kernel/D2H: %4/%5/%6 ms | Writer queue: %7 (max %8), overruns: %9")
                .arg(value(QStringLiteral("pipeline_p50_ms")),
                     value(QStringLiteral("pipeline_p95_ms")),
                     value(QStringLiteral("pipeline_p99_ms")),
                     value(QStringLiteral("h2d_ms")),
                     value(QStringLiteral("kernel_ms")),
                     value(QStringLiteral("d2h_ms")),
                     value(QStringLiteral("writer_queue")),
                     value(QStringLiteral("writer_queue_max")),
                     value(QStringLiteral("writer_overruns"))));
        const QString frame = value(QStringLiteral("result_frame"));
        const QString points = value(QStringLiteral("result_points"));
        const QString ratio = value(QStringLiteral("valid_ratio"));
        const QString mean_u = value(QStringLiteral("mean_u"));
        const QString mean_v = value(QStringLiteral("mean_v"));
        const QString mean_exx = value(QStringLiteral("mean_exx"));
        const QString mean_eyy = value(QStringLiteral("mean_eyy"));
        const QString mean_exy = value(QStringLiteral("mean_exy"));
        const QString processing = value(QStringLiteral("processing_ms"));
        frame_status_->setText(tr("Frames：%1 / %2")
                                   .arg(value(QStringLiteral("processed")),
                                        value(QStringLiteral("captured"))));
        latency_status_->setText(tr("P95：%1 ms").arg(
            value(QStringLiteral("pipeline_p95_ms"))));
        dropped_status_->setText(tr("Dropped：%1").arg(
            value(QStringLiteral("dropped"))));
        if (!frame.isEmpty()) result_frame_->setText(frame);
        if (!points.isEmpty()) result_points_->setText(points);
        if (!ratio.isEmpty()) {
            valid_ratio_->setText(QStringLiteral("%1 %").arg(ratio.toDouble() * 100.0, 0, 'f', 1));
        }
        if (!mean_u.isEmpty() && !mean_v.isEmpty()) {
            if (millimeters_per_pixel_ > 0.0) {
                mean_displacement_->setText(
                    QStringLiteral("U=%1 px (%2 mm), V=%3 px (%4 mm)")
                        .arg(mean_u)
                        .arg(mean_u.toDouble() * millimeters_per_pixel_, 0, 'g', 6)
                        .arg(mean_v)
                        .arg(mean_v.toDouble() * millimeters_per_pixel_, 0, 'g', 6));
            } else {
                mean_displacement_->setText(
                    QStringLiteral("U=%1 px, V=%2 px").arg(mean_u, mean_v));
            }
        }
        if (!mean_exx.isEmpty() && !mean_eyy.isEmpty() && !mean_exy.isEmpty()) {
            mean_strain_->setText(QStringLiteral("εₓₓ=%1, εᵧᵧ=%2, εₓᵧ=%3 (tensor)")
                                      .arg(mean_exx, mean_eyy, mean_exy));
        }
        if (!processing.isEmpty()) processing_time_->setText(processing + QStringLiteral(" ms"));
    }
    if (response.startsWith(QStringLiteral("OK CONFIG ")) ||
        response.startsWith(QStringLiteral("OK CONFIGURED "))) {
        const auto value = [&response](const QString& name) {
            const QRegularExpression expression(
                QStringLiteral("(?:^| )%1=([^ ]+)").arg(QRegularExpression::escape(name)));
            const auto field_match = expression.match(response);
            return field_match.hasMatch() ? field_match.captured(1) : QString{};
        };
        const QString camera_backend = value(QStringLiteral("camera.backend"));
        const QString dic_backend = value(QStringLiteral("dic.backend"));
        backend_summary_->setText(
            QStringLiteral("Camera backend: %1    DIC backend: %2")
                .arg(camera_backend, dic_backend));
        const auto set_double = [&value](QDoubleSpinBox* box, const QString& name) {
            const QString text = value(name);
            if (!text.isEmpty()) box->setValue(text.toDouble());
        };
        const auto set_int = [&value](QSpinBox* box, const QString& name) {
            const QString text = value(name);
            if (!text.isEmpty()) box->setValue(text.toInt());
        };
        set_double(exposure_us_, QStringLiteral("camera.exposure_us"));
        set_double(gain_db_, QStringLiteral("camera.gain_db"));
        external_trigger_->setChecked(
            value(QStringLiteral("camera.external_trigger")) == QStringLiteral("true"));
        set_int(roi_offset_x_, QStringLiteral("camera.offset_x"));
        set_int(roi_offset_y_, QStringLiteral("camera.offset_y"));
        set_int(image_width_, QStringLiteral("image.width"));
        set_int(image_height_, QStringLiteral("image.height"));
        set_double(image_fps_, QStringLiteral("image.fps"));
        set_int(subset_radius_, QStringLiteral("dic.subset_radius"));
        set_int(grid_step_, QStringLiteral("dic.grid_step"));
        set_int(search_radius_, QStringLiteral("dic.search_radius"));
        set_double(quality_threshold_, QStringLiteral("dic.quality_threshold"));
    }
}

void MainWindow::handleResult(DicResultPtr result) {
    if (!result) return;
    field_view_->setResult(result);
    if (!result->points.empty()) {
        point_index_->setMaximum(static_cast<int>(result->points.size() - 1));
    }
    signal_view_->appendResult(result);
    const auto summary = p2dic::summarize_result(*result);
    result_frame_->setText(QString::number(summary.frame_sequence));
    result_points_->setText(QString::number(summary.point_count));
    valid_ratio_->setText(QStringLiteral("%1 %").arg(summary.valid_ratio * 100.0, 0, 'f', 1));
    if (millimeters_per_pixel_ > 0.0) {
        mean_displacement_->setText(
            QStringLiteral("U=%1 px (%2 mm), V=%3 px (%4 mm)")
                .arg(summary.mean_u, 0, 'f', 3)
                .arg(summary.mean_u * millimeters_per_pixel_, 0, 'g', 6)
                .arg(summary.mean_v, 0, 'f', 3)
                .arg(summary.mean_v * millimeters_per_pixel_, 0, 'g', 6));
    } else {
        mean_displacement_->setText(
            QStringLiteral("U=%1 px, V=%2 px")
                .arg(summary.mean_u, 0, 'f', 3)
                .arg(summary.mean_v, 0, 'f', 3));
    }
    mean_strain_->setText(
        QStringLiteral("εₓₓ=%1, εᵧᵧ=%2, εₓᵧ=%3 (tensor)")
            .arg(summary.mean_exx, 0, 'f', 6)
            .arg(summary.mean_eyy, 0, 'f', 6)
            .arg(summary.mean_exy, 0, 'f', 6));
    processing_time_->setText(QStringLiteral("%1 ms").arg(summary.processing_ms, 0, 'f', 3));
}

void MainWindow::handleResultStreamState(bool online) {
    result_stream_state_->setText(
        online ? tr("数据流在线 / Stream Online") : tr("数据流离线 / Stream Offline"));
    stream_status_->setText(online ? tr("Streams：结果在线") : tr("Streams：结果离线"));
}

void MainWindow::handleResultStreamFailure(const QString& message) {
    appendLog(QStringLiteral("STREAM"), message);
}

void MainWindow::handlePreview(PreviewFramePtr preview) {
    preview_view_->setPreview(std::move(preview));
}

void MainWindow::handlePreviewStreamState(bool online) {
    preview_stream_state_->setText(
        online ? tr("预览在线 / Preview Online") : tr("预览离线 / Preview Offline"));
    if (online) stream_status_->setText(tr("Streams：结果＋预览在线"));
}

void MainWindow::handlePreviewStreamFailure(const QString& message) {
    appendLog(QStringLiteral("PREVIEW"), message);
}

void MainWindow::handleFailure(const QString& message) {
    connected_ = false;
    connection_state_->setText(tr("连接异常 / Error"));
    edge_status_->setText(tr("Edge：异常"));
    stream_status_->setText(tr("Streams：离线"));
    result_client_.stop();
    preview_client_.stop();
    appendLog(QStringLiteral("ERROR"), message);
}

void MainWindow::invalidateConnection() {
    connected_ = false;
    result_client_.stop();
    preview_client_.stop();
    connection_state_->setText(tr("未连接 / Offline"));
    result_stream_state_->setText(tr("数据流离线 / Stream Offline"));
    preview_stream_state_->setText(tr("预览离线 / Preview Offline"));
    edge_status_->setText(tr("Edge：离线"));
    stream_status_->setText(tr("Streams：离线"));
}

void MainWindow::send(const QString& command) {
    appendLog(QStringLiteral("TX"), command);
    client_.sendCommand(currentHost(), currentPort(), command);
}

void MainWindow::appendLog(const QString& level, const QString& message) {
    log_->appendPlainText(QStringLiteral("%1 [%2] %3")
                              .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), level, message));
}

QString MainWindow::currentHost() const { return host_->text().trimmed(); }
quint16 MainWindow::currentPort() const { return static_cast<quint16>(port_->value()); }
quint16 MainWindow::currentResultPort() const {
    return static_cast<quint16>(result_port_->value());
}
quint16 MainWindow::currentPreviewPort() const {
    return static_cast<quint16>(preview_port_->value());
}
