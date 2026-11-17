# 设置页显示对齐（2026-11-17）

用户反馈设置项显示与 Qt 差异较大。本轮依据 `D:/share/CarHml_XIAO/qml/Settings/` 和 `qml/ComControl/` 的源文件修正根分类和共用列表显示。

## 已修正

- 根分类从原有透视轮盘恢复为固定横排。按 `PageMainSettings.defaultLocalOffSet()`，图标父容器 y=80；五个 `ComSettingMenu` 的位置为 x=70/170/270/370/470、局部 y=10、尺寸52×52。沿用原始普通/选中图片，选中静止缩放1.15，底部46×4红条使用 Qt 的 `#DC143C`。
- 去除轮盘的背景、深度压暗、层级重排和转动动画，以及设置覆盖层额外添加的半透明黑底。
- 根分类预览、我的座驾、系统设置和互联设置保留图标行；进入压栈子页才隐藏图标。系统设置不再另加一行大标题。
- 分类列表从 y=160 开始，压栈子页从 y=140 开始。通用列表视口高300，中心卡片列表高285，按实际视口高度滚动选中项。日期滚轮使用独立较高视口。
- 系统列表恢复 `ItemDetails` 的左名称、右摘要：宽450，名称26px，右摘要20px。修复原日期摘要覆盖“日期时间”名称的错误，语言、系统版本、开机动画也显示各自摘要。版本读取失败仍显示实际失败状态。
- 长名称限制为单行：未选中时省略，选中且可见时往返滚动；隐藏列表不启动文字滚动。翻译缓存读取前恢复 LVGL DOTS 临时改写的文本，防止截断内容污染中英文切换。
- 中心卡片使用实际折线勾号，替代原来的 X，边框与勾号随焦点一起变色。

本轮沿用现有按键路由、设置值、命令和保存方式，没有补齐尚未迁移的菜单业务。用户之前要求的 `ODO`、`TripA`、`TripB`、常规字重档位保持生效。

## 验证

Windows 临时 MinGW 无窗口工程构建成功，CTest 7/7通过（display / i18n / overlays / turn_signal / dial / steering_key / fonts）。字体渲染专项992项检查、0失败，包含新增实际仪表背景上的设置覆盖层截图。

方向盘回归补充验证五个分类切换时位置固定、选中红条唯一、系统页保留图标行、日期名称与摘要独立、压栈子页内容起点140，以及中英文下各列表项目完整可见。现有按键、设置与命令回归通过。

验证命令：

```powershell
cmake --build .cache/font-check/build -j 8
ctest --test-dir .cache/font-check/build --output-on-failure
./.cache/font-check/build/check_fonts.exe .cache/font-check/settings-port.ppm
```

临时 Windows 兼容适配的范围与限制见 [FONT_PORT.md](FONT_PORT.md)。未执行本轮 Linux/板端构建和实机显示验收。

## 预览和剩余差异

- [系统设置修改前后](preview/settings/system-comparison.png)：两侧都是 LVGL 渲染，不是 Qt 实机截图。
- [实际仪表上的中文系统设置](preview/settings/system-zh.png)
- [实际仪表上的英文系统设置](preview/settings/system-en.png)
- [我的座驾](preview/settings/mycar-zh.png)
- [中心卡片勾选](preview/settings/cards-zh.png)

这次修正了上述布局和显示问题，没有完成设置系统的全部 Qt 视觉移植。分类图标只对齐静止选中态，Qt 弹性缩放和子页入场动画未复现；省略使用 LVGL 的尾部省略，与 Qt 部分字段的中间省略不同；开关仍只显示当前 ON/OFF，未还原 Qt 同时显示的双选项；亮度条、互联蓝牙卡片和个别子页控件仍有差异。单位页仍只支持已迁移的里程/温度两项，不能用显示调整宣称油耗/压力单位业务已完成。

Qt 未显式指定的拉丁默认字体仍保留 Montserrat，尚未取得 Qt 同输入运行截图进行像素级验收。
