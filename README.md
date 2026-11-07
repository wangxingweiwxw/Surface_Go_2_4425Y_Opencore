![OpenCore](https://github.com/acidanthera/OpenCorePkg/raw/master/Docs/Logos/OpenCore_with_text_Small.png)

# Surface Go 2 — Pentium Gold 4425Y — OpenCore

面向 **Microsoft Surface Go 2 / Pentium Gold 4425Y / 8GB 内存 / 128GB SSD** 的 macOS Monterey EFI。该配置已在本机用于 macOS Monterey 12.7.6 与 Windows 10 双系统，包含显卡加速、触摸屏、电池显示和模式 25 休眠支持。

An EFI configuration for the **Microsoft Surface Go 2 with Pentium Gold 4425Y, 8GB RAM and a 128GB SSD**. This configuration has been used on the tested device with macOS Monterey 12.7.6 and Windows 10, including graphics acceleration, touchscreen input, battery reporting and mode 25 hibernation.

本仓库保存 Monterey 稳定配置，**不适用于直接引导 Ventura 或更高版本**。Core m3、4GB/eMMC 等机型未经本配置验证。

This repository preserves the stable Monterey configuration. **It is not a drop-in EFI for Ventura or later.** Core m3 and 4GB/eMMC variants have not been validated with this configuration.

## 硬件与系统 / Hardware and software

| 项目 / Item | 配置 / Configuration |
| --- | --- |
| 设备 / Device | Microsoft Surface Go 2 |
| 处理器 / CPU | Intel Pentium Gold 4425Y, 1.70GHz |
| 内存 / Memory | 8GB LPDDR3 |
| 内置存储 / Internal storage | 128GB SSD |
| 核显 / iGPU | Intel UHD Graphics 615 |
| 已验证系统 / Tested OS | macOS Monterey 12.7.6, Darwin 21.6.0 |
| 双启动 / Dual boot | Windows 10 + macOS |
| 引导器 / Bootloader | OpenCore 1.0.5 DEBUG |
| SMBIOS | MacBookPro14,1 |

## 使用前必读 / Important

**Please add your own serial number and UUID by using Gen SMBIOS in this guide.**

**请按下方指南使用 GenSMBIOS 生成并填写自己的序列号和 UUID。**

- [Dortania — Creating your config.plist](https://dortania.github.io/OpenCore-Install-Guide/config.plist/#creating-your-config-plist)
- [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS)

发布版已清空 `EFI/OC/config.plist → PlatformInfo → Generic` 的 `SystemSerialNumber`、`MLB`、`SystemUUID`，并将 `ROM` 置零。使用前填写自己的身份信息：通过 GenSMBIOS 生成与 `MacBookPro14,1` 对应的序列号、MLB 和 UUID，按 Dortania 指南设置本机 ROM。请勿直接使用全空或全零身份登录 Apple 服务。

The public configuration has empty `SystemSerialNumber`, `MLB` and `SystemUUID` fields and a zeroed `ROM` under `EFI/OC/config.plist → PlatformInfo → Generic`. Before use, generate your own serial number, MLB and UUID for `MacBookPro14,1` using GenSMBIOS, and configure your machine's ROM following Dortania. Do not sign in to Apple services with blank or zeroed identity fields.

协议 GUID、驱动二进制 UUID 等通用标识不属于 SMBIOS 机器身份，保留原值以保证配置和驱动完整性。

Protocol GUIDs and driver binary UUIDs are not the machine's SMBIOS identity and are retained for configuration and binary integrity.

## 已验证功能 / Confirmed on the tested device

| 功能 / Feature | 状态及实现方法 / Status and approach |
| --- | --- |
| Windows / macOS 双启动 / Dual boot | OpenCore 菜单选择两套系统；保留磁盘现有 Windows 引导文件。 / OpenCore offers OS selection; retain the disk's existing Windows boot files. |
| 显卡与 Metal / Graphics and Metal | Monterey 下已长期稳定使用；WhateverGreen 配合 SurfaceGo2SHRX 的部分 BMI2 指令兼容支持。 / Long-term stable use reported on Monterey; WhateverGreen with partial BMI2 compatibility supplied by SurfaceGo2SHRX. |
| 触摸屏 / Touchscreen | 基础触摸正常，使用 VoodooI2C / VoodooI2CHID。 / Basic touch works through VoodooI2C / VoodooI2CHID. |
| Type Cover | 键盘、基础触控板输入及合盖触发已验证；完整手势与热插拔仍需测试。 / Keyboard, basic trackpad input and lid-triggered hibernation tested; full gestures and hot-plug need further testing. |
| 电池显示 / Battery reporting | 电量与充电状态正常，使用 VirtualSMC、SMCBatteryManager 和 ECEnabler。 / Battery level and charging status work through VirtualSMC, SMCBatteryManager and ECEnabler. |
| Wi-Fi / 网络 / Networking | 无线联网可用，使用 AirportItlwm。 / Wi-Fi connectivity works through AirportItlwm. |
| App Store | 系统日期、时间正确时连接正常；其他 Apple 服务需独立验证。 / Connectivity works with the correct system date and time; other Apple services require separate testing. |
| 菜单及合盖休眠 / Menu and lid hibernation | 模式 25 休眠及电源键恢复已验证，使用 HibernationFixup 和 Surface 唤醒 ACPI 配置。 / Mode 25 hibernation and power-button resume tested, using HibernationFixup and Surface wake ACPI configuration. |

## 限制与待测项目 / Limitations and pending checks

- **普通 S3 睡眠、开盖自动唤醒：未实现或未确认。** 当前采用写入磁盘的模式 25 休眠，按电源键恢复；短时测试通过不代表过夜续航及所有外设恢复均已验证。
  **Conventional S3 sleep and automatic wake on lid opening are not established.** The current setup uses mode 25 disk hibernation and power-button resume. Short tests do not establish overnight battery life or recovery of every peripheral.
- **AirDrop：当前 Intel 无线方案不提供可用的原生 AirDrop；接力、随航、无线屏幕镜像未验证。**
  **AirDrop: native AirDrop is not available with the current Intel wireless setup; Handoff, Sidecar and wireless screen mirroring are unverified.**
- **摄像头：尚未适配。** Surface Pen 压感、侧键、防误触，多指触摸，自动旋转和旋转后的触摸坐标仍待验证。
  **Cameras are not supported by this configuration.** Surface Pen pressure, buttons and palm rejection, multitouch, automatic rotation and rotated touch coordinates remain unverified.
- 蓝牙稳定性、音频输入输出完整功能、亮度快捷键、USB-C 外接显示、microSD、DRM、iMessage / FaceTime 及休眠后的外设恢复需要进一步检查；包含相关驱动不等于已完成所有功能测试。
  Bluetooth reliability, complete audio I/O, brightness hotkeys, USB-C displays, microSD, DRM, iMessage / FaceTime and peripheral recovery after hibernation need further checks. Bundled drivers do not establish full functionality.
- 曾观察到系统拼音输入法连续拼音候选词异常，尚无明确修复结论。
  Multi-syllable candidate issues have been observed with the system Pinyin input method; a fix has not been confirmed.
- 本机为 SSD 型号；不能据此认定 eMMC 型号内置存储可用。
  The tested device uses an SSD; this does not establish internal-storage support for eMMC variants.

## 安装方法 / Installation

1. 备份现有系统和 EFI，准备可恢复的启动介质。参考 [Dortania 安装指南](https://dortania.github.io/OpenCore-Install-Guide/)制作 Monterey 安装介质。
   Back up your systems and EFI and keep working recovery media. Prepare Monterey installation media using the [Dortania guide](https://dortania.github.io/OpenCore-Install-Guide/).
2. 填写上述 SMBIOS 身份信息，保持其余稳定配置，先在 U 盘上测试。
   Fill in your SMBIOS identity, retain the remaining stable configuration and test from a USB drive first.
3. 将 `EFI/BOOT` 和 `EFI/OC` 放入目标磁盘 **FAT32 EFI 系统分区（ESP）**，确保路径为 `EFI/OC/config.plist`。不要写入 MSR 分区，不要嵌套成 `EFI/EFI`。
   Place `EFI/BOOT` and `EFI/OC` on the target drive's **FAT32 EFI System Partition (ESP)** so the configuration is at `EFI/OC/config.plist`. Do not use the MSR partition or create an `EFI/EFI` nesting.
4. 双系统请保留原有 `EFI/Microsoft` 和 BCD。本仓库不分发 Windows 引导文件或机器专属 BCD。
   For dual boot, preserve your existing `EFI/Microsoft` and BCD. Windows boot files and machine-specific BCD are not distributed here.
5. 使用能引导 OpenCore 的 Surface UEFI 设置，分别验证 macOS 和 Windows。确认正常后再考虑部署到内置 ESP；失败时恢复备份。
   Use Surface UEFI settings that permit OpenCore to boot, and verify both macOS and Windows. Deploy to the internal ESP only after successful testing; restore your backup if needed.

### 休眠设置 / Hibernation settings

EFI 与 macOS 电源设置共同实现休眠；复制 EFI 不会自动应用电源参数。以下为本机验证过的设置，仅面向本仓库的 Monterey 配置：

Hibernation requires both EFI support and macOS power settings. Copying the EFI does not apply power settings automatically. These settings were tested on this device with the Monterey configuration:

```bash
sudo pmset -a hibernatemode 25 sleep 0 powernap 0 proximitywake 0 disablesleep 0
sudo pmset -c womp 0
pmset -g custom
```

`sleep 0` 关闭闲置自动睡眠，不阻止菜单或合盖休眠。恢复时短按电源键，在 OpenCore 选择原 macOS；若有解锁提示则解锁。先完成 macOS 恢复，再切换 Windows。不要把此流程视为开盖即唤醒的 S3 睡眠。

`sleep 0` disables idle automatic sleep, not menu- or lid-triggered hibernation. Resume with a short power-button press, select the original macOS in OpenCore and unlock if prompted. Complete the macOS resume before switching to Windows. This is not conventional S3 sleep with automatic lid-open wake.

## 组件与来源 / Components and credits

- [chaeimseu/Surface_Go_2_Opencore](https://github.com/chaeimseu/Surface_Go_2_Opencore) — Pentium 机型参考与 README 组织参考。 / Pentium reference configuration and README organization.
- [atisDu/Surface_go_opencore](https://github.com/atisDu/Surface_go_opencore) — Surface Go 社区适配参考。 / Community Surface Go reference.
- [jlempen/Surface-Go-2-OpenCore](https://github.com/jlempen/Surface-Go-2-OpenCore) — m3 型号适配参考，不代表本仓库支持 m3。 / Core m3 reference; not a claim of m3 support here.
- [Acidanthera](https://github.com/acidanthera) — OpenCore, Lilu, WhateverGreen, VirtualSMC, AppleALC, CPUFriend, HibernationFixup and related components.
- [OpenIntelWireless](https://github.com/OpenIntelWireless) — AirportItlwm and IntelBluetoothFirmware.
- [VoodooI2C](https://github.com/VoodooI2C/VoodooI2C) — I2C input support.
- [ECEnabler](https://github.com/1Revenger1/ECEnabler), [RealtekCardReader](https://github.com/0xFireWolf/RealtekCardReader), [RealtekCardReaderFriend](https://github.com/0xFireWolf/RealtekCardReaderFriend), [USBInjectAll](https://github.com/RehabMan/OS-X-USB-Inject-All).

`SurfaceGo2SHRX` 0.5.0 用于本机 Monterey 的部分 BMI2 指令兼容，限定 Darwin 21.6 并需 `-go2shrx`。它不是通用 AVX/AVX2 模拟器。对应源码位于 [Sources/SurfaceGo2SHRX](Sources/SurfaceGo2SHRX)。第三方组件保留各自许可证；仓库许可证不改变其许可条款。

`SurfaceGo2SHRX` 0.5.0 provides partial BMI2 compatibility for this Monterey setup, restricted to Darwin 21.6 with `-go2shrx`. It is not a general AVX/AVX2 emulator. Its source is in [Sources/SurfaceGo2SHRX](Sources/SurfaceGo2SHRX). Third-party components remain under their respective licenses; the repository license does not override them.

## 使用范围 / Scope

这是特定硬件上验证的配置分享，并非所有 Surface Go 2 的通用安装方案。发布副本仅清除了机器身份字段，配置通过 OpenCore 校验；更换 SMBIOS 身份后仍需在自己的机器上测试。不要把本仓库当作 macOS 13 或更高版本的兼容承诺。

This is a configuration tested on specific hardware, not a universal installer for every Surface Go 2. The public copy only clears machine-identity fields and passes OpenCore validation; test it on your own machine after personalizing SMBIOS. This repository does not promise compatibility with macOS 13 or later.
