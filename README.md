# Portable-2D-DIC
This is a portable real‑time 2D‑DIC software for monocular industrial cameras. It comprises "DIC Edge" on an edge‑computing box and **DIC Studio** for Windows PCs. Image acquisition, DIC processing, strain computation and data storage are executed entirely on the edge box. The Windows PC only undertakes remote control and result visualization.
# Portable 2D-DIC

## 当前能力

- 合成相机、PGM 回放、大恒 GalaxySDK 相机后端；
- CPU ZNCC 教学实现、CPU 亚像素实现、CUDA 网格亚像素实现；
- 双线程实时流水线、固定帧池、过期帧丢弃和失效点恢复；
- 规则网格位移梯度与 `εₓₓ/εᵧᵧ/εₓᵧ (tensor)` 应变；
- 有界异步写盘、版本化 `.p2dic` 二进制结果、逐帧 CRC32、截断恢复和 manifest；
- 可选实时 CSV，并提供 `dic_export_csv` 离线导出工具；
- P50/P95/P99 端到端性能统计、CUDA 分段计时和 `dic_perf_monitor` CSV/JSON 采样；
- 控制、DIC 点场、相机预览三条独立 TCP 通道；
- Qt/C++ 中英双语 Studio：项目中心、操作员/专家模式、浅色/深色主题、六步工作流、
  实时预览、云图、测点曲线和 FFT；
- Studio 远程曝光/增益/触发/ROI/DIC 参数读取、空闲应用和显式持久化；
- START/PAUSE/RESUME/STOP，会话不中断暂停；
- 预览两点 mm/px 标定、云图点击选点，标定比例写入 manifest；
- Windows CPU、CUDA、GalaxySDK、GalaxySDK+CUDA 构建；
- Jetson 构建、systemd 自启动、健康检查与恢复文件。

## 网络端口

| 端口 | 用途 | 默认协议 |
|---:|---|---|
| 17840 | PING、STATUS、CONFIG、START、PAUSE、RESUME、STOP、RESET、SHUTDOWN | UTF-8 行协议 |
| 17841 | 位移/应变/质量完整点场 | 版本化二进制 + CRC32 |
| 17842 | 降采样 Mono8 相机预览 | 版本化二进制 + CRC32 |

## VS Code

按 `Ctrl+Shift+B` 可选择 CPU、Qt Studio、CUDA、GalaxySDK 或 GalaxySDK+CUDA 构建任务；
“运行和调试”中可启动 CPU Edge、CUDA Edge 和 Studio。每个构建任务都会运行对应 CTest。
当前最直接的演示入口是 `Ctrl+Shift+B` 后选择 `DIC: Open demo platform`；它会构建并验证
工程、启动隐藏的模拟 Edge，再打开 Studio 项目中心。

## 命令行示例

```powershell
./tools/cmake-vscode.cmd --preset windows-cuda-release
./tools/cmake-vscode.cmd --build --preset build-windows-cuda
ctest --preset test-windows-cuda --output-on-failure
./out/build/windows-cuda-release/dic_edge.exe --config config/dic-edge.dev.conf
```

另一个终端中：

```powershell
./out/build/windows-cuda-release/dic_ctl.exe PING
./out/build/windows-cuda-release/dic_ctl.exe CONFIG
./out/build/windows-cuda-release/dic_ctl.exe "CONFIG image.fps=15 camera.exposure_us=2000"
./out/build/windows-cuda-release/dic_ctl.exe "CONFIG SAVE"
./out/build/windows-cuda-release/dic_ctl.exe "START session_id=demo-001"
./out/build/windows-cuda-release/dic_ctl.exe STATUS
./out/build/windows-cuda-release/dic_ctl.exe STOP
```

长时间性能采样（Edge 运行期间）：

```powershell
./out/build/windows-cuda-release/dic_perf_monitor.exe 127.0.0.1 17840 1800 perf-30min.csv 1000
./out/build/windows-cuda-release/dic_export_csv.exe sessions/demo-001/results.p2dic demo-001.csv
```

CUDA 构建会让 `FramePool` 优先使用页锁定内存，GalaxySDK 图像直接复制进可异步上传的帧；
若 CUDA 驱动不可用会安全退回普通内存。默认 `dic.inverse_compositional=false`，因为当前
RTX 5060 全幅 A/B 测试中融合 FA-GN 比实验性 IC-GN 更快。

## 文档入口

- `PROJECT_STATUS.md`：真实进度和测试基线；
- `docs/architecture.md`：软件架构与数据流；
- `docs/learning-roadmap.md`：从零学习路线和三人分工；
- `docs/hardware-bom.md`：计算盒采购方案与兼容性风险；
- `docs/hardware-procurement-installation.md`：完整硬件清单、光学选型、装配、Jetson/GalaxySDK
  安装与分级验收；
- `docs/compute-storage-six-option-report.md`：RTX 5060/Nano Super与512GB/1TB/2TB六种组合的
  计算性能、存储时长和采购结论；
- `docs/acceptance-plan.md`：模拟、相机、Jetson 和测量验收；
- `docs/jetson-deployment.md`：Jetson 构建、安装与恢复；
- `docs/requirements.md`：需求基线。
- `docs/studio-ux-spec.md`：DIC Studio 产品、界面、变量、单位和分阶段实现规范。

## 真实性边界

RTX 5060 上的模拟性能不能代替真实相机或 Jetson 验收。GalaxySDK ARM64 与目标 JetPack
的组合、USB3 满幅帧率、真实散斑测量精度和 Jetson 持续性能必须拿到硬件后验证。
