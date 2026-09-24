# USB 独占输出验收

## 实现范围

- `ao=usb-exclusive`，`usb-exclusive-fd` 接收应用通过用户 USB 授权得到的文件描述符，AO 自行复制并管理副本。
- UAC1/UAC2，Full/High Speed，单声道/立体声 Type I 整数 PCM；采样率不改变，按 DAC 格式选择容器，允许无损补零，拒绝丢弃非零低位。
- 支持同步、适应型和带显式反馈端点的异步输出。异步反馈参与分包；只有隐式反馈的设备拒绝使用。UAC2 目前要求输入终端直连 Clock Source，不猜测 Clock Selector/Multiplier 拓扑。
- DSD 的四种原始 packet 格式（MSB/LSB、交错/平面）转换成 DoP 1.1，保持原始 DSD 位；DSD64/128 对应 176.4/352.8 kHz，更高倍率取决于 DAC 接受的载波采样率。原始数据经过封装，不进行 DSD→PCM 解码。
- DoP 必须显式开启；USB 描述符不能证明 DAC 支持 DoP。没有实现厂商专有 Native DSD 切换、DST 解压和 DSD WavPack 直出。
- `ao-volume` 读取/修改 USB Feature Unit 的 master 音量，按设备 dB 范围及步长映射百分比。设备无 master 音量、只有每声道音量或无法读取范围时，返回不支持，不改软件或系统音量。不自动设置启动音量。
- 不支持浮点解码输出、8/64-bit PCM、多声道、UAC3/4、多 AudioControl 功能复合设备；这些路径明确拒绝，不能作为“所有格式均已支持”的依据。
- 未引入 DDK、libusb 或新的系统动态库依赖。仍须设备提供 USBManager、可用的原生句柄及 usbfs ioctl 权限；仅 API 存在不能证明 HarmonyOS 商用设备接受这些操作。

## 自动检查

`scripts/verify-usb-audio.sh`：边界检查、UAC1/2、采样率表、十秒分数分包、DoP MSB/LSB 字节序。该测试在补丁流程执行。

局部 ARM64 编译不等于完整构建。通过 Linux/macOS CI 重新构建 libmpv 后，才可替换应用内两个库副本。旧库会被应用的后端选项探测拦截。

## 真机必须完成的项目

1. 授权拒绝/取消、两个 DAC 手动选择、权限撤销；确认没有静默选择其他设备。
2. USB 抓包或数字回环比较 WAV/FLAC 16/24/32-bit 样本、44.1/48/88.2/96/176.4/192 kHz；24-in-32 检查填充位。位深缩减遇非零位应停止。
3. 用已知 DAC 验证 DSF/DFF 原始 DSD、DoP 标记相位、双声道同步、跨 packet 奇数字节、seek、EOF。奇数总字节无法完整组成 DoP 时报告失败，不丢弃末字节。32-valid-bit DoP 容器约定需要单独验证。
4. 30 分钟异步 DAC 时钟偏移测试，检查反馈不丢、无 URB 错误、无丢样/重复样；切换曲目频率及硬解回退均应重新协商。
5. 播放/暂停/拖动/关闭/拔插循环及传输超时，确认 URB 取消后才释放内存、恢复系统驱动、不把 DoP 数据发送到系统 PCM 输出。
6. USB 硬件音量变化时系统音量保持原值；不支持音量的设备给出明确提示。bit-perfect 的定义是送入 DAC 的数据不经数字缩放，DAC 内部硬件音量处理属于设备行为。
7. 开启既有后台播放设置，确认 AVSession 和 AUDIO_PLAYBACK 连续任务生效，息屏至少 30 分钟。保持系统音量不变并不保证 USB 传输被系统认定为活跃音频，必须记录具体设备和系统版本的结果。
8. phone/tablet/2in1/tv 分别核对 USB 主机能力与授权/usbfs 支持；模拟器不能代替真机 DAC 验证。

协议依据：[DoP 1.1](https://dsd-guide.com/sites/default/files/white-papers/DoP_openStandard_1v1.pdf)、[USB Audio 1.0](https://www.usb.org/sites/default/files/audio10.pdf)、[USB Audio 2.0 文档入口](https://www.usb.org/documents?search=Audio+2.0)。
