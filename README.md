# DS5 Bridge Touchpad Gestures 2.0.1

本分支将触摸板手势移植到了 [SundayMoments 的 DS5 Bridge 2.0.1](https://github.com/SundayMoments/DS5_Bridge/releases/tag/v2.0.1)。原项目源自 awalol 的 DS5Dongle，完整说明和鸣谢见 [UPSTREAM_README.md](UPSTREAM_README.md)。

- **留一指连拍。** 双指触发首次截图后，抬起一根手指，留下的手指可以自由滑动。另一根手指每次重新落下就触发一张截图，无需再等 150 ms。留指离开后结束本轮连拍。
- **截图和录屏更容易触发。** 双指落下后的 150 ms 判定窗口内，每根手指允许的偏移从横向 48、纵向 32 放宽到横向 72、纵向 48 个原始坐标单位。录屏在实体按下瞬间仍有额外容差，用于容纳按压时的位移。
