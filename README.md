# DS5 Bridge Touchpad Gestures

## 1. 原版固件来源

本仓库基于 [SundayMoments/DS5_Bridge](https://github.com/SundayMoments/DS5_Bridge) 的 `main` 分支、v1.7.1（提交 `5ee08e0984085c99eac5afe14c05f572b6e9dc59`）。原版由 **SundayMoments** 开发，源自 **awalol** 的 [DS5Dongle](https://github.com/awalol/DS5Dongle)。原作者的完整项目说明、鸣谢和参考资料保存在 [UPSTREAM_README.md](UPSTREAM_README.md)；原有 [NOTICE](NOTICE) 与 [LICENSE](LICENSE) 保持不变（AGPL-3.0-only）。

## 2. 手势识别原理

手势由 Raspberry Pi Pico 2 W 固件自行识别，无需 Companion App 常驻。固件按各自的 contact ID 跟踪两根手指：两指须在 **120 ms** 内先后落下；第二根手指建立后启动 **150 ms** 判定窗口。窗口结束时两指仍在、基本静止且没有 Touchpad Click，则发送一次第一组快捷键；窗口内出现 Touchpad Click 按下边沿，则立即取消待定的第一组快捷键，改发一次第二组快捷键。控制器报告在判定期间照常实时转发，不等待 150 ms。

## 3. 防止误触发

单指触摸、单独按下 Touchpad Click、超过 120 ms 才建立的双指动作均不触发。第二根手指落下前允许首指在落点附近稳定（X/Y 最多 **72/48** 原始坐标单位）；此时重新记录双指起始位置，判定期间任一手指偏移超过 **48/32** 单位即取消。两指的 contact ID 分别追踪，不依赖触点 slot 次序；极短暂的触点丢失只在同一 ID 及时恢复时继续判定。触发或取消后，须等触点全部离开才可识别下一次，因此持续放指或按住 Click 不会重复发送。仅命中第二组手势时，屏蔽这次实体 Click 直至松开；其他触摸、按键和控制器输入继续透传。连接或报告状态失效时清除手势及待发送状态。

## 4. Pico 输出的键盘组合

| 两指操作 | Bridge Keyboard HID 实际发送 |
| --- | --- |
| 基本静止保持 150 ms，不按 Touchpad Click | **Win + Alt + Print Screen** |
| 150 ms 内按下 Touchpad Click | **Alt + F2** |

两组均发送完整的键盘按下及松开报告；电脑端如何响应取决于用户自己的快捷键设置。
