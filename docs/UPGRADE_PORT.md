# Qt 升级迁移与回退

升级前检查点：`c292787`，GitHub 标签 `pre-upgrade-20261118`。
分支：`codex/qt-upgrade-port`。恢复整个升级前源码可切换该标签；保留后续其它修改时，按逆序 revert 升级提交。
Git 回退是源码回退，不会自动撤销已经写入设备的系统文件或 MCU 固件。

## 分步提交

1. MCU 协议与 MD5：移植 Qt 的 CRC、头包、数据包、重传、进度、流校验、EOT。
2. Linux 升级执行：兼容 U 盘目录、APP 备份切换、system_update 文件安装、MCU 独占串口与恢复。
3. 接入：Qt 升级弹窗、后台检测、设置保存、5 秒重启、rcS 启动脚本及部署说明。

每步提交并推送，不修改升级前标签。Qt 参考为 `CarHml_XIAO/src/upgrademanager.cpp`、`ymodem.cpp`、`qml/ComControl/UpgradePopup.qml`。

第 1 步提交：`26e0b43`。MD5 标准向量、百万字符、Qt 方言模拟 MCU（短写、重传、包号回绕、CRC、填充、流校验、取消、EOT 超时）已在 Windows/MinGW 运行。
第 2 步提交：`d14f5ae`。第 3 步提交为弹窗、启动脚本和主程序接入。

第 2 步包含 Linux worker 与文件事务：常规串口由 UI 线程停止并 join，升级线程独占新串口，所有退出路径请求 UI 恢复常规端口与握手。升级线程不调用 LVGL。
系统目录覆盖、权限和 MD5 规则保持 Qt，逐文件临时写入、fsync、原子替换。临时文件使用独占创建；若断电留下 `.upgrade-new`，不会直接覆盖它，应先检查对应事务后清理。

APP 使用 `/opt/car/releases/<binary-md5>/car_dashboard_hml` 和同目录 `assets/`；`car_A`/`car_B` 为启动入口，`boot_flag` 仍为 0/1。发布前准备完整新版本与资源，再将 B 指向旧版本、flag=1、A 指向新版本、flag=0，避免删除唯一可启动程序。
首次迁移旧的普通 `car_A` 会保留 `car_baseline`。原有 `setting.cfg` 路径不变。
U 盘可额外提供 `assets/`；只有程序且资源未变时，自动复制当前版本资源到新版本。更新资源也必须发布新程序版本（新的 MD5），避免相同程序版本被当作无需更新。

文件测试使用显式临时根目录，覆盖错误校验、首次安装、已安装版本、残留临时文件、跨卷组件发现、系统字体/Logo 替换，绝不写测试主机的 `/usr` 或 `/etc`。

## 兼容约定

```
/mnt/usb/<volume>/
  car_dashboard_hml
  car_dashboard_hml.md5
  <firmware>.bin
  <firmware>.bin.md5
  system_update/
    bin/          -> /usr/bin
    lib/          -> /usr/lib
    share/fonts/  -> /usr/share/fonts
    etc/          -> /etc
    res/          -> /mnt/resource
```

执行顺序：system → APP → MCU；APP 成功后等待 7 秒再开始 MCU。
MCU：`/dev/ttyS2` 115200，FF/02 请求中含三字节大端长度及 UTF-8 文件名，等待 FF 状态同意帧。
YMODEM 保留 Qt 方言：头包 ACK 后 C；**数据包以 C 确认**；剩余 >128 用 1024 字节，否则用 128 字节；0x1A 填充；CRC16 CCITT 初值 0。
流 MD5 只统计确认成功的原始数据，排除填充和重传。

修正 Qt 的错误结果汇总、首次 APP 跳过校验、错误路径不恢复串口等行为；EOT 失败不能报成功。
Qt 并没有实际实现最终 A2 帧，不凭空添加 MCU 协议。
system_update 是在线文件覆盖，不等于系统分区 A/B。板端断电、串口和 MCU bootloader 需要真机验收。

## 首次部署到板子

在 Linux 交叉编译得到的可执行文件实际目标名见 CMake，不要把 Windows 测试程序放上板。
先停止旧 watchdog，避免两个监督脚本同时拉起串口程序。首次手动放置：

