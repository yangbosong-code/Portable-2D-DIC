# DIC Edge / DIC Studio 使用手册（开发版）

## 1. 首次运行模拟系统

1. 用 VS Code 打开工程根目录；
2. `Ctrl+Shift+B` 选择 `DIC: Open demo platform`；
3. 任务会先构建并运行 22 项非渲染回归测试，再启动模拟 Edge 和 Studio；
4. 在项目中心点击“快速模拟演示 / Quick Demo”；
5. Studio 使用 `127.0.0.1`，端口 17840/17841/17842 自动连接；
6. 输入会话名并开始，检查预览、位移/应变云图、测点曲线和 FFT；
7. 停止后在 `sessions` 检查 `manifest.json` 与 `results.p2dic`；需要表格时再离线导出 CSV。

Qt 6.11 的三个 offscreen 控件渲染测试当前单独隔离，避免 VS Code 预启动任务挂起；连接、
协议、算法、流水线、保存和 Edge 自检等 22 项仍自动执行。该隔离不代表跳过真实界面检查，
发布前必须在 Windows 桌面手动检查浅色/深色、操作员/专家模式和 100%～150% 缩放。

## 2. 命令行控制

```powershell
./out/build/windows-qt-release/dic_ctl.exe PING
./out/build/windows-qt-release/dic_ctl.exe STATUS
./out/build/windows-qt-release/dic_ctl.exe "START session_id=demo-001"
./out/build/windows-qt-release/dic_ctl.exe PAUSE
./out/build/windows-qt-release/dic_ctl.exe RESUME
./out/build/windows-qt-release/dic_ctl.exe STOP
./out/build/windows-qt-release/dic_ctl.exe RESET
```

`SHUTDOWN` 只关闭 Edge 进程，不关闭计算机。网络断开不会自动停止正在进行的实验。

## 3. 远程参数、ROI 与保存

Studio 右侧“设备/专家参数”页可读取和修改 ExposureTime、Gain、External Trigger、
OffsetX/OffsetY、Width/Height、FPS、SubsetRadius、GridStep、SearchRadius 和
QualityThreshold。修改只允许在 `idle` 状态应用；测量中会被 Edge 拒绝。

“应用”立即重建空闲流水线，但重启会恢复配置文件；确认无误后再点击“保存到 Edge”。保存使用
临时文件提交，并把旧配置保留为同路径的 `.bak`。Edge 必须通过 `--config PATH` 启动才允许保存。

## 4. 两点尺度标定

1. 保证标尺和试件位于同一成像平面；
2. 在预览页输入两个标记之间的已知毫米距离；
3. 点击“两点标定”，依次点击两个标记中心；
4. Studio 显示 mm/px，U/V 云图、摘要和测点曲线切换为毫米；
5. START 时比例写入 `manifest.json`，二进制原始位移仍保存 px。

## 5. 性能记录与离线导出

```powershell
./out/build/windows-cuda-release/dic_perf_monitor.exe 192.168.1.10 17840 1800 perf-30min.csv 1000
./out/build/windows-cuda-release/dic_export_csv.exe sessions/demo-001/results.p2dic demo-001.csv
```

性能采样工具每秒记录采集/处理/丢帧、Pipeline P50/P95/P99、H2D/kernel/D2H P95、
应变时间和写盘队列水位，同时生成一个 `.json` 末次状态摘要。若 `.p2dic` 因断电截断，
导出器会保留最后一个 CRC 正确的完整帧，并以非零退出码提示文件不完整。

两点标定只建立平面尺度，不校正镜头畸变。正式测量仍需用标定板评估视场内尺度变化，并尽量
使用低畸变镜头、把 ROI 放在画面中心。点击云图任一点可直接选择该网格点的时程和 FFT。

## 6. 构建真实相机版本

在 Windows 开发机选择 `DIC: GalaxySDK + CUDA configure + build + test`。运行前必须确认：

- GalaxySDK 路径与 DLL 搜索路径正确；
- 相机未被另一程序独占；
- 配置使用 Galaxy 后端、Mono8 和合法 ROI；
- 首次测试从低帧率、短会话开始。

GalaxyView 可以用于检查相机连接、曝光和驱动，但最终 DIC 采集与计算由 Edge 执行；
同一时刻不要让 GalaxyView 和 Edge 同时独占相机。

## 7. Jetson

参见 `docs/jetson-deployment.md`。安装脚本不会覆盖已存在的 `/etc/p2dic/dic-edge.conf`。
服务日志使用：

```bash
sudo systemctl status p2dic-edge
sudo journalctl -u p2dic-edge -f
/opt/p2dic/bin/dic_ctl PING
```

## 8. 故障定位顺序

1. `PING` 不通：检查 Edge 进程、防火墙、IP 和控制端口；
2. 有状态无点场：确认会话正在运行、结果端口和 DIC 有效点比例；
3. 有点场无预览：确认 preview 开关、预览端口和 CRC 日志；
4. Galaxy 打不开：关闭 GalaxyView，检查序列号、权限、USB 和 SDK 动态库；
5. fps 低：分别记录相机取帧、DIC、写盘和显示 fps，不只看一个总数；
6. 位移漂移：先检查刚性、照明、散斑、曝光和温度，再调整算法。
