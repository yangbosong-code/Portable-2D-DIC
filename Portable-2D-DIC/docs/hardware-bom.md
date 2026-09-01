# 便携计算盒 BOM 与采购结论

包含相机、镜头、照明、机械、标定、触发以及逐步安装的完整版见
`docs/hardware-procurement-installation.md`。本文件仅保留计算盒采购结论。

资料核对日期：2026-08-09。价格和可售套装变化快，下单当天必须在 NVIDIA 官方或授权渠道
复核；只买全新设备。以下预算只计算计算机/计算盒，不包含相机、镜头、照明、标定板和结构件。

## 当前采购结论

现在最合理的顺序是：**先完成算法和 Jetson 交叉编译验证，再购买推荐档计算盒**。已有
Ultra 9 285H + RTX 5060 Laptop 8GB 足够承担开发、CUDA 性能分析和界面验证，但最终交付
仍使用独立计算盒，笔记本只运行 DIC Studio。

预算 2000～3000 元时，首选 **Jetson Orin Nano Super Developer Kit 8GB**；允许提高到
4000 元时，仍应优先增加高耐久 NVMe、散热、供电和可靠 USB/网络，而不是为名义 TOPS 牺牲
存储与稳定性。Orin NX 16GB 是增强候选，但完整全新套装通常会突破 4000 元，且不是当前
第一采购建议。

## 三档方案

| 档位 | 核心平台 | 适用目标 | 主要限制 | 计算盒预算 |
|---|---|---|---|---:|
| 基础验证档 | 现有 RTX 5060 笔记本 + 模拟 Edge | 代码、CUDA、UI、回放和基准 | 不满足独立交付 | 已有设备 |
| 推荐交付档 | Jetson Orin Nano Super 8GB 官方开发套件 | 10 fps 底线、15 fps 目标的便携原型 | 8GB 内存；Galaxy ARM/JetPack 必须先确认 | 约 2800～4000 元 |
| 增强研发档 | Orin NX 16GB 模组 + 可靠载板/整机 | 更大 ROI、更复杂算法、多传感器扩展 | 成本、散热和驱动集成显著上升 | 通常高于 4000 元 |

最终帧率不能根据 TOPS 推算。ME2P-1840-21U3M 的 18 MP 输入、USB3 采集、DIC 网格、应变、
写盘和温度必须组成整机基准；采购前以 10/15/21.4 fps 三档验收脚本为依据。

## 推荐交付档必买清单

| 部件 | 最低要求 | 预算参考 | 选择理由 |
|---|---|---:|---|
| Jetson Orin Nano Super Developer Kit 8GB | 全新官方套件，核对完整 MPN/序列号 | 以官方当日价为准 | CUDA、低功耗、小体积、官方载板和电源降低集成风险 |
| 1TB NVMe SSD | M.2 2280、TLC、PCIe、约 600 TBW 或更高 | 350～600 元 | 系统、结果和诊断数据；正式连续写入不用 microSD |
| 散热 | 原装主动散热完整；机箱进出风无遮挡 | 0～250 元 | Jetson 持续性能取决于功耗模式和温度，不只看峰值 |
| 供电 | 使用官方 19V 电源；移动供电后续单独验证 | 套件内/另计 | 避免相机与计算负载瞬态导致掉电 |
| 系统介质 | 16GB 以上 U 盘 | 30～80 元 | 安装/恢复 JetPack；不作为正式数据盘 |
| 网络 | 千兆网优先；Wi-Fi 仅用于控制/降采样显示 | 0～300 元 | 原始图和完整场不应默认经 Wi-Fi 全速传输 |
| USB3 工业线 | 短线、固定、防松、实测满幅稳定 | 相机附件预算 | 18 MP@21.4 fps 对链路和供电敏感 |
| 防护机箱 | 不遮挡风扇、USB3、网口和 NVMe；预留固定孔 | 100～350 元 | 科研/竞赛搬运与防拉扯 |

若需要默认保存全部 Mono8 原始帧，1TB 明显不够：理论数据量约 395 MB/s、约 1.42 TB/h，
还未计文件系统与元数据。推荐默认只保存参考/标定/关键/异常图像和完整 DIC 数值结果；全原始
模式必须使用更大容量高耐久 NVMe，并在开始前显示可录时长。

## 可选扩展（第一阶段只保留软件接口）

| 扩展 | 当前处理 |
|---|---|
| DAQ/载荷采集 | 先实现时间戳、通道、单位和模拟载荷接口，不立即购买硬件 |
| 隔离触发/同步盒 | 等相机 GPIO、电平、试验机接口和同步误差目标明确后选型 |
| 便携路由器 | 现场无法直连网线时再买，优先支持千兆 LAN 和稳定 AP 模式 |
| UPS/电池 | 需要重新做 19V 输出、峰值功率、EMI 和相机稳定性验收 |
| Orin NX 16GB | Nano 实测达不到目标或内存水位不足时再升级 |

## 下单前硬门槛：GalaxySDK ARM64

大恒公开 Galaxy Linux ARM SDK 页面需逐项核对目标 ARMv8、USB3、Mercury 型号和 Ubuntu
版本；NVIDIA JetPack 的 Ubuntu/CUDA 组合也会更新。因此在购买计算盒或刷入最终系统前，
向大恒技术支持提交以下完整信息并取得兼容包或书面建议：

- 相机：ME2P-1840-21U3M，USB3 Vision，Mono8/Mono12；
- 平台：Jetson Orin Nano Super，ARM64；
- 目标 JetPack、Ubuntu、CUDA 版本；
- 4504×4096、Mono8、10/15/21.4 fps；
- 连续取流、外触发、GPIO 和掉线恢复要求。

未确认前，不能把 ARM 相机驱动列为“已完成”。若当前 GalaxySDK 不支持目标 JetPack，优先
选择大恒认可的 JetPack 版本；不要为了追新 CUDA 版本破坏相机驱动。CUDA 13.3 是 Windows
开发机环境，不自动等于 Jetson 的部署版本。

官方入口：

- NVIDIA Jetson Orin Nano Super Developer Kit：
  https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/
- NVIDIA Jetson Orin 系列：
  https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/
- NVIDIA JetPack： https://developer.nvidia.com/embedded/jetpack
- 大恒 Galaxy Linux SDK： https://en.daheng-imaging.com/list-59-1.html

## 验收后再定是否升级

推荐档到货后依次验证：USB3 满幅、10 fps、15 fps、21.4 fps、30 分钟性能、8 小时稳定、
温度/降频、NVMe 写入、网络断开继续测量、相机断开恢复和断电数据恢复。只有当证据显示 GPU
内核、内存或热设计是主要瓶颈时才升级 Orin NX；若瓶颈在算法点数、搜索半径、显示或写盘，
应先优化算法和数据路径。
