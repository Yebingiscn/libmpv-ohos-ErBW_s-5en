# USB 独占输出验收

## 实现范围

- `ao=usb-exclusive`，`usb-exclusive-fd` 接收应用通过用户 USB 授权得到的文件描述符，AO 自行复制并管理副本。
- UAC1/UAC2，Full/High Speed，单声道/立体声 Type I 整数 PCM；采样率不改变，按 DAC 格式选择容器，允许无损补零，拒绝丢弃非零低位。
- 支持同步、适应型和带显式反馈端点的异步输出。异步反馈参与分包；只有隐式反馈的设备拒绝使用。UAC2 目前要求输入终端直连 Clock Source，不猜测 Clock Selector/Multiplier 拓扑。
- DSD 的四种原始 packet 格式（MSB/LSB、交错/平面）转换成 DoP 1.1，保持原始 DSD 位；DSD64/128 对应 176.4/352.8 kHz，更高倍率取决于 DAC 接受的载波采样率。原始数据经过封装，不进行 DSD→PCM 解码。
- DoP 必须显式开启；USB 描述符不能证明 DAC 支持 DoP。`usb-exclusive-dsd-mode=no/dop/native` 选择输出模式；Native 同时使用 `audio-spdif=native-dsd`，DoP 使用 `audio-spdif=dop`。
- Native DSD 使用独立 opaque 格式，按 VID/PID、备用接口、UAC2 32-bit 描述符及已知 Amanero 固件版本匹配 U32 BE/LE 传输，不按 DAC 芯片名推测能力。未知设备或固件停止；不支持 U8/U16、ITF 厂商切换和全部 DSD_RAW 设备。完整首批 ID 列表见补丁中的 `audio/out/usb_audio_dsd.h`，这些接口也从 PCM/DoP 候选中排除。
- DST、DSD WavPack 通过 FFmpeg `dsd_raw` 私有选项输出标记过的 MSB-first DSD 字节，再封装 DoP 或 Native；不经过浮点 PCM。WavPack copy/fast/high 三种 DSD 解包路径均保留，坏 CRC 拒绝。普通 PCM WavPack 保持 PCM；正常非独占播放默认仍使用原有解码方式。
- `ao-volume` 读取/修改 USB Feature Unit 的 master 音量，按设备 dB 范围及步长映射百分比。设备无 master 音量、只有每声道音量或无法读取范围时，返回不支持，不改软件或系统音量。不自动设置启动音量。
- 不支持浮点解码输出、8/64-bit PCM、多声道、UAC3/4、多 AudioControl 功能复合设备；这些路径明确拒绝，不能作为“所有格式均已支持”的依据。
- 未引入 DDK、libusb 或新的系统动态库依赖。仍须设备提供 USBManager、可用的原生句柄及 usbfs ioctl 权限；仅 API 存在不能证明 HarmonyOS 商用设备接受这些操作。

## 自动检查

`scripts/verify-usb-audio.sh`：边界检查、UAC1/2、采样率表、十秒分数分包、DoP MSB/LSB 字节序。该测试在补丁流程执行。

新增 `tests/dsd-output.c` 覆盖 Native/DoP、单/双声道、平面/交错、MSB/LSB、所有分包位置、重置、设备规则及固件拒绝。`scripts/verify-dsd-decoders.sh` 在独立目录编译 host FFmpeg 的两个解码器，验证 DST 未压缩帧及截断错误、WavPack copy/fast/high 还原字节和 CRC 错误、普通 PCM WavPack 回归；测试数据是自生成的位模式，见 `tests/fixtures/dsd/README.md`。

2026-09-25 本地验证：以上 C 测试通过；另以 FFmpeg FATE 的 `dst/dst-64fs44-2ch.dff` 中 10 个压缩 DST 帧验证 raw DSD 经独立 DSD-to-PCM 后与默认解码结果逐字节一致。MPV/FFmpeg 改动文件 ARM64 SDK 编译通过，应用调试 HAP 构建通过。尚未完成新 libmpv 的完整目标构建，也没有连接 DAC 验证；现有 HAP 内仍是此前的底层库，不含这些新增功能。

局部 ARM64 编译不等于完整构建。通过 Linux/macOS CI 重新构建 libmpv 后，才可替换应用内两个库副本。旧库会被应用的后端选项探测拦截。

## 真机必须完成的项目

1. 授权拒绝/取消、两个 DAC 手动选择、权限撤销；确认没有静默选择其他设备。
2. USB 抓包或数字回环比较 WAV/FLAC 16/24/32-bit 样本、44.1/48/88.2/96/176.4/192 kHz；24-in-32 检查填充位。位深缩减遇非零位应停止。
3. 用已知 DAC 验证 DSF/DFF 原始 DSD、DST DFF、DSD WavPack、PCM WavPack、DoP 标记相位、双声道同步、跨 packet 奇数字节、seek、EOF。DoP 每声道 2 字节 / Native 每声道 4 字节无法形成完整末帧时报错，不填充或丢弃源数据。32-valid-bit DoP 容器约定需要单独验证。Native BE/LE、已知固件、未知固件拒绝和 PCM/DSD 往返切换分别测试。
4. 30 分钟异步 DAC 时钟偏移测试，检查反馈不丢、无 URB 错误、无丢样/重复样；切换曲目频率及硬解回退均应重新协商。
5. 播放/暂停/拖动/关闭/拔插循环及传输超时，确认 URB 取消后才释放内存、恢复系统驱动、不把 DoP 数据发送到系统 PCM 输出。
6. USB 硬件音量变化时系统音量保持原值；不支持音量的设备给出明确提示。bit-perfect 的定义是送入 DAC 的数据不经数字缩放，DAC 内部硬件音量处理属于设备行为。
7. 开启既有后台播放设置，确认 AVSession 和 AUDIO_PLAYBACK 连续任务生效，息屏至少 30 分钟。保持系统音量不变并不保证 USB 传输被系统认定为活跃音频，必须记录具体设备和系统版本的结果。
8. phone/tablet/2in1/tv 分别核对 USB 主机能力与授权/usbfs 支持；模拟器不能代替真机 DAC 验证。

协议依据：[DoP 1.1](https://dsd-guide.com/sites/default/files/white-papers/DoP_openStandard_1v1.pdf)、[USB Audio 1.0](https://www.usb.org/sites/default/files/audio10.pdf)、[USB Audio 2.0 文档入口](https://www.usb.org/documents?search=Audio+2.0)。
