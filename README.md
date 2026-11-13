# DS5 Bridge Touchpad Gestures

## 1. 原版固件来源

本仓库基于 [SundayMoments/DS5_Bridge v2.0.1](https://github.com/SundayMoments/DS5_Bridge/releases/tag/v2.0.1)（提交 `ba839b5104ffb775a4f20a3f5426be98b387c1a8`）。原版由 **SundayMoments** 开发，源自 **awalol** 的 [DS5Dongle](https://github.com/awalol/DS5Dongle)。原作者的完整项目说明、鸣谢和参考资料保存在 [UPSTREAM_README.md](UPSTREAM_README.md)。项目按 [LICENSE](LICENSE) 和 [NOTICE](NOTICE) 中的许可分发。

## 2. 手势识别原理

手势由 Raspberry Pi Pico 2 W 固件自行识别，无需 Companion App 常驻。两指须在 **120 ms** 内先后落下，第二根手指落下后开始 **150 ms** 判定窗口。窗口结束时，两指仍在且移动量在容差内，就触发首次截图。在窗口内按下触摸板并符合录制判定时，触发录制快捷键。判定期间，控制器报告照常转发。

首次截图后，抬起一根手指，另一根就成为留指。留指可以自由滑动。它还在触摸板上时，另一根手指每次抬起后再落下，就立即触发一张续拍。留指抬起或触摸板清空后，本轮连拍结束。

## 3. 防止误触发

首次判定中，第二指落下前首指可移动 **72 X / 48 Y** 个原始坐标单位。第二指落下时重新记录双指的位置，首次截图和录制的双指判定均允许任一手指偏移 **72 X / 48 Y** 个单位。若最近 **20 ms** 内仍有合格的双指报告，录制在实体 Click 按下瞬间允许偏移至 **96 X / 64 Y**。固件按 contact ID 跟踪触点，连拍中 **12 ms** 内恢复的同一触点视为短暂丢触。持续放置两指不会重复截图。续拍指落下时若报告已带实体 Click，该次点击照常透传并跳过续拍。只有首次判定中命中录制手势的 Click 才会屏蔽到松开。连接或报告状态失效时清除手势及待发送状态。

## 4. Pico 输出的键盘组合

| 两指操作 | Bridge Keyboard HID 实际发送 |
| --- | --- |
| 基本静止保持 150 ms，不按 Touchpad Click | **Win + Alt + Print Screen** |
| 首次截图后留一指，另一指再次落下 | **Win + Alt + Print Screen** |
| 150 ms 内按下 Touchpad Click | **Alt + F2** |

两组均按修饰键、主键的顺序发送按下报告，再依次松开。电脑端如何响应取决于用户自己的快捷键设置。
