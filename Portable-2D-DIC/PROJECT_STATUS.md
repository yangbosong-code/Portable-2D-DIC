# Portable 2D-DIC 项目状态

更新日期：2026-08-09

## 当前结论

工程已形成可构建、可运行、可自动测试的无相机开发闭环，并在 RTX 5060 Laptop GPU 上
实际执行过 CUDA 代码。Windows 的 GalaxySDK 与 CUDA 组合也已完成编译测试。当前属于
“软件工程版 + 硬件待验收”，不能标记为整机交付完成。

## 已完成

- C++20/CMake 多后端工程：Synthetic、PGM、GalaxySDK；
- CPU ZNCC 教学实现、CPU 亚像素和 CUDA 网格亚像素实现；
- 固定帧池、采集/计算解耦、最新帧优先、过期帧丢弃、失效点恢复；
- 位移、质量、`εₓₓ/εᵧᵧ/εₓᵧ (tensor)` 应变与异步二进制/manifest 保存，可选实时 CSV；
- 有界写盘队列、逐帧 CRC32、截断恢复扫描和离线 CSV 导出；
- 最近 1024 帧的采集/DIC/应变/回调/端到端 P50、P95、P99 统计；
- CUDA 页锁定帧池、非阻塞 stream、异步 H2D/D2H、CUDA event 分段计时和六路融合归约；
- FA-GN/实验性 IC-GN A/B 开关，实测更快的 FA-GN 保持默认；
- Edge 状态机和控制/结果/预览三通道协议，二进制包带 CRC32；
- Qt/C++ 中英双语 Studio Phase 1：项目中心、操作员/专家模式、浅色/深色主题、六步工作流、
  专业测量布局、预览、云图、测点时程和 Hann 窗 FFT；
- 远程相机/ROI/DIC 参数查询、空闲应用、带 `.bak` 的显式配置持久化；
- 会话 PAUSE/RESUME、预览两点 mm/px 标定、manifest schema 2 和云图点击选点；
- CUDA 共享内存归约竞态和 MSVC/Ninja 中文依赖前缀导致的旧对象崩溃修复；
- Windows CPU、Qt、CUDA、GalaxySDK、GalaxySDK+CUDA 构建预设和 VS Code 任务；新增
  `DIC: Open demo platform` 一键构建、启动 Edge 和打开 Studio；
- Jetson 构建脚本、systemd 自启动、健康检查和恢复部署文件；
- 架构、学习路线、BOM、用户手册和分阶段验收方案。

## 可复现模拟性能快照

开发机 RTX 5060 Laptop 8GB、CUDA 13.3；输入 4504×4096 Mono8 合成散斑；网格步长
32 px，共 17,780 点；连续 100 帧整数平移：CUDA 计算平均 2.508 ms，P95 2.792 ms，
最后一帧平均位移 100.979 px，有效点 97.9%。纯计算折算约 399 fps。`grid_step=64`
时平均 1.522 ms、P95 1.784 ms。该基准已包含 H2D、kernel、D2H 和 CPU 结果组装。

该结果不包含真实 USB3 采集、预处理、写盘和网络显示，不能换算成 Jetson 整机 fps，也不能
证明真实散斑精度。

## 软件测试状态

2026-08-09 新界面迁移后，Windows Studio 的连接、协议、算法、流水线、保存、恢复、Edge
自检和部署检查 22/22 通过。`field_view_render`、`signal_view_render`、`preview_view_render`
三个测试在 Qt 6.11 offscreen 插件退出阶段挂起，已从 VS Code 预启动任务隔离，必须另行修复，
不能把它们记为通过。此前 CUDA、GalaxySDK 与组合构建曾全部通过；重新发布时必须重新运行
四套测试并完成 Windows 100%～150% 缩放的人工界面检查。

## Studio 后续阶段

- Phase 2：ROI/mask、散斑质量、交互色标、探针/统计和时间轴；
- Phase 3：项目 JSON 持久化、恢复/回放、导出、PDF 报告和模拟载荷；
- Phase 4：真实相机、Jetson、DAQ/触发与计量学验收。

完整界面与变量规范见 `docs/studio-ux-spec.md`。

## 必须等硬件完成

- ME2P-1840-21U3M 的满幅稳定帧率、曝光、触发、超时和掉帧；
- Galaxy ARM64 SDK 与目标 JetPack/Ubuntu 的兼容性；
- Jetson 上 10/15/21.4 fps 实测、温度、写盘和 8/24 小时稳定性；
- 镜头、散斑、照明、刚体安装、像素到毫米标定和测量不确定度；
- 断电、相机断开、磁盘满、过热和网络中断的整机恢复。

模拟测试不会冒充硬件验收。采购和实验到位后，严格执行 `docs/acceptance-plan.md`。