```
/opt/car/
  car_A             # 已编译的 LVGL 程序，chmod 755
  assets/           # 本仓库完整 assets/，和 car_A 对应
  boot_flag         # 内容 0
  watchdog.sh       # deploy/watchdog.sh，chmod 755，LF 换行
  runtime_env.sh    # 可选：板端动态库路径等环境，不依赖 Qt
```

把 [rcS 示例](../deploy/rcS-upgrade-snippet.sh) 的块加到现有 `/etc/init.d/rcS` 末尾，仅添加一次，不覆盖原来的挂载、网络等启动流程。
watchdog 使用简单循环，不加锁、不记录 PID；由 rcS 保证只启动一次。断电后进程自然结束，下次开机由 rcS 重新启动。
每次拉起程序重新读取 `boot_flag`，并进入该版本目录加载自己的 `assets/`；退出码 75 保留为升级重启的后备处理。
必须保留已有 U 盘自动挂载服务；升级模块监测挂载结果，本身不执行 mount。
持久设置仍使用 `/opt/car_dashboard/setting.cfg`。

升级包延续 Qt 的文件名。板端新程序在 U 盘命名为 `car_dashboard_hml`，用 Linux `md5sum car_dashboard_hml > car_dashboard_hml.md5` 生成配套校验。
MCU 同理：`md5sum firmware.bin > firmware.bin.md5`，长度必须为 1..16777215 字节，UTF-8 文件名最多 110 字节。
新增或修改图片时在同一 U 盘根目录附上完整 `assets/`，程序版本也须更新。
不需要新 tar.gz 包，不增加对 MCU 的新协议要求。

APP、MCU 的本地 MD5 分别是 `/opt/car/car_dashboard_hml.md5`、`/opt/car/bin.md5`。MCU 失败保留 `mcu_flag=1`，下次启动显示中断提示，重新插入正确固件包后重试；不在没有 U 盘的情况下猜测或自动烧写缓存文件。
全部组件结束后，只要 APP 发布或系统文件发生变更就等待 5 秒重启；单独 MCU 更新不重启 Linux。
重启前保存设置并停止工作线程，再执行 `/sbin/reboot`；执行失败时返回 75，由 watchdog 尝试 `reboot`。普通退出/崩溃则 1 秒后重新启动。

## 板端程序回退

停止 watchdog 后，写 `boot_flag=1`，再次启动 watchdog 即使用 B 及其原资源目录。
此操作只回退 APP；system_update 更新的系统文件和 MCU 固件不随 APP 回退。
源码回退使用 `git switch --detach pre-upgrade-20261118`；需要保留其它后续提交时，按第 3、2、1 步逆序 revert。

## 验证记录与真机验收

已运行：Windows/MinGW 核心链路 1743 项检查、MD5/YMODEM 模拟传输、隔离文件安装测试，以及包含升级弹窗在内的 9 项 UI 回归。
升级弹窗 1300 项检查覆盖中英文、字体缺字、800×580 居中、文字边界、特殊方向盘路由拦截及显示/销毁；四张预览在 `docs/preview/upgrade/`。
脚本通过 `sh -n` 语法检查。

本次 Linux 开发虚拟机不可连接，因此 Linux worker、symlink APP 发布与实际 watchdog 进程测试尚未在 Linux 运行；Windows 的 POSIX 线程声明检查不代替 Linux 编译。
Linux 专用 APP 发布和资源回退测试已包含在 `test_upgrade_files.c`，watchdog 测试在 `test_watchdog.sh`。在 Linux 上执行：

```sh
cmake -S . -B build-upgrade -DCARMW_BUILD_UI=OFF
cmake --build build-upgrade --parallel 4
ctest --test-dir build-upgrade --output-on-failure
```

上板还需验证：只有 APP、只有 MCU、只有 system_update、三者组合；缺失/错误 MD5、同版本、多个 bin、拔盘、串口失败；升级失败后常规握手恢复；5 秒重启与设置保存；APP 发布过程各断电点；实际 MCU 的 FF 同意帧、C 应答、EOT 和固件启动结果。
在完成这些真机检查前，不把本地模拟测试当作烧写成功证明。
