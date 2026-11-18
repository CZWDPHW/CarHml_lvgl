# Qt 字体与字号移植记录

2026-11-17，本次按用户要求恢复 `D:/share/CarHml_XIAO` 中明确指定的字体和字号，覆盖当前 Forester Style1、中心卡片及设置页。车速刻度表、单位换算、指针角度与刻度高亮命中区间沿用修改前规则；车速高亮外观改为 Konexy 24px 居中放大约 1.2 倍。

## 字体映射

| 部位 | 当前字体与像素字号 |
| --- | --- |
| 档位 | Forgotten Futurist 62，常规字重（按用户后续要求取消粗体） |
| 车速刻度 | Konexy 24 |
| 转速刻度 | Konexy 44 / 48；零刻度在不超过 200rpm 时为 48，否则 44 |
| 时钟数字 / 冒号 | Bebas 36 合成粗体 / Geely 36 |
| 车外温度 | Bebas 28 |
| 总里程 / 小计 / 续航数字 | 通用字体 28 / 24 / 24，粗体 |
| 续航前缀 / 小计前缀 | Arial 24 / Arial 24 粗体 |
| 巡航 | Geely 24 / 14 |
| 行程与短期数值 | Geely 27 |
| 胎压数值 / 单位 | Geely 24 / 18 |
| 瞬时油耗 | Geely 34 |
| 音乐 | FM 64、频率 40 粗、姓名 42 粗、号码 32、歌名 28 粗、歌词 32 粗 |
| 互联 | 模式 Geely 32、提示与蓝牙 Geely 24、连接中 Geely 20、预览名称 16 |
| 设置列表 | 通用字体 26 |

QML 未明确指定字体族的拉丁文字暂保留 Montserrat，字号与粗细按对应字段设置。未捕获原 Qt 运行时默认字体解析结果，不能将这些字段视为字体族已完全对齐。中文来自原项目 `NotoSansCJK-Regular.ttc` 的简体中文字面，合并到对应字号的通用、Geely 和 Arial 字库中。

2026-11-18 设置页例外：按英文偏大与左右叠字的反馈，英文菜单改用Arial Regular 22px、标题28px、辅助文字20px，中文字号保留；新增Arial 20/22/28，生成规格总数由50增至53。仪表本体的通用字体保留原样。Qt 默认字体尚无目标机运行时证据，不能将此选择宣称为字体族完全对齐；布局、回归与预览见 [SETTINGS_VISUAL_PORT.md](SETTINGS_VISUAL_PORT.md)。

2026-11-17 英文温度符号修复：原始 Bebas 字体不含 U+00B0（°），现在在 Bebas 28 静态字库中合入 Noto Sans CJK SC 同字号的该字符；生成器和 manifest 同步记录补字来源。其余 ASCII 字形描述、行高和基线与补字前一致。补回归复现了英文缺字符问题，修复后覆盖 °C/°F、中英切换；Windows 8/8、Linux 11/11 通过，含画面字体检查 1037 项、0 失败。[英文温度实际渲染](preview/fonts/temperature-en.png)。Release/AArch64 产物已更新，SHA256 `b32ad8b3b3a57365b6f2f37cc885665fa583ef22d0b0792191cb0c8fc4664b8f`；未部署。

## 实现和资源

- `src/ui/ui_fonts.h/.c` 统一管理字体族、字号、粗细与中文切换；查询只匹配已生成规格，未知规格返回 NULL。
- `src/ui/fonts/manifest.json` 记录 50 个规格及源字体哈希、合成粗体信息。生成 C 源约 31.4MB，实际只读字形数据与最终程序大小不同；板端体积和性能尚未实测。
- i18n 不再把 44/48 等大字号中文映射到 36，也不会把转速组件动态更新的字号恢复成旧缓存值。
- 互联页仅扩展中间容器的绘制范围，保留外层中心区域裁剪，避免原字体较宽的英文被截断；LVGL 9.5 需要同时设置 `OVERFLOW_VISIBLE` 和扩展绘制尺寸事件。
- 字体专项曾加宽设置标题文本框；后续设置显示对齐恢复 Qt 的标题宽度与分类层级，系统分类页不再另加大标题，详见 [SETTINGS_VISUAL_PORT.md](SETTINGS_VISUAL_PORT.md)。
- 旧中文字体源文件保留在工作区，但从生产 CMake 源列表移除，避免重复编译。

## 重新生成

依赖 Python 3、Node.js 和已安装的 `lv_font_conv` 1.5.3。生成器与 JS 包装脚本均在 `tools/`；运行时不依赖 Qt、Node 或 Windows 字体安装。

本机已执行：

```powershell
python tools/qt_fonts.py --qt-font-dir D:/share/CarHml_XIAO/font --converter D:/share/lvgl9.5demo/dashboard_t113/build/asset-tools/node_modules/lv_font_conv/lv_font_conv.js
```

Arial 默认读取 `C:/Windows/Fonts`，其他系统可用 `--arial-dir` 指定。Geely 原字体的 GPOS ClassDef 无效，转换器不能直接处理；生成器仅在 `.cache/qt-fonts/` 的副本中去除 GPOS/GSUB/GDEF，保留字形、字符映射与 legacy kern。原 Qt 文件不变，manifest 记录原始文件哈希。

Bebas 的所用文件只有常规字面，按 QML 粗体要求静态水平膨胀；中文粗体也静态合成。与 Qt 合成粗体的像素差异尚未核对。档位按用户后续要求使用 Forgotten 常规字重。中文子集来自两份现有翻译源码，不保证覆盖任意动态歌名、人名；新增译文后需重新生成。

## 已执行验证及范围

Windows MinGW 无窗口 LVGL 构建，7/7 组通过：display、i18n、overlays、turn_signal、dial、steering_key、fonts。字体专项带实际渲染为 987 项检查、0 失败；覆盖生成规格、字形、档位容器、车速规则、中英文切换、转速动态字号和互联三模式文字裁剪。

```powershell
cmake --build .cache/font-check/build -j 8
ctest --test-dir .cache/font-check/build --output-on-failure
./.cache/font-check/build/check_fonts.exe .cache/font-check/main-fonts.ppm
```

`.cache/font-check` 是本机临时验证工程，未纳入版本控制。当前工程正式 PC 构建依赖 Linux SDL2；本机无该环境，验证副本对时钟、mkdir、文件同步和测试临时路径作了 Windows 兼容处理，未改变生产 POSIX 实现。因此以上结果不等于 Linux/板端构建或设置持久化兼容验收。板端构建、内存/帧率和 Qt 同输入逐像素对照仍待验证。

已检查实际 1920×720 渲染图：主仪表、中英文 FM/来电/互联/设置。预览使用固定演示输入，无真实车辆写入。

- [主仪表（中文）](preview/fonts/main-zh.png)
- [主仪表（英文）](preview/fonts/main-en.png)
- [互联英文](preview/fonts/connect-en.png)
- [音乐中文](preview/fonts/music-zh.png)
- [设置英文](preview/fonts/settings-en.png)
- [设置中文](preview/fonts/settings-zh.png)
