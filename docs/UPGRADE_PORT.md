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
