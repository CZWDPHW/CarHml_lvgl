# 设置快速切换性能修复（2026-11-17）

用户反馈板端设置快速切换约 4fps，日志中多次 handler 最大耗时约 280ms。原日志的 `fps` 实际是 `lv_timer_handler()` 调用频率，不能直接当作实际出帧率。

## 已确认的问题

- 当前虚拟机 `build_board/CMakeCache.txt` 的 `CMAKE_BUILD_TYPE` 为空；修复前 LVGL 的 `C_FLAGS = -std=c11`，UI 也只有警告选项和 `-std=c11`，没有开启优化。软件绘制成本因此增加，但未实测其单独影响。
- 每次设置按键都刷新所有隐藏子页，重复隐藏/显示当前列表，重设图标源，遍历整棵树翻译，读取 `/etc/dash_ver`。
- 每次车况刷新都会重新写版本、油耗和速度报警显示；日期页相同数据也走全量 apply。本地回归复现连续 20 次相同输入产生 20 次绘制。
- 打开设置、可见性同步、next/back 存在重复 apply。

## 本次修改

- 未指定构建类型的单配置 BOARD 构建默认 Release；显式 Debug 等配置保留。
- 页面切换时才隐藏旧列表；按键只更新当前显示的分支。
- 未变化的图标源、行颜色及文本不重复设置；保留原文/译文缓存，隐藏分支到显示时再翻译。
- 系统版本文件每个页面实例读取一次；相同版本、油耗、日期、报警值不再重复刷新。清理日志的异步状态变化继续轮询并显示。
- 删除重复 apply。日期摘要在当前系统列表可见且日期发生变化时刷新。
- 主循环日志改用 `handler_hz` 表示调用频率，`fps` 使用 LVGL RENDER_READY 计数；增加 `render_avg`、`render_max` 和 `flush_total`。render 时间包含同步 flush；flush_total 是本窗口 flush 回调耗时总和，不是硬件扫描帧率或异步显示延迟。

车速映射、字体字号和按键业务动作没有改变。

## 验证

- Windows 临时无窗口工程：8/8 组通过；真实 UI 渲染检查 992 项、0 失败，检查了中文系统设置画面。
- Linux Release（`/tmp/car_dashboard_settings_perf_pc`）：11/11 组通过；新增 `test_settings_perf.c` 共 51 项检查，包含相同输入无绘制、日期页相同输入无绘制、相邻选择只局部重绘。
- 同一 Windows 测试配置，在独立设置页上连续上下切换 20 次并同步绘制：修改前中文约 75ms、英文约 60ms；修改后中文约 2–3ms、英文约 3–4ms。相邻行切换中文约 34,084 像素、英文约 45,046 像素，小于整个 594×300 视口。测试不含板端 CPU、仪表底图、UART 和 framebuffer，不能换算成板端帧率。
- BOARD：Linaro GCC 7.4.1、Release 构建成功；已核对 LVGL 与 UI 都使用 `-O3 -DNDEBUG`。项目源码无编译警告；第三方 glTF shader 原始字符串告警仍存在。
- CMake 配置验证：未指定类型时得到 Release，显式重新指定 Debug 后保持 Debug。

现有虚拟机缓存可这样重建：

```sh
cmake -S /mnt/hgfs/share/car_dashboard_lvgl \
  -B /mnt/hgfs/share/car_dashboard_lvgl/build_board \
  -DCARMW_PLATFORM=BOARD -DCMAKE_BUILD_TYPE=Release -DCARMW_BUILD_TESTS=OFF
cmake --build /mnt/hgfs/share/car_dashboard_lvgl/build_board -j8
```

产物：`build_board/bin/carmw_ui_demo`，ARM aarch64 ELF。

SHA256：`44ab9b577f413624397e0e23d7e1c35d55f8f1efa0727831e11f93eeac189483`。

本轮仅只读核对了板端旧程序的 SHA256（`9ef8b6549e4f4b30fd63267a422865eff410ac01354e827f127d640bde18386a`）；未部署、未启动板端程序、未发送真实车辆指令。需要替换新版后复测连续切换设置、日期编辑、普通仪表指针和样式切换，依据新版日志确认稳定 fps 与 render_max。
