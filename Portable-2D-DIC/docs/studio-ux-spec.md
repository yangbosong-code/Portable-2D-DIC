# DIC Studio 产品与界面规范 v1.0

更新日期：2026-08-09。本文件是后续界面开发的需求基线；未完成的控件必须标注“规划中”，
不能用静态演示数据冒充已实现能力。

## 1. 产品定位

产品名为 **Portable 2D-DIC Measurement System**。`DIC Edge` 在计算盒内独立完成相机、
DIC、应变和保存；Windows `DIC Studio` 只负责项目管理、远程控制、显示、回放和报告。
网络断开不能中止 Edge 当前实验。GalaxyView 可以用于驱动检查和维护，但不是交付平台的
计算组件，同一时刻不得与 Edge 争用相机。

界面参考 VIC-2D、ZEISS CORRELATE、MatchID、LaVision StrainMaster 和 XTDIC 的共同工作
模式；测量流程、验证和术语参考 iDICs Good Practices Guide（Edition 2, 2025）与
VDI/VDE 2626 Part 1。真实硬件、标定和不确定度尚未验收前，不宣称符合或通过上述标准。

## 2. 默认工作方式

- 默认语言：中文；参数名、协议字段和日志保留英文；
- 默认主题：浅色专业测量界面；右上角一键切换深色；
- 默认模式：操作员模式；右上角一键切换专家模式；
- 高风险修改不设密码，但必须二次确认并写日志；
- 目标屏幕：1920×1080，Windows 100%～150% 缩放；最低 1100×720；
- 主视图约占工作区 65%，属性栏在右侧，曲线/FFT 在下方；
- Edge 计算/保存帧率与 Studio 预览刷新率解耦，界面卡顿不能拖慢计算盒。

## 3. 启动与项目中心

启动后首先显示项目中心，而不是直接暴露全部相机参数：新建项目、打开项目目录、最近项目、
快速模拟演示，以及自动发现的 Edge 设备与在线状态。

项目保存为普通目录加一个可读 JSON 描述文件，不使用不可恢复的单体巨大文件：

```text
project-name/
├─ project.p2dic.json       # 项目元数据、坐标/单位、处理模板和版本
├─ calibration/             # 标定图像、标定参数、验证结果
├─ sessions/
│  └─ session-id/
│     ├─ manifest.json      # 不可变会话清单、参数快照、软件/硬件版本
│     ├─ results.p2dic      # 逐帧 CRC 的二进制 DIC 结果
│     ├─ diagnostics/       # 参考帧、关键帧、异常帧和质量证据
│     └─ exports/           # CSV、图片、视频等派生文件
├─ reports/                 # PDF，后续增加 DOCX
└─ project.p2dic.json.bak
```

项目描述文件自动保存并保留 `.bak`；原始会话和标定记录不可静默覆盖，新分析写入派生版本。

## 4. 六步测量工作流

左侧固定工作流为：项目、设备、图像与标定、采集、分析、报告。操作员模式按顺序引导，未满足
前置条件时解释原因；专家模式允许直接跳转任意页面。顶栏只保留高频动作：会话名、开始、暂停
记录、停止、冻结显示、设置参考和故障复位。

“暂停记录”与“冻结显示”必须是两个独立状态：冻结显示时 Edge 继续采集/计算/保存；暂停记录
时 Edge 保持会话但不把暂停期间的预览帧计为丢帧。恢复前比较参考关系，若试样可能移动，提示
“参考关系可能失效”，提供继续原参考、建立新参考分段或取消恢复，并把选择写入 manifest。

## 5. 工作区结构

- 中央上部：`相机预览`、`场图叠加`；默认“云图半透明覆盖原图”；
- 中央下部：测点/虚拟计时器曲线、FFT、事件和日志；
- 右侧属性：设备、测量、专家参数、显示与探针；
- 底部状态条：Edge、结果/预览流、processed/captured、P95、Dropped、磁盘、温度和 DAQ；
- 面板使用 splitter，内容过长时在面板内滚动，禁止因为 Host 或状态文本增长而撑长窗口。

