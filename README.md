# lvgl_two — 车载 MCU 串口协议解析中间件

把 `0x2E` 帧流变成一份**带版本号和变更掩码的车况快照**：UI 只负责"取快照 + 判断哪一组变了"，
既不碰串口、也不做字节解包。

```
 0x2E 帧流 ──► parser ──► registry/decode ──► pending state ──► store ──► UI
                 │              │                                  ▲
                 └── ACK/NACK ──┴── session(握手) ──► TX 队列 ─────┘
```

无第三方依赖，只用 C11 + POSIX。核心层（parser / decode / state）不依赖任何平台设施，
串口和线程都被隔离在 port 层与 `car_link` 里。

---

## 1. 先跑起来

```bash
# 单元测试（不需要硬件）
make test

# 无硬件端到端演示：内存串口 + 假 MCU，跑 4 秒打印车况
make demo

# 或者 CMake
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

接真机：

```c
#include "carmw/car_link.h"
#include "carmw/car_port_posix.h"

static car_port_posix_t s_posix;
static car_port_t s_port;
static car_link_t s_link;

void car_link_bootstrap(void)
{
    car_port_posix_attach(&s_port, &s_posix, "/dev/ttyS2", 115200);
    car_link_init(&s_link, &s_port);     /* 打开串口，start 可选 */
}
```

---

## 2. 和 LVGL 的两种接法

**A. 协作式（推荐）** — 不创建线程，数据与 UI 天生同线程，不需要任何锁竞争：

```c
static void car_pump_timer(lv_timer_t *t)
{
    (void)t;
    car_link_pump(&s_link, 0);           /* 读串口 + 解析 + 提交快照 */
}

static void car_refresh_timer(lv_timer_t *t)
{
    (void)t;

    car_state_view_t view;
    car_link_view(&s_link, &view);       /* 拿快照，顺带清空 changed */

    if(view.changed & CAR_CH_SPEED_RPM) {
        lv_label_set_text_fmt(speed_label, "%u", view.state.speed_rpm.speed_kph);
        lv_bar_set_value(rpm_bar, (int32_t)view.state.speed_rpm.engine_rpm, LV_ANIM_OFF);
    }

    if(view.changed & CAR_CH_WARNINGS) {
        warning_panel_apply(&view.state); /* 自己实现 */
    }
}

/* lv_timer_create(car_pump_timer, 10, NULL);
   lv_timer_create(car_refresh_timer, 33, NULL); */
```

**B. 独立线程** — `car_link_start(&s_link)`，pump 在后台跑；UI 侧仍然只调 `car_link_view()`，
锁只保护队列和快照那次 1KB 的拷贝。

下发命令（两种模式下都一样，只入队不阻塞）：

```c
car_cmd_t req;

if(car_cmd_set_time(&req, 2026, 11, 1, 18, 30)) {
    car_link_exec(&s_link, &req);        /* 由 pump 线程真正写串口 */
}
```

---

## 3. 目录

```
include/carmw/
  car_types.h        帧格式、op 枚举、应答字节
  car_protocol.h     组帧 / 校验 / 单帧解析（纯函数）
  car_parser.h       字节流 -> 帧（流式状态机）
  car_registry.h     命令字典：长度边界 + 解码函数 + 变更掩码
  car_state.h        状态模型 + 快照存储 + 掩码定义
  car_command.h      下发命令构造器
  car_session.h      上电握手 / 初始化拉取状态机
  car_port.h         传输抽象
  car_port_posix.h   Linux 串口实现
  car_port_memory.h  内存端口（测试 / PC 演示）
  car_link.h         中枢：把上面全部缝起来
src/                 对应实现（+ car_decode.c / car_decode_internal.h 白盒解码器）
tests/               7 个测试套件
examples/demo.c      无硬件演示
```

---

## 4. 协议

```
0x2E | cmd_id | ctrl | len_lo | payload[0..4095] | checksum

ctrl  = (op_type << 4) | (len >> 8)      op_type 只有 4 bit
len   = ((ctrl & 0x0F) << 8) | len_lo    最大 4095
chk   = (cmd_id + ctrl + len_lo + Σpayload) ^ 0xFF      ← 不含帧头，累加后取反
```

案例：车速 120 km/h、转速 5000 rpm

```
2E 04 C0 04 00 78 13 88 24
   │  │  └─ 04 长度 4
   │  └──── C0 = op 0xC(Status) << 4 | 长度高 4 位 0
   └─────── 04 命令号
