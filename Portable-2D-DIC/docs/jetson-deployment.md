# Jetson 部署与恢复

## 前提

- Jetson Orin 系列设备已经安装匹配的 JetPack/CUDA；
- 已安装大恒 GalaxySDK ARM64 驱动与开发包；
- 相机用户态权限已经按 GalaxySDK 安装说明配置；
- NVMe 已挂载并预留足够的会话存储空间。

## 构建

```bash
export GALAXY_SDK_ROOT=/path/to/GalaxySDK/Development
./tools/build-jetson.sh
```

## 安装

```bash
sudo ./deploy/jetson/install.sh
```

安装器创建独立的 `p2dic` 系统用户，将程序安装到 `/opt/p2dic/bin`，配置放在
`/etc/p2dic/dic-edge.conf`，实验数据写入 `/var/lib/p2dic/sessions`。已有配置不会被覆盖。

## 运维命令

```bash
systemctl status p2dic-edge.service
journalctl -u p2dic-edge.service -f
/opt/p2dic/bin/dic_ctl STATUS 127.0.0.1 17840
sudo systemctl restart p2dic-edge.service
```

服务崩溃后由 systemd 自动重启。每 30 秒执行一次 `PING` 健康检查；进程存在但控制端口
无响应时，恢复服务会重启 Edge。SIGTERM 停止流程会让当前会话以 `interrupted` 状态关闭。

## 尚需实机验证

- GalaxySDK ARM64 库、USB 权限和相机枚举；
- CUDA 架构 87 与目标 JetPack 工具链兼容性；
- 断电恢复、NVMe 写满、过热降频和 8 小时连续运行；
- 10/15/21.4 fps 下的丢帧率、延迟和数据完整性。