第二阶段补充 ROI、多边形 mask、散斑质量、探针、线/面积统计、虚拟引伸计、应变片/应变
花、时间轴和交互色标；未实现项只显示禁用入口与阶段说明。

## 6. 坐标、变量和单位

图像存储坐标为 `x` 向右、`y` 向下；正式结果坐标为试样 `X` 向右、`Y` 向上。视图始终画出
坐标轴，坐标变换、旋转和单位写入项目与会话日志。

显示符号使用本地矢量字体/富文本或本地 SVG，达到 LaTeX 排版观感；禁止从网页截取符号贴图。
内部字段保持稳定英文名。变量定义如下：

| 界面符号 | 内部字段 | 定义 |
|---|---|---|
| `U`, `V`, `|D|` | `u`, `v`, `magnitude` | X/Y 位移和位移模量 |
| `εₓₓ`, `εᵧᵧ` | `exx`, `eyy` | 正应变 |
| `εₓᵧ` | `exy` | 张量剪切应变 |
| `γₓᵧ` | `gxy` | 工程剪切应变，`γₓᵧ = 2 εₓᵧ` |
| `ε₁`, `ε₂` | `principal_1`, `principal_2` | 主应变 |

不得用含糊的 `exy` 文案同时表示两种剪切应变。标定前位移单位为 px，标定后为 mm；应变
支持无量纲、µε 和 %，视图和导出必须记录实际单位。原始位移永远保留，平滑、应变窗口和
滤波是可追溯的后处理参数，不做隐藏平滑。

## 7. 场图、曲线与 FFT

- 有符号场默认使用以零为中心的色盲友好发散色图；
- 位移模量、相关质量等非负量使用 Viridis 类顺序色图；
- 无效点透明或灰色，并与低值区明确区分；
- 默认 robust range，提供自动、对称、固定、百分位和锁定范围；
- 色标必须显示变量、单位、上下限和无效点含义；禁止 rainbow 默认色图；
- 曲线按单位分组，默认不使用双 Y 轴；探针、线、面积和虚拟引伸计随项目保存；
- FFT 使用真实采样时间，检查时间连续性和丢帧，默认 Hann 窗；不连续数据必须警告。

## 8. 数据、报告和告警

18 MP Mono8 一帧约 18.45 MB，21.4 fps 约 395 MB/s、约 1.42 TB/h。默认保存参考/标定/
关键/异常图像和完整 DIC 结果，不默认保存全部原始帧；专家可开启全原始保存，但必须先检查
NVMe 连续写入、剩余空间和预计可记录时长。

告警分信息、警告、错误、致命四级。磁盘不足、写盘队列溢出、相机断连、温度超限、CRC
失败和参考失效必须出现在状态栏、事件记录和报告。结果使用 CRC、manifest 和恢复扫描；发布
版本再增加文件级 SHA-256 清单。

报告默认中文并保留英文参数名，可选双语模板；PDF 为正式输出，DOCX 后续实现。报告只能
总结实际数据、处理参数、质量证据和限制，不自动生成超出证据的材料结论。

## 9. 分阶段交付

- Phase 1（当前）：项目中心、双模式、双主题、工作流、专业布局、现有功能完整迁移；
- Phase 2：ROI/mask、散斑质量、交互色标、探针/统计、时间轴；
- Phase 3：项目持久化、恢复/回放、导出、PDF 报告、模拟载荷接口；
- Phase 4：真实 ME2P-1840-21U3M、Jetson、DAQ/触发和计量学验收。

权威入口：

- https://www.idics.org/guide/
- https://www.vdi.de/en/home/vdi-standards/details/vdivde-2626-blatt-1-optical-measuring-procedures-digital-image-correlation-basics-acceptance-test-and-iterim-check
- https://www.correlatedsolutions.com/vic-2d/
- https://www.zeiss.com/metrology/en/software/zeiss-correlate/features.html
- https://www.matchid.eu/software
- https://www.digitalimagecorrelation.com/en/products/strainmaster/system-components/dic-software/index.php
- https://www.xtop3d.com/software-details/xtdic.html
