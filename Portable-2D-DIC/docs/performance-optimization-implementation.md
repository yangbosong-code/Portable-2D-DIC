# 性能优化实施记录（2026-08-09）

## 已落地

- 采集、DIC、应变、回调、Pipeline、端到端最近 1024 帧 P50/P95/P99；
- CUDA H2D、kernel、D2H 使用 CUDA event 独立计时；
- 计算回调与磁盘 I/O 解耦，有界队列满时显式进入故障状态；
- 版本化 `.p2dic` 逐帧记录、CRC32、截断扫描恢复及离线 CSV 导出；
- CUDA 构建下的页锁定 `FramePool`，相机直接写入 pinned frame；分配失败安全回退普通内存；
- 非阻塞 CUDA stream、`cudaMemcpyAsync`、pinned D2H 结果缓冲；
- 6 路累加量融合 warp 归约，减少 shared-memory barrier；
- IC-GN 参考 Hessian 预计算实验后端，可由 `dic.inverse_compositional=true` 开启；
- Studio 性能面板、QImage 云图一次绘制、基 2 FFT；
- `dic_perf_monitor` 长时间 CSV/JSON 性能采样工具。

## RTX 5060 Laptop 全幅基准

输入为 4504×4096 Mono8 合成散斑，subset radius 15，连续 100 帧，每帧平移 1 px。

| solver | grid_step | 点数 | 平均 | P50 | P95 | 估算计算吞吐 |
|---|---:|---:|---:|---:|---:|---:|
| 优化前 FA-GN | 64 | 4,480 | 3.110 ms | 3.005 ms | 4.080 ms | 322 fps |
| 优化后 FA-GN | 64 | 4,480 | 1.522 ms | 1.479 ms | 1.784 ms | 657 fps |
| 实验 IC-GN | 64 | 4,480 | 1.676 ms | 1.652 ms | 1.924 ms | 597 fps |
| 优化前 FA-GN | 32 | 17,780 | 5.343 ms | 4.902 ms | 9.083 ms | 187 fps |
| 优化后 FA-GN | 32 | 17,780 | 2.508 ms | 2.466 ms | 2.792 ms | 399 fps |
| 实验 IC-GN | 32 | 17,780 | 2.951 ms | 2.905 ms | 3.327 ms | 339 fps |

优化后相对旧基线：step 64 平均耗时下降约 51.1%，step 32 下降约 53.1%。最终帧
平均位移误差分别约 0.014 px 和 0.021 px；有效率分别为 98.6% 和 97.9%。

step 32 的平均 CUDA 分段时间为：H2D 0.848 ms、kernel 1.003 ms、D2H 0.086 ms。
IC-GN 在当前实现和 GPU 上仍比融合 FA-GN 慢，因此保留实验开关但不设为默认。

## 回归结果

- Qt/CPU：25/25；
- CUDA：21/21；
- GalaxySDK：19/19；
- GalaxySDK + CUDA：21/21；
- 合计：86/86。

## 仍需真实硬件验收

当前数据不包含 ME2P-1840-21U3M 的真实 USB3 采集，也不能替代 Jetson Orin Nano Super。
拿到硬件后必须依次执行 10/15/21.4 fps、30 分钟/8 小时、温度/功耗/掉帧/断电恢复测试。
只有实测显示 H2D 与 kernel 在 Jetson 上构成连续瓶颈时，才继续投入双 stream 跨帧重叠、
mapped zero-copy 或纹理对象；不要在没有 Nsight 时间线证据时增加并发复杂度。
