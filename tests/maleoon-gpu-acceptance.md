# 默认 GPU 优化与验收

## 已实现的默认行为

- SweetVideo 使用应用私有 cacheDir/mpv-gpu-v1，沿用 gpu-next 的缓存管理、容量控制和后端设备/驱动签名。不创建公共目录、不引入新系统库。目录创建失败不影响播放。
- Shader 路径先整体校验，再以 MPV_FORMAT_NODE_ARRAY 一次替换。按实际属性去重，Reset 后不会使用旧句柄缓存；应用只准备当前选择的链，复用当前进程已准备的文件。拷贝失败不提交残缺链。
- 弹幕每帧一次顶点上传，最多四张 1024x1024 RGBA atlas，保留 32MiB 原有纹理预算。槽位不搬迁，带一像素边缘扩展；只合并相邻同纹理绘制，保持透明混合顺序。精度不足、超大字形、atlas 满或创建失败时回到独立纹理。原有 ArkTS 失败回退保持不变。
- GLES/Vulkan 实际 GPU 名称包含 Maleoon 时，带显式 GLSL 注释标记的 ArtCNN C4F16/C4F32 系列使用 8x8 工作组。保持每线程输出数、网络权重、采样位置和 barrier。非马良、未知自定义 shader、能力不足保持原始 12x16。原文件已有 float16 扩展判断和 FP32 回退，未强制降低精度。
- 不增加用户设置，也不自动打开超分本身；用户原来选择的 Anime4K/ArtCNN 模式不变。

## 已审计并保留的路径

- Anime4K 已使用向量矩阵计算；缓存、原子提交和复用同样适用。未在无图像误差验收时强制把 CNN 累加器、采样坐标或 HDR 运算改成 mediump。
- GLES 非 load_target 已执行 framebuffer invalidate，弹幕在 pass 开始清透明；Vulkan 中间 target 使用 DONT_CARE loadOp，仍需要 STORE 给后续 pass。没有盲目丢弃要读取的附件。
- Vulkan swapchain 默认 depth=3 请求四张图，符合重负载建议，不全局强制双缓冲。当前前端没有消费旋转 preTransform 的补偿链路，未直接改成旋转 currentTransform，避免横竖屏画面旋转错误。
- 保留 NativeImage / NativeBuffer 零拷贝、缓存及 acquire/release fence。没有删 pl_gpu_flush 或资源退休等待，也没有额外每帧 glFinish。
- ArtCNN storage/imageStore 中间纹理不能仅靠改 usage 标记获得 HEBC。没有给 storage 图强行宣称启用 HEBC；要改变算法路径须另做设备与图像验证。

## 回归层次

1. 顺序应用全部 libplacebo 补丁后运行 scripts/verify-maleoon-gpu.sh。它验证 GPU 识别、能力不足回退、倍率和奇数尺寸像素覆盖，不验证真实 shader 编译或 GPU 输出。
2. 原生 SDK 编译 + SweetVideo debug HAP 构建。不能替代底层 Linux/macOS 构建。检查最终包内 ARM64 .so 的架构、NEEDED 和未解析接口，尤其 Audio Suite 仍应可选。
3. 真机至少覆盖马良手机/平板及非马良或未知 GPU，TV/2in1 如可获得单独报告。验证 Surface 的视频/字幕/OSD、GPU buffer 的硬解/软解、HDR/P010、横竖屏、seek、Reset、页面退出与反复创建。
4. 画质：无 shader / Anime4K / ArtCNN C4F16（含 DN/DS）/ C4F32，同一静帧、奇数尺寸、边缘、高对比字幕做逐像素比较。单测覆盖不能证明 GPU 驱动上的浮点一致性。
5. 弹幕：0/30/120 条、重复与不同文本、半透明重叠、atlas 满、换字体/大小/屏幕缩放。确认无邻字渗色、无混合顺序改变、资源仍按 owner thread 清理。

## 性能验收（不添加常态逐帧日志）

固定片源、解码方式、shader、分辨率、屏幕刷新率、温度和供电状态，关闭网络波动因素。分别测首次使用当前链（冷缓存）和正常退出后重启（热缓存）；不能把普通应用启动时间当 shader 编译耗时。

每组原版/优化版各三次，预热 30 秒、测量 60 秒。用现有 mpv vo-passes / frame-drop-count / decoder-frame-drop-count 以及官方 GPU profiler 记录 CPU/GPU 帧耗时中位数、P95、掉帧数、draw call/texture bind、带宽和功耗。不要依靠“流畅了”的主观感觉，也不要打开同步 GPU timer/readback 后用其耗时代表正常播放。

8x8 是默认的有限工作组优化，不代表已证明最优。后续在独立测试构建比较 8x4（32）、8x8（64）、16x8（128）和原 12x16（192），同步修改 output block 与 shared-memory 布局关系，保留网络与 barrier。先满足画质一致，再选择实测 P95 和功耗较好的策略。

参考：[华为 Maleoon GPU 最佳实践](https://developer.huawei.com/consumer/cn/doc/doccenter-feature-dev/bpta-maleoon-gpu-best-practices)。实际设备性能和兼容性仍需设备验收。