```

> 注意这套"长度挤进第 3 字节、校验取反"的编码和常见的
> "整字节 op + 16 位长度 + 累加和"**不兼容**。`car_protocol.c` 里的
> `car_frame_encode()` 是唯一事实源，测试里直接断言了上面这串字节。

应答字节（收到每帧必回一个）：

| 字节 | 含义 |
|------|------|
| `0xFF` | 收下且解析成功 |
| `0xF0` | 校验和错 |
| `0xF3` | 命令/长度/内容非法 |
| `0xFC` | 资源不足（本实现不用，保留兼容） |

---

## 5. 六个设计决策（每条都对应一个真实坑）

**① 先验长度校验，假帧头不会卡住整条链路**
读到 `ctrl/len_lo` 立刻用命令字典判断 `(cmd, len)` 是否存在。`0x04` 的长度只可能是 4，
所以载荷里混进一个 `0x2E` 时，下一轮判断当场否掉，只丢这 1 字节。

**② 校验失败只退 1 字节，而不是丢整帧**
坏候选帧的"载荷"里可能正藏着真帧。`car_parser.c` 在失败时只前移 1 字节重新扫描，
测试 `test_parser.c` 第 6 组专门验证"坏帧 + 好帧一次喂入，好帧必须被找回来"。

**③ 表驱动，长度边界和解码器写在同一个结构体里**
`car_registry.c` 一张表同时喂给解析器（policy）、校验器和解码分派，
不存在"校验说合法、解码说不够长"的两处漂移。不变式（`mask != 0` 必须有 `decode`）
由 `tests/test_registry.c` 守着。

**④ 变更掩码 + 快照**
解码结果先落进写者私有的 `pending`，一轮 pump 结束才 `apply` 一次，
UI 永远看不到"解了一半"的状态。`view.changed` 是自上次 `car_link_view()` 以来的
变更掩码，UI 按组刷新而不是全量重绘。

**⑤ 协作式 pump 和独立线程共用同一条代码路径**
`car_link_pump()` 就是全部逻辑；线程模式只是一个 `while(!stop) pump(20)` 的壳。
没有两套并发实现，也就没有两套 bug。

**⑥ 端口抽象 + 内存端口**
`car_port_t` 只有 5 个函数指针。板子上是 termios+poll，测试和演示是内存端口，
于是握手、ACK、拆包、状态合并**全部能在 PC 上端到端验证**，不需要等你焊好串口。

---

## 6. 测试覆盖

| 套件 | 覆盖内容 |
|------|----------|
| `test_parser` | 组帧字节、逐字节喂、粘包、前导噪声、坏校验恢复、长度非法重同步、载荷内含 `0x2E`、4095 最大帧、窗口边界 |
| `test_registry` | 表有序、二分查找自洽、长度上下界、未登记命令策略、全解码器烟雾测试 |
| `test_decode` | 20+ 条命令的正常/越界/截断用例，含"拒收时不得污染旧状态" |
| `test_state` | 快照版本号、变更掩码累计与清零、故障灯列表/整表优先级 |
| `test_session` | 上电时序、1s 重试、40ms 批量拉取 13 条、READY 后不再重试 |
| `test_command` | 下发帧的位域与字节级断言（时间/里程/单位/HUD/灯光/泊车/车辆设置） |
| `test_link` | 内存端口端到端：握手、ACK/NACK 四类应答、按键事件、半帧跨 pump、统计口径、串口缺失时降级 |

```bash
make test          # 或 ctest --test-dir build --output-on-failure
```

---

## 7. 加一条新命令（3 步）

1. `src/car_decode_internal.h` 声明 `bool car_decode_xxx(const uint8_t*, uint16_t, car_state_t*)`
2. `src/car_decode.c` 实现它：**先校验长度和取值范围，再写状态**（失败必须原样返回 false）
3. `src/car_registry.c` 表里按 `cmd_id` 升序插一行：

```c
{0x42, 3, 3, CAR_CH_XXX, car_decode_xxx, "New feature"},
```

`car_state.h` 里加一个 `CAR_CH_XXX` 位和对应字段即可。解析器、快照、链接层都不用改。

---

## 8. 移植到 MCU

- 编译时加 `-DCARMW_NO_THREADS`：锁退化为空操作，`car_link_start()` 恒返回 false，
  只有协作式 pump 可用（本来也更适合裸机/RTOS 主循环）。
- 不要编译 `src/port/car_port_posix.c`，换成你自己的 UART 收发实现（5 个函数指针）。
- 主要内存开销：`car_link_t` 约 12KB（其中解析窗口 4.1KB、待处理状态约 1.6KB、
  收发队列约 0.7KB）。只跑 parser+decode 的话，静态 RAM 约 4.5KB。
- 全部代码无 `malloc`，无递归，栈上最大局部变量 256 字节。

---

## 9. 已知边界（有意为之）

- **发送队列载荷上限 16 字节**：协议里所有可定义命令的载荷都 ≤ 11 字节；
  真要发 4KB 的升级帧，请直接写自己的 raw 通道，不要走队列。
- **`view.changed` 是单读者语义**：一个 UI 线程取快照。多读者请各自记 `version` 做差分。
- **没有自动重传**：ACK/NACK 只负责告诉 MCU 结果，重传策略在 MCU 侧（与现网一致）。
- **`0x01/0x07/0x3A/0x3B/0x3D/0x40/0x41` 只放行不解析**：这些是调试/透传命令，
  载荷结构未定义；需要时按第 7 节加解码器即可，它们已经在表里占位（`decode == NULL`）。
