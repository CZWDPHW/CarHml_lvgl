# LVGL 车载仪表 · 协议解析中间件 lvgl_two

## 设计与使用手册

---

**文档说明**

本文档由 2026-11-01 至 11-02 的开发对话整理而成，内容覆盖：既有两套实现的对比结论、`lvgl_two` 的架构与代码走读、接入 LVGL 的方法、目标平台（Allwinner T113-S3 + LVDS）的性能取舍、以及串口收发模型。

**目标读者**：接手 `lvgl_two` 的嵌入式软件工程师。

**重要声明**：文档中所有代码与行号均取自当时磁盘上的源码；`lvgl_two` 的全部代码在本机**未经过编译器验证**（开发环境的 shell 进程无法启动，错误码 0xC0000142）。首次在 Linux/WSL 上执行 `make test` 时请预留排错时间。

---

## 目录

1. 背景：两套既有实现的问题
2. lvgl_two 架构
3. 代码走读：一帧的完整旅程
4. 接入与使用
5. 目标平台：T113-S3 + LVDS
6. 串口收发模型
7. 已知问题与待办
8. 附录

---

# 1. 背景：两套既有实现的问题

## 1.1 对比结论

| 维度 | lvgl_one | car_dashboard_lvgl | lvgl_two |
|---|---|---|---|
| 定位 | 传输层骨架 | 分层协议栈 | 分层 + 可测试 + 可移植 |
| 解析/传输 | serial_com.c 419 行包干 | vehicle_protocol.c + vehicle_source.c | car_parser.c + car_link.c |
| 报文校验 | 无 | 命令表（min/max 长度） | 命令表 + 先验校验 |
| 载荷解码 | 仅 0x04，其余 TODO | 两套解码器（文本摘要 + 结构化） | 表驱动单一解码器 |
| 下发 | 裸 send + 心跳 | 30+ 语义化命令 | 22 个构造器 + 队列 |
| 握手 | 无 | 有（1s 重试 / 40ms 批量） | 有（同样时序，状态可查） |
| 测试 | 无 | 7 个 CTest | 7 个套件（含端到端） |
| 中间件代码量 | 约 640 行 | 约 2700 行 | 约 3800 行 |

## 1.2 帧格式：两种互不兼容的解释

协议帧头是 `0x2E`，但两套实现对第 3、4 字节的解释**完全不同**。以真实帧"车速 120 / 转速 5000"为例：

```
2E 04 C0 04 00 78 13 88 24
```

| 字段 | car_dashboard_lvgl（正确） | lvgl_one（错误） |
|---|---|---|
| cmd | `0x04` | `0x04` ✓ |
| op_type | 第 3 字节高半字节 → `0xC` | 第 3 字节整字节 → `0xC0` ✗ |
| length | 第 3 字节低半字节 << 8 或 第 4 字节 → `4` | 第 4、5 字节拼 16 位 → `1024` ✗ |
| checksum | `(04+C0+04+00+78+13+88) ^ 0xFF = 0x24`，不含帧头 | 累加含帧头、不取反 ✗ |

**结论**：`lvgl_one` 的 `find_frame` 读到 `1024 > 256` 直接判"长度非法"，真机一帧都解析不出来；它自己发出的帧 MCU 也不认。更能说明问题的是它的仿真脚本 `scripts/mock_serial.py` 是照着自己那套错格式造帧的，所以自测"通过"，真机必然空屏。

`lvgl_two` 以 `car_dashboard_lvgl` 的字节含义为准（其字节序列经现场联调验证）。

## 1.3 lvgl_one 的缺陷清单

| 级别 | 位置 | 问题 |
|---|---|---|
| P0 | serial_com.c 长度字段 | 按 16 位读，真实帧被判非法 |
| P0 | serial_com.c op_type | 用整字节，真实帧 op 在高半字节 |
| P0 | serial_com.c 校验和 | 含帧头且不取反 |
| P0 | vehicle_data.c | `vehicle_data_update()` 是空函数，解码全 TODO |
| P0/P1 | serial_com.c 发送缓冲 | `frame[2+256]`=258 字节，满长帧需 262，栈溢出 4 字节 |
| P1 | serial_com.c 波特率 | `cfsetispeed` 传裸 115200 而非 `B115200`，未定义行为 |
| P1 | serial_com.c 停止线程 | 往串口写一个 `0x00` 唤醒 read，污染 MCU 链路 |
| P1 | 全文件 | 无 ACK/NACK、无握手、无断链降级、无单测 |
| P2 | vehicle_data.h | `CMD_SERIAL_OPEN_FAIL = 0xFFF` 塞进 uint8_t 枚举 |

## 1.4 car_dashboard_lvgl 的结构性弱点

| 级别 | 问题 | lvgl_two 的对策 |
|---|---|---|
| P1 | 长度字段无合理性校验，假帧头最多吞 4095 字节才重同步 | 命令表做先验校验，假帧头当场丢弃 1 字节 |
| P1 | 测试文件重复定义变量，编译不过 | 已修正 |
| P2 | 文本解码器与结构化解码器 min-length 不一致（0x05: 5 vs 3） | 表驱动，长度与解码器写在同一行 |
| P2 | 缺 0x16 子命令结构化解析 | 部分实现（drive_mode / 陀螺仪） |
| P3 | op_type 硬绑 4 bit | 协议所限，保留 |

---

# 2. lvgl_two 架构

## 2.1 数据流

```
        +--------------+  5 个函数指针
硬件/内存 |  car_port_t  |  open/close/read/write/now_ms
        +------+-------+
               | 裸字节
        +------v-------+
        |  car_parser  |  4100 字节滑动窗口，零 malloc
        +------+-------+  先验长度校验 + 1 字节重同步
               | (cmd, op, payload, len) 回调 link_on_frame
        +------v-------+
        |   registry   |  47 条命令表（长度/解码器/掩码）
        |   + decode   |  39 个解码器
        +------+-------+
               | 写 link->pending + 或上掩码
        +------v-------+
        | state store  |  apply 一次 = 1KB 拷贝 + version+1
        +------+-------+
               | car_link_view()   ← 所有 UI 代码的唯二入口（另一个是 take_keys）
        +------v-------+
        |    LVGL UI   |
        +--------------+
```

## 2.2 五层职责

| 层 | 文件 | 只关心 | 绝对不该做 |
|---|---|---|---|
| 端口 | car_port_posix.c / car_port_memory.c | 收字节、发字节 | 不认识 0x2E |
| 拆帧 | car_parser.c | 字节 → 帧 | 不认识 cmd_id 含义 |
| 语义 | car_registry.c + car_decode.c | 载荷 → 结构体字段 | 不碰串口、不碰 LVGL |
| 存储 | car_state.c | 快照 + 版本号 + 变更掩码 | 不知道谁在读 |
| 编排 | car_link.c | 定时、握手、ACK、队列 | 不做业务判断 |

## 2.3 目录

```
lvgl_two/
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
    car_lock.h         互斥封装（无 pthread 平台自动退化）
    car_link.h         中枢
  src/                 对应实现（另有 car_decode.c / car_decode_internal.h）
  tests/               7 个测试套件
  examples/demo.c      无硬件演示
  Makefile / CMakeLists.txt
```

## 2.4 六个设计决策

| 决策 | 解决的具体问题 |
|---|---|
| 帧长度先验校验（registry 作为 parser 的 policy） | 载荷里混进 0x2E 时谎报的长度被当场否掉，只丢 1 字节；不会像旧实现那样最多吞 4095 字节才重同步 |
| 校验失败只前移 1 字节 | 坏候选帧的"载荷"里藏着的真帧能被捞回来 |
| 表驱动单一事实源 | 长度边界、解码函数、变更掩码写在同一行，杜绝两处漂移 |
| 变更掩码 + 一次提交 | 解码先落写者私有 pending，一轮 pump 结束才 apply；UI 看不到解了一半的状态 |
| 协作式 pump 与线程模式同一代码路径 | 线程模式只是 `while(!stop) pump(20)` 的壳，不存在两套并发实现 |
| 端口 5 函数指针抽象 | 板子上用 termios+poll，测试/演示用内存端口，全链路可在 PC 上端到端验证 |

## 2.5 并发模型：默认单线程

**默认是纯单线程，连一个 pthread_create 都不会调。** 多线程是可选，且开了也只有 2 个线程。

| | A. 协作式（默认推荐） | B. 后台线程（可选） |
|---|---|---|
| 谁调 car_link_pump | LVGL 定时器（主线程） | car_link_start() 起的线程 |
| 线程数 | 1 | 2 |
| 锁竞争 | 无 | 有，临界区都很小 |
| 数据一致性 | 天然一致 | 靠快照保证 |
| 代码路径 | 完全相同 | 完全相同 |

线程模式只是一个壳：

```c
static void *link_thread_main(void *argument)
{
    car_link_t *link = (car_link_t *)argument;
    while(!link->stop_requested) {
        car_link_pump(link, 20);
    }
    return NULL;
}
```

### 数据所有权

| 数据 | 写者 | 读者 | 保护 |
|---|---|---|---|
| link->parser | pump 线程 | 无 | 无需（独占） |
| link->pending + pending_mask | pump 线程 | 无 | 无需（写者私有） |
| link->session | pump 线程 | 无 | 无需（独占） |
| link->store.state | pump（apply） | UI（view） | store 锁 |
| TX 队列 | UI（exec） / pump（drain） | — | link 锁 |
| 按键队列 | pump（push） / UI（take） | — | link 锁 |
| 诊断环 | pump（push） / UI（take） | — | link 锁 |
| link->stats | pump | UI | 无锁（仅诊断） |

### 全部 8 个临界区

```
store 锁：car_state_store_apply   1KB memcpy + 计数
          car_state_store_view    1KB memcpy + 清掩码
link  锁：link_trace_push         30B 结构体
          link_key_push           4B
          link_drain_tx           取一条 tx_item
          car_link_exec           存一条 tx_item
          car_link_take_keys      每条 2B
          car_link_take_trace     每条 30B
```

按 20Hz 收帧计算，每秒最多几十次锁进出，开销可忽略。

### 三个不严格之处（如实记录）

1. `car_link_stats()` 返回裸指针，UI 读计数器与 pump 线程自增之间是 C11 意义上的 data race。x86-64 上 8 字节对齐读写不会撕裂，纯诊断用途。要严格就改成加锁拷贝的 `car_link_stats_copy()`。
2. `stop_requested` 是 `volatile bool`，配合 `pthread_join` 在实践上足够；标准层面不如 `_Atomic bool`。
3. `view.changed` 是单读者语义：一个 UI 线程取快照没问题；两个线程同时 view 数据不会坏，但"自上次以来变了什么"会互相抢着清零。

### 两条硬约束

- `car_link_pump()` 只能有一个线程在调（单写者）。
- `CARMW_NO_THREADS` 下 `car_link_start()` 恒返回 false，只有协作式可用。

---

# 3. 代码走读：一帧的完整旅程

## 3.1 调用链总览

```
[入口 A：收数据] pump_timer (你的, 10ms)
+- car_link_pump                      car_link.c:282
   +- car_port_now                    car_link.c:288
   +- car_session_next                car_link.c:292
   +- link_drain_tx                   car_link.c:300
   +- port->read  --> posix_read      car_link.c:310
   +- car_parser_feed                 car_link.c:322
   |  +- parser_drain (每字节一次)     car_parser.c:216
   |     +- parser_accepts            car_parser.c:131
   |     |  +- car_registry_accept    car_registry.c:105
   |     |     +- car_registry_find   car_registry.c:75
   |     +- car_frame_checksum        car_parser.c:146
   |     +- link_on_frame             car_link.c:140    <== 唯一分发点
   |        +- car_registry_find      car_link.c:158
   |        +- car_session_on_frame   car_link.c:169    (仅 cmd=0x00)
   |        +- desc->decode           car_link.c:173    <== 变量在这里被写
   |        |  +- car_decode_speed_rpm car_decode.c:95
   |        |     +- rd_u16 x2         car_decode.c:7
   |        +- pending_mask |= mask   car_link.c:180
   |        +- link_key_push          car_link.c:183    (仅 cmd=0x20)
   |        +- link_trace_push        car_link.c:189
   |        +- link_reply(0xFF)       car_link.c:194    -> posix_write
   +- link_commit                     car_link.c:329
      +- car_state_store_apply        car_link.c:209 -> car_state.c:97

[入口 B：取数据] ui_timer (你的, 33ms)
+- car_link_view                     car_link.c:417
   +- car_state_store_view            car_state.c:118    <== 变量拷到你手里
```

## 3.2 逐跳明细

以 `2E 04 C0 04 00 78 13 88 24`（车速 120、转速 5000）为例，一次 pump 内发生的事：

| # | 函数 | 位置 | 关键入参 | 产出 / 副作用 |
|---|---|---|---|---|
| 1 | car_link_pump | car_link.c:282 | timeout=0 | 整个流程的宿主 |
| 2 | car_port_now -> posix_now -> car_now_ms | car_port.c:30 | &link->port | now_ms（握手/NACK 限速用） |
| 3 | car_session_next | car_session.c:153 | now_ms | 通常 false，仅握手/批量拉取期间吐命令 |
| 4 | link_drain_tx | car_link.c:216 | — | 队列空则立刻返回 |
| 5 | port->read -> posix_read | car_link.c:310 -> car_port_posix.c:127 | 256B 栈缓冲 | got = 9 |
| 6 | car_parser_feed | car_parser.c:188 | buf, 9, link_on_frame, link | 逐字节 append |
| 7 | parser_compact | car_parser.c:14 | — | 偶发（start>0 时） |
| 8 | parser_drain | car_parser.c:87 | — | 每 append 1 字节调一次，共 9 次 |
| 9 | parser_accepts -> car_registry_accept -> car_registry_find | car_parser.c:70 -> car_registry.c:105 -> car_registry.c:75 | (0x04, 4) | 命中表项，返回 true |
| 10 | car_frame_checksum | car_parser.c:146 -> car_protocol.c:5 | (0x04,0xC0,0x04,payload,4) | 0x24，与末字节相等，ok=true |
| 11 | link_on_frame（经函数指针 on_frame） | car_parser.c:162 -> car_link.c:140 | (0x04, 0x0C, true, buf+4, 4, link) | 进入业务分支 |
| 12 | car_registry_find(0x04) | car_link.c:158 | cmd_id | 拿到 desc = &s_commands[4] |
| 13 | car_session_on_frame | car_link.c:169 -> car_session.c:93 | — | 跳过（cmd_id != 0x00） |
| 14 | desc->decode(...) | car_link.c:173 | &link->pending | 核心：写变量 |
| 15 | car_decode_speed_rpm | car_decode.c:95 | data, len, &pending | 见 3.3 |
| 16 | rd_u16(data+2) / rd_u16(data) | car_decode.c:7 | — | 5000 / 120 |
| 17 | link->pending_mask 或上 desc->mask | car_link.c:180 | CAR_CH_SPEED_RPM | 标记这组变了 |
| 18 | link_trace_push | car_link.c:189 | 帧内容 | 诊断环 +1 |
| 19 | link_reply(link, 0xFF) -> posix_write | car_link.c:194 | 1 字节 | MCU 收到 ACK |
| 20 | 回到 parser_drain：start += 9 | car_parser.c:176 | — | 整帧消费，窗口清空 |
| 21 | link_commit | car_link.c:329 -> car_link.c:203 | — | mask 非 0，继续 |
| 22 | car_state_store_apply | car_state.c:97 | &pending, mask | 1KB 拷贝 + version++ |

第 14 步的 `desc` 就是表里这一行（car_registry.c:14）：

```c
{0x04, 4, 4, CAR_CH_SPEED_RPM, car_decode_speed_rpm, "Speed and RPM"},
```

`desc->decode` 是函数指针，跳进 `car_decode_speed_rpm`。整条链路唯一"认识车速"的地方在 car_decode.c:111-113：

```c
state->speed_rpm.speed_kph  = speed;   /* 变量在这里诞生 */
state->speed_rpm.engine_rpm = rpm;
state->speed_rpm.valid      = true;
```

注意此时 `state` 参数是 `&link->pending`，还不是 UI 能看到的那个。

## 3.3 字节级走查：9 次 drain 只有第 9 次出货

`car_parser_feed` 是"加一个字节就 drain 一次"，所以 9 个字节对应 9 次调用：

| i | 字节 | end | avail | drain 里的判断 | 结果 |
|---|---|---|---|---|---|
| 0 | 2E | 1 | 1 | avail < 4 | return |
| 1 | 04 | 2 | 2 | avail < 4 | return |
| 2 | C0 | 3 | 3 | avail < 4 | return |
| 3 | 04 | 4 | 4 | cmd=0x04, ctrl=0xC0, len=4；accept=true；total=9；avail(4)<9 | return（半帧） |
| 4 | 00 | 5 | 5 | 同上 | return |
| 5 | 78 | 6 | 6 | 同上 | return |
| 6 | 13 | 7 | 7 | 同上 | return |
| 7 | 88 | 8 | 8 | 同上 | return |
| 8 | 24 | 9 | 9 | avail>=9，校验 0xDB^0xFF=0x24 等于 buf[8]，ok；回调；start+=9 | 出货 1 帧 |

回到 feed 的循环，第 9 字节处理完 `start == end == 9`，下一次 drain 走 `avail==0` 分支把窗口归零。

## 3.4 变量一共有三份副本

这是最容易搞混的点。

| 时点 | 表达式 | 值 | 谁能看见 | 生命周期 |
|---|---|---|---|---|
| decode 返回瞬间 | link->pending.speed_rpm.speed_kph | 120 | 只有 pump 线程 | 常驻，累积态（不重置） |
| commit 之后 | link->store.state.speed_rpm.speed_kph | 120 | 加锁，任何读者 | 常驻 |
| car_link_view 之后 | view.state.speed_rpm.speed_kph | 120 | 你的代码 | 你的栈副本，永久 |

两个推论：

1. `pending` 是累积的：它保存 37 个分组的完整车况，0x04 只改 speed_rpm 那一组，fuel 保持上次的值。所以 apply 才能"整体覆盖"（car_state.c:106 `store->state = *sample;`）。
2. 直接读 `link.pending` 是错的：那是写者私有状态，线程模式下随时被改，而且可能只解了一半。

## 3.5 其它帧类型的分叉点

| 收到的帧 | 从哪一步开始不同 | 最终结果 |
|---|---|---|
| 0x04 正常 | 主线 | pending.speed_rpm + ACK 0xFF |
| 0x20 按键 | car_link.c:183 多走 link_key_push | 状态组 + 按键队列（UI 用 take_keys 取） |
| 0x00 握手 | car_link.c:169 多走 car_session_on_frame | 会话推进到 bulk/ready，pending.init.state |
| 校验错 | car_link.c:152 | 只 trace + NACK 0xF0，不进 decode、不提交 |
| 未登记 cmd | car_link.c:160 | frames_unknown++ + NACK 0xF3 |
| decode 拒收 | car_link.c:174 | frames_rejected++ + NACK 0xF3，状态不被污染 |
| 长度与表不符 | car_parser.c:131 | 根本到不了 link_on_frame，只 length_rejects++ |
| 半帧 | car_parser.c:142 | 不回调，link_commit 这轮空转 |

## 3.6 为什么是表驱动而不是 switch

`car_link.c:173` 的 `desc->decode(...)` 就是分发点。`desc` 来自 `car_registry_find(cmd_id)`（二分查找），**一张表 + 一次查找 + 一次间接调用**，语义上等价于 switch。

但这张表要同时承担 5 个角色，而 switch 只能承担 1 个：

| 角色 | 谁在用 | 用 switch 怎么写 |
|---|---|---|
| 帧长度合法性（解析器拆帧前就要用） | car_parser.c:131 的 policy | 得再写一个 47 分支的 switch |
| 载荷最小/最大长度 | 同上 | 同上 |
| 解码函数 | car_link.c:173 | switch 可以 |
| 变更掩码 mask | car_link.c:180 | 得在 switch 里顺手赋 |
| 可读名字（日志/诊断页） | car_cmd_name() | 得再写一个 47 分支的 switch |

关键在第一条：解析器在还没收全帧的时候就要判断"这个长度合不合理"，这个判断必须和解码器的要求是同一份数据。否则就会出现旧项目那种漂移。

### 加一条命令：1 行 vs 3 处

现在（表驱动）：

```c
{0x42, 3, 3, CAR_CH_NEW_FEATURE, car_decode_new_feature, "New feature"},
```

换成 switch 则要改 3 处，其中 2 处是必须保持一致的 47 分支 switch。而且只要每个 case 都写成 `{长度检查, 解码函数, 掩码}` 的同一形状，你其实就是手写了一张表，只不过写成控制流形式。

### 性能账

| | 当前 | 纯 switch |
|---|---|---|
| 查找 | 6 次二分比较，约 12~18 cycles | 跳表或判定树，约 3~6 cycles |
| 调用 | 间接调用（不可内联） | 直接调用（小函数可内联） |
| rodata | 47 x 32B 约 1.5 KB | 跳表约 1KB 或纯代码 |
| 每帧差额 | — | 约 10~20 cycles |

按 115200 波特率、每秒 1280 帧计算：1280 x 20 = 25,600 cycles/s，占 T113 单核 1.2GHz 的 0.002%，任何测量里都看不见。

代价：每帧会做两次 car_registry_find（policy 一次、link_on_frame 一次），约 30 cycles，占 0.004%，同样不值得为它牺牲分层。

### switch 更好的三种情况

1. 命令少于 8 条且无附带的长度/掩码元数据 —— switch 更短更直观。
2. 需要极致内联到小 flash MCU（32KB 级别时 1.5KB 指针表肉疼，函数指针无法内联）。T113 上不成立。
3. 想让编译器用 `-Wswitch-enum` 帮忙查漏。表驱动没有这个保护，只能靠 `frames_unknown` 计数器和测试里那条"mask 非 0 必须有 decode"的不变式。

---

# 4. 接入与使用

## 4.1 构建

```bash
make            # 构建静态库 + 单测 + 演示
make test       # 跑 7 个套件
make demo       # 无硬件端到端演示（假 MCU + 内存串口）
make clean

# 或 CMake
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

交叉编译：

```bash
make CROSS=aarch64-linux-gnu- TARGET=arm
```

Windows 下可直接编库和测试（`car_lock.h` 检测到 `_WIN32` 会自动退化 `CARMW_NO_THREADS`），只有 demo 和 POSIX 串口部分限定 `UNIX`。

## 4.2 初始化

```c
#include "carmw/car_link.h"
#include "carmw/car_port_posix.h"

car_link_t g_link;                    /* 全工程唯一一份 */

int main(void)
{
    static car_port_posix_t posix;
    static car_port_t port;

    lv_init();
    /* ... display / indev 初始化 ... */

    car_port_posix_attach(&port, &posix, "/dev/ttyS2", 115200);
    car_link_init(&g_link, &port);    /* 打不开也不崩，UI 照常起来 */

    car_ui_init(lv_screen_active(), &g_link);
    /* ... lv_timer_handler 循环 ... */
}
```

`car_link_init()` 返回 false 表示串口没打开，此时 link 进入"未运行"状态，pump 是空操作，store 里是全零默认值。

## 4.3 LVGL 的两个定时器

```c
static car_link_t *s_link;
static car_state_view_t s_view;

/* 数据泵：10ms，只做收发与解析 */
static void pump_timer(lv_timer_t *t)
{
    (void)t;
    car_link_pump(s_link, 0);
}

/* UI 刷新：33ms，只画变了的部分 */
static void ui_timer(lv_timer_t *t)
{
    (void)t;
    car_link_view(s_link, &s_view);

    if(s_view.changed & CAR_CH_SPEED_RPM) dashboard_on_speed(&s_view.state);
    if(s_view.changed & CAR_CH_FUEL)      dashboard_on_fuel(&s_view.state);
    if(s_view.changed & CAR_CH_GEAR)      dashboard_on_gear(&s_view.state);
    if(s_view.changed & CAR_CH_DOORS)     dashboard_on_doors(&s_view.state);
    if(s_view.changed & CAR_CH_WARNINGS)  dashboard_on_warnings(&s_view.state);

    /* 按键是事件不是状态，每帧都要抽干 */
    car_key_event_t keys[8];
    size_t n = car_link_take_keys(s_link, keys, 8);
    for(size_t i = 0; i < n; ++i) ui_on_key(keys[i].code, keys[i].pressed);
}

void car_ui_init(lv_obj_t *root, car_link_t *link)
{
    s_link = link;
    dashboard_create(root);
    lv_timer_create(pump_timer, 10, NULL);
    lv_timer_create(ui_timer, 33, NULL);
}
```

要点：

- `changed` 只在这一帧为真，且 `car_link_view()` 会清空它。一个 UI 定时器里只 view 一次，然后分发给所有控件。
- `car_link_pump()` 的 timeout 给 0。给 20 会让 LVGL 真的阻塞等串口。

## 4.4 数据字段总表

`view.state` 就是全量车况。命令与字段的对应关系：

| 命令 | 字段 | 说明 / 需要换算的 |
|---|---|---|
| 0x04 | speed_rpm.speed_kph / .engine_rpm / .valid | 直接可用 |
| 0x06 | fuel.percent / .remaining_range_km / .icon_state / .port_left / .main_tank_l / .sub_tank_l | port_left = 加油口在左 |
| 0x15 | coolant.percent / .celsius / .state / .display | 解码器已算好 celsius = raw-50；display 区分 值/未知/隐藏/故障 |
| 0x33 | gear.text（char[4]）/ .visible / .blink / .reverse_gear / .four_present / .four_state | ASCII，可直接 %s |
| 0x35 | turn.left / .right / .high_beam_key | bool |
| 0x0B | doors.left_front / .right_front / .left_rear / .right_rear / .trunk / .hood | bool |
| 0x02 | warnings.* + car_state_warning_level() | 不要自己遍历数组 |
| 0x0A | tires.wheels[0..4].kpa / .state | 顺序 FL/FR/RL/RR/备胎；state 0 无数据 1 正常 2 偏低 3 偏高 |
| 0x32 | clock.year / .month / .day / .hour / .minute / .second / .hour24 | 已拆好 |
| 0x34 | acc.on | |
| 0x08 | mileage.odometer_raw / .trip_a_raw / .trip_b_raw / .trip_type | 原始值，系数需标定 |
| 0x0C/0x0D/0x0E | trip_stats.* / refuel.* / trip.* | avg_fuel_x10 要除以 10 |
| 0x11 | instant_fuel.visible / .value_x10 / .per_hour | 除以 10；per_hour 时单位是 L/h |
| 0x13 | cruise.set_speed_kph / .cruise_state / .acc_state / .limit_speed_kph | |
| 0x22 | radar.front[5] / .rear[5]（.color / .value）/ .display_on | color 0~3 |
| 0x38 | other_info.oil_pressure_pct / .voltage_x10 / .gearbox_temp_c / .oil_temp_c | 电压除以 10 |
| 0x39 | units.mile / .hour24 / .fahrenheit / .fuel_unit / .pressure_unit | 先看这个再决定显示单位 |
| 0x19 | drive_mode / .drive_mode_valid | 1~8 |
| 0x20 | key.code / .pressed | 推荐用 take_keys 消费边沿事件 |
| 0xEE | hud.* | |
| 0x14 | car_model.selected / .count / .entries[] | |
| 0x29/0x30/0x31 | version.wired / .main / .chip | char[48] |

### 三个必须自己处理的细节

1. **valid 标志**：每组都有。`view.has_data == false` 说明一条有效帧都没收到，别把 0 当真实值画出来。
2. **未标 x10 但仍是原始值的**：mileage.odometer_raw、other_info.outside_temp_raw、trip.distance_km 的单位系数取决于 MCU 标定（Qt 版通常里程 x0.1 km、车外温度有偏移）。建议在 UI 层统一放一个 unit_convert.h，别散落在各控件里：

```c
/* ui/unit_convert.h —— 系数按你的车标定，只改这里 */
#define ODO_SCALE     0.1f
static inline float ui_odometer_km(const car_state_t *s)
{ return (float)s->mileage.odometer_raw * ODO_SCALE; }
```

3. **changed 会被 view 清掉**：先存一份再分发。

## 4.5 典型控件写法

速度：

```c
static void dashboard_on_speed(const car_state_t *s)
{
    if(!s->speed_rpm.valid) return;      /* 没收到过就别画 */
    lv_label_set_text_fmt(s_speed_label, "%u",
                          (unsigned)s->speed_rpm.speed_kph);
}
```

转速弧（量程 0~8000 映射到 0~270 度）：

```c
int32_t rpm = (int32_t)s->speed_rpm.engine_rpm;
if(rpm > 8000) rpm = 8000;
lv_arc_set_value(s_rpm_arc, (rpm * 270) / 8000);
lv_obj_set_style_arc_color(s_rpm_arc,
    rpm > 6500 ? lv_color_hex(0xFF2D2D) : lv_color_hex(0x00C2FF),
    LV_PART_INDICATOR);
```

水温四态（display 字段就是为这个准备的）：

```c
switch(s->coolant.display) {
    case CAR_COOLANT_SHOW_VALUE:
        lv_label_set_text_fmt(s_temp_label, "%d°C", (int)s->coolant.celsius);
        lv_obj_clear_flag(s_temp_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case CAR_COOLANT_SHOW_HIDDEN:
    case CAR_COOLANT_SHOW_UNKNOWN:
        lv_obj_add_flag(s_temp_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case CAR_COOLANT_SHOW_ERROR:
        lv_label_set_text(s_temp_label, "--");
        break;
}
```

故障灯（一个函数搞定 256 个灯）：

```c
for(unsigned i = 0; i < WARNING_ICON_COUNT; ++i) {
    uint8_t code = warning_assets[i].code;
    uint8_t level = car_state_warning_level(s, code);   /* 统一入口 */

    switch(level) {
        case CAR_WARN_OFF:
            lv_obj_add_flag(warning_assets[i].img, LV_OBJ_FLAG_HIDDEN);
            break;
        case CAR_WARN_ON:
            lv_obj_clear_flag(warning_assets[i].img, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(warning_assets[i].img, LV_OPA_COVER, 0);
            break;
        case CAR_WARN_BLINK_FAST:
        case CAR_WARN_BLINK_SLOW:
            lv_obj_clear_flag(warning_assets[i].img, LV_OBJ_FLAG_HIDDEN);
            lv_anim_start(level == CAR_WARN_BLINK_FAST ? &blink_fast
                                                       : &blink_slow);
            break;
    }
}
```

## 4.6 下发命令

中间件已封装 22 个构造器：填结构体，入队，返回。

```c
car_cmd_t req;

if(car_cmd_set_time(&req, 2026, 11, 1, 18, 30))
    car_link_exec(s_link, &req);        /* 只入队，不阻塞 UI */

car_cmd_select_trip(&req, 1);           /* 1=ODO 2=TRIPA 3=TRIPB */
car_link_exec(s_link, &req);

car_cmd_set_light(&req, CAR_LIGHT_COMING_HOME, 15);
car_link_exec(s_link, &req);

car_hud_t hud = s_view.state.hud;
hud.show_nav = true;
car_cmd_set_hud(&req, &hud);
car_link_exec(s_link, &req);
```

`car_link_exec()` 返回 false 只有三种情况：链路没起来、载荷超 16 字节、队列满（16 条）。不要原地重试，队列满时重试会把 UI 拖死。

## 4.7 加协议的四种情形

### 情形 A：全新 cmd_id（以 0x42 氛围灯、4 字节为例）

**第 1 步：加状态字段（car_state.h）**

(a) 第 65 行（`CAR_CH_FUNC_CONFIG` 之后）加变更位，当前用到位 35：

```c
#define CAR_CH_AMBIENT (UINT64_C(1) << 36)
```

(b) 第 418 行（`} car_func_config_t;` 之后）加结构体：

```c
typedef struct {
    bool on;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool valid;
} car_ambient_t;
```

(c) 第 459 行（`car_func_config_t func_config;` 之后）加成员：

```c
    car_ambient_t ambient;
```

**第 2 步：声明解码器（car_decode_internal.h，第 52 行之后）**

```c
bool car_decode_ambient(const uint8_t *data, uint16_t len, car_state_t *state);
```

**第 3 步：实现解码器（car_decode.c 末尾）**

```c
bool car_decode_ambient(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 4) {     /* 先校验 */
        return false;
    }

    state->ambient.on = data[0] != 0;  /* 再写；失败时一个字节都不写 */
    state->ambient.r = data[1];
    state->ambient.g = data[2];
    state->ambient.b = data[3];
    state->ambient.valid = true;
    return true;
}
```

**第 4 步：表里插一行（car_registry.c，0x41 与 0xEE 之间）**

```c
    {0x42, 4, 4, CAR_CH_AMBIENT, car_decode_ambient, "Ambient light"},
```

**第 5 步：验收**

```bash
make test
```

表插错位置、mask 非 0 却忘了挂解码器、min > max，都会被 test_registry.c 抓出来。

**第 6 步：加测试用例（test_decode.c 末尾）**

```c
{
    const uint8_t data[4] = {1, 0xFF, 0x80, 0x00};

    car_state_init(&st);
    CHECK(car_decode_ambient(data, 4, &st));
    CHECK(st.ambient.on);
    CHECK(st.ambient.r == 0xFF);
    CHECK(!car_decode_ambient(data, 3, &st));
}
```

**第 7 步：上机验证**

可直接粘进串口助手（HEX 发送）的字节：

```
2E 42 C0 04 01 FF 80 00 79
```

拆开：2E 帧头 / 42 命令 / C0 = op 0xC<<4 | 长度高位 0 / 04 长度 / 4 字节载荷 / 79 校验 = (0x42+0xC0+0x04+0x01+0xFF+0x80+0x00)^0xFF。

预期：仪表回一个 FF（ACK），状态里 ambient.r 变成 0xFF。

**第 8 步：接进 UI**

```c
if(view.changed & CAR_CH_AMBIENT) {
    lv_obj_set_style_bg_color(ambient_bar,
        lv_color_make(view.state.ambient.r, view.state.ambient.g,
                      view.state.ambient.b), 0);
}
```

漏掉这一步不会报错，只是"数据到了但界面不动"。

### 情形 B：已有命令加字段（0x38 从 6 到 7 字节）

改 2 处，但有个坑：旧解码器写的是 `len != 6`，新帧会被整帧拒掉。

表里放宽长度：

```c
{0x38, 6, 7, CAR_CH_OTHER_INFO, car_decode_other_info, "Other vehicle info"},
```

解码器改成"最少 6、有 7 才取新字段"：

```c
if(data == NULL || len < 6) {
    return false;
}
/* ...原有 6 个字段... */
state->other_info.new_field = (len >= 7) ? data[6] : 0;
```

规则：解码器的 len 条件必须和表里的 min/max 完全对应。表说 6~7，解码器就不能写 `len == 7`，否则 MCU 发 6 字节时会：通过先验校验 → 被解码器拒 → 回 NACK → MCU 重发 → 死循环。

### 情形 C：子命令（0x16 下加一项）

表完全不用动（0x16 已经是 1~64），只改 car_decode_high_func 里的分支链：

```c
if(data[0] == 0x04U && len >= 2) {
    state->high_func.drive_mode = data[1];
}
else if(data[0] == 0x42U && len >= 3) {
    state->high_func.sub42 = rd_u16(data + 1);
}
else if(data[0] == 0x3FU && len >= 11) {
    ...
}
```

子命令的坑：每个分支都要自己检查长度，父命令的 min 只能兜 1 字节。

### 情形 D：新增下发命令

car_command.h 末尾声明，car_command.c 末尾实现：

```c
bool car_cmd_set_ambient(car_cmd_t *req, bool on, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t payload[4];

    payload[0] = on ? 1U : 0U;
    payload[1] = r;
    payload[2] = g;
    payload[3] = b;

    return car_cmd_set_payload(req, 0x42, (uint8_t)CAR_OP_SET_GET, payload, 4);
}
```

如果这条命令只有下发、没有上报，表里仍然要登记（否则 MCU 回来的应答会被当成未登记命令回 NACK）：

```c
{0x42, 1, 16, 0, NULL, "Ambient light"},
```

参照现有的 0x3A / 0x3B / 0x3D。

### 检查清单

| # | 检查项 | 谁保证 |
|---|---|---|
| 1 | 表按 cmd_id 升序 | test_registry 自动 |
| 2 | mask 非 0 则 decode 非 NULL | test_registry 自动 |
| 3 | 解码器的 len 条件等于表里的 min/max | 只有人能保证 |
| 4 | mask 位没和别的组重复 | 人工 grep |
| 5 | UI 里有对应 changed 分支 | 人工 |

第 3 条是唯一真正危险的地方。自查方法：

```bash
make test CFLAGS="-O1 -g -fsanitize=address"
```

`test_registry` 会用最大长度全零载荷把所有解码器跑一遍，配合 ASan 能抓出越界读。

## 4.8 调试手段

```c
/* 1. 握手走到哪一步：init -> bulk -> ready */
printf("%s\n", car_link_phase_name(&g_link));

/* 2. 最近 16 帧原始记录（含被 NACK 的） */
car_trace_entry_t t[16];
size_t n = car_link_take_trace(&g_link, t, 16);

/* 3. 统计：收了多少、错了多少、谁在 NACK */
const car_link_stats_t *st = car_link_stats(&g_link);

/* 4. 解析器内部：噪声字节 / 长度拒收 / 重同步次数 */
g_link.parser.stats
```

排查"仪表不动"的标准顺序：phase 是不是 ready → frames_rx 涨不涨 → checksum/rejected 涨说明协议对不上 → noise_bytes 疯涨说明波特率或线序有问题。

gdb 抓调用链（三个断点）：

```bash
make CFLAGS="-O0 -g"          # Makefile 里 CFLAGS 是 ?= ，可直接覆盖
gdb ./bin/carmw_demo

(gdb) break link_on_frame
(gdb) commands
> silent
> printf "FRAME  cmd=%02X op=%X ok=%d len=%u\n", cmd_id, op, checksum_ok, len
> continue
> end

(gdb) break car_decode_speed_rpm
(gdb) commands
> silent
> printf "DECODE speed_rpm len=%u\n", len
> continue
> end

(gdb) break car_state_store_apply
(gdb) commands
> silent
> printf "COMMIT mask=0x%llx ver=%u\n", mask, store->version
> continue
> end
```

backtrace 会看到：car_decode_speed_rpm <- link_on_frame <- parser_drain <- car_parser_feed <- car_link_pump <- pump_timer。

---

# 5. 目标平台：T113-S3 + LVDS

## 5.1 性能账：中间件不是瓶颈

T113-S3 是双核 Cortex-A7 @1.2GHz + 128MB DDR3，无 3D GPU，LVGL 全靠 CPU 软件渲染。

| 环节 | 量的级 | 折算到 1.2GHz 单核 |
|---|---|---|
| UART 115200 收数据 | 11.5 KB/s，每秒约 1280 帧（9 字节/帧） | — |
| 拆帧 + 校验 + 解码 一帧 | 约 150~300 cycles | 0.03% |
| car_state_store_apply（1KB 拷贝 + 版本号） | 约 300 cycles x 20 次/秒 | 0.0005% |
| car_link_view（1KB 拷贝，UI 侧 30fps） | 约 300 cycles x 30 次/秒 | 0.0008% |
| 合计 | | 不超过 0.05% 单核 |

就算上到 921600 波特率（8 倍），也才 0.4%。

而 LVGL 画一帧：

| 场景 | 单帧耗时（A7@1.2G 软件渲染量级） | 30fps 下 CPU |
|---|---|---|
| 800x480 全屏重绘，含文字 + 弧形 | 5~20 ms | 15%~60% |
| 只重绘变化的小区域（数字、指针） | 0.2~1 ms | 1%~3% |

解析比渲染便宜 3 个数量级。把 0.03% 的工作搬到第二个核，省下的时间在测量噪声里都看不见，反而多出跨核同步、cache 抖动、调度延迟。

## 5.2 多线程该不该用

结论：不需要，而且开了也不会更快。

| 候选任务 | 该不该放第二个核 |
|---|---|
| 串口解析 | 不该，量太小 |
| SD 卡/eMMC 读图、字体加载 | 该，有真实阻塞 I/O |
| 日志写文件 / 音视频解码 | 该 |
| UI 渲染 | 不该，LVGL 单线程拆不开 |
| 空闲时降频省电 | 该，让 ondemand 自己降 |

值得开线程的 4 条判据（满足任意一条）：

1. 串口 I/O 会阻塞（用了阻塞 read，或挂在半双工总线上）。当前 port 是非阻塞 + poll，不成立。
2. 波特率 >= 460800 且帧长很大（4KB 升级帧、图像传输）。
3. 同时有别的阻塞源（写 SD 卡日志、读资源）。这时该起线程的是那些活，不是串口。
4. 要在等串口的同时跑严格周期的控制环。仪表盘没有这种需求。

真要开线程时的绑核：

```c
car_link_start(&g_link);        /* 起 1 个线程跑 pump(20) */

cpu_set_t set;
CPU_ZERO(&set);
CPU_SET(1, &set);
pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
```

或者 shell 里 `taskset -c 1 ./dashboard`。

三个注意点：别给 pump 线程 SCHED_FIFO 高优先级（会抢渲染线程导致掉帧）；start 之后 UI 只能通过 car_link_view 拿数据；线程模式下 stats 是无锁读、changed 是单读者语义。

## 5.3 LVDS 显示路径

LVDS 是连续扫描输出接口：DE 以像素时钟不停从 framebuffer 读数据发给面板。这决定了三件事——必须整帧 FB、必须双缓冲、必须和 vsync 对齐。这三件事恰好都是多线程的敌人。

### 带宽账

| 分辨率 | 格式 | 单帧 | 60Hz 扫描带宽 | 加上 CPU 渲染写入 |
|---|---|---|---|---|
| 1024x600 | RGB565 | 1.23 MB | 74 MB/s | 约 148 MB/s |
| 1024x600 | ARGB8888 | 2.46 MB | 148 MB/s | 约 295 MB/s |
| 1280x800 | RGB565 | 2.05 MB | 123 MB/s | 约 246 MB/s |

T113-S3 是 16-bit DDR3，实测可用带宽约 600~900 MB/s。结论：RGB565 是正确选择（带宽占 1/4，A7 填充率也几乎翻倍）。

### 三种方案

| | 方案 A：单 FB + DIRECT | 方案 B：双 FB + DIRECT + pan | 方案 C：小 buffer + memcpy |
|---|---|---|---|
| LVGL 模式 | DIRECT | DIRECT | PARTIAL |
| 撕裂 | 有 | 无 | 有 |
| 额外拷贝 | 无 | 无 | 每帧整块 memcpy |
| 内存 | 1 帧 | 2 帧 | 1 帧 + 小 buffer |
| LVDS 适用 | 勉强 | 正确解 | 那是 SPI 屏的做法 |

方案 C 是网上 SPI 屏教程的标配，放到 LVDS 上等于每帧多拷 1~2 MB，还照样撕裂。

### 方案 B 的骨架（fbdev 路径，LVGL 9）

```c
static int fb_fd;
static uint8_t *fb_mem;
static struct fb_var_screeninfo vinfo;
static size_t screen_bytes;

fb_fd = open("/dev/fb0", O_RDWR);

ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
vinfo.yres_virtual = vinfo.yres * 2;          /* 两块 FB 竖着排 */
if(ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
    perror("no pan support");                  /* 见 5.4 兜底 */
}

screen_bytes = (size_t)vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);
fb_mem = mmap(NULL, screen_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED,
              fb_fd, 0);

lv_display_t *disp = lv_display_create((int32_t)vinfo.xres,
                                       (int32_t)vinfo.yres);
lv_display_set_color_format(disp,
    vinfo.bits_per_pixel == 16 ? LV_COLOR_FORMAT_RGB565
                               : LV_COLOR_FORMAT_XRGB8888);
lv_display_set_buffers(disp,
                       fb_mem,                    /* buf1 */
                       fb_mem + screen_bytes,     /* buf2 */
                       screen_bytes,              /* DIRECT 必须整屏 */
                       LV_DISPLAY_RENDER_MODE_DIRECT);
lv_display_set_flush_cb(disp, flush_cb);

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    static int shown;

    int idx = (px_map == fb_mem) ? 0 : 1;

    if(idx != shown) {
        struct fb_var_screeninfo v = vinfo;
        v.yoffset = (uint32_t)idx * vinfo.yres;
        ioctl(fb_fd, FBIOPAN_DISPLAY, &v);
        shown = idx;
    }

    int zero = 0;
    ioctl(fb_fd, FBIO_WAITFORVSYNC, &zero);   /* 阻塞到下一个 vblank */

    lv_display_flush_ready(d);
}
```

`FBIO_WAITFORVSYNC` 会阻塞最多 16.6ms，看起来是浪费，但它做的是把帧率锁在 60fps 并保证无撕裂。性能目标不是无限帧率，而是：每帧渲染时间 < 16.6ms，最好 < 8ms。

### 两个真正的坑

**坑 1：cache 一致性。** DE 是 DMA 主设备，CPU 写 FB 后必须让 cache 落盘，否则出现残影/花屏/指针拖尾。好消息：sunxi 的 fbdev 和 DRM 的 dumb buffer 都走 `dma_mmap_wc`，映射是 write-combine 而非 cached，所以不需要手动 clean cache。代价是：写 FB 性能介于 cached 与 uncached 之间（可接受），读 FB 非常慢（所以少用半透明叠层）。

如果你自己 mmap 成了 cached 映射，必须 kernel 侧做 `dma_sync_single_for_device`，否则一定花屏。确认方法：

```bash
cat /sys/class/graphics/fb0/* 2>/dev/null
grep -i "fb\|disp" /proc/iomem | head
```

**坑 2：pan 不支持。** 有些 BSP 的 `FBIOPUT_VSCREENINFO` 会拒绝 `yres_virtual = 2*yres`。两个选择：改走 DRM/KMS（`drmModePageFlip` + `DRM_MODE_PAGE_FLIP_EVENT`）；或退回单 FB，靠"渲染时间 < 16.6ms"硬扛，会有轻微撕裂。

## 5.4 配置清单

```c
/* lv_conf.h */
#define LV_COLOR_DEPTH            16        /* RGB565，带宽减半 */
#define LV_USE_DRAW_SW_ASM        LV_DRAW_SW_ASM_NEON
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0
#define LV_USE_SHADOW             0
#define LV_USE_BLUR               0
#define LV_DEF_REFR_PERIOD        16        /* 约 60fps */
#define LV_USE_PERF_MONITOR       1         /* 先开着量，上线关掉 */
```

```
编译：-O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
内存：两块 1024x600x2B = 2.46MB，确认 CMA / sunxi-disp reserved 区够（建议 >=8MB）
```

最大的杠杆是"别全屏重绘"：把静态表盘做成一张 lv_image 铺底，只有数字、指针、警告灯这些小块在动。1024x600 全屏重绘约 10ms；只画指针和数字约 0.5~1ms。

自测方法（每帧打一次，跑 10 秒看分布）：

```c
static void flush_cb(...)
{
    static uint32_t t_prev, n, acc, mx;
    uint32_t now = lv_tick_get();
    uint32_t dt = now - t_prev;
    t_prev = now;
    acc += dt;
    if(dt > mx) mx = dt;
    if(++n % 60 == 0) {
        printf("frame: avg %u ms, max %u ms\n", acc / 60, mx);
        acc = 0; mx = 0;
    }
}
```

判断：avg 约 16ms 说明被 vsync 锁住、渲染有余量；avg > 16.6ms 且 max 抖动大说明有东西在抢 CPU（多半是全屏 invalidate 或大块半透明图层）。

---

# 6. 串口收发模型

## 6.1 结论

| 层面 | 是不是同时 |
|---|---|
| UART 硬件 | 全双工，TX/RX 是两根独立的线 |
| 一次 car_link_pump 内部 | 先发后收，严格顺序执行，不是两个线程同时读写 |
| 读循环内部 | 会夹着写（每解出一帧就立刻回 ACK），同一轮里读-写-读-写交替 |
| 整个进程 | 只有一个线程在碰串口（协作模式） |

准确说法：帧粒度的读写交替，字节粒度的收发由硬件并行。

## 6.2 一次 pump 的精确时序

```
t0  car_link_pump                              car_link.c:282
t1  |- car_session_next 循环 -> link_send_frame  car_link.c:292~297   写（0~N 条握手命令）
t2  |- link_drain_tx                           car_link.c:300          写（0~16 条用户命令）
t3  |- read(buf, 256, timeout)                 car_link.c:310          读
t4  |  |- car_parser_feed                      car_link.c:322
t5  |     |- link_on_frame                     car_link.c:140
t6  |        |- desc->decode -> 写 pending      car_link.c:173
t7  |        |- link_reply(0xFF) -> write()    car_link.c:194          读循环里夹一次写
t8  |- read() 再来一轮（直到 EAGAIN）           car_link.c:310
t9  |- link_commit -> store_apply              car_link.c:329
```

关键在 t7：link_on_frame 是在 read 循环内部被回调的，而它自己会 write(ACK)。所以一轮 pump 里读写按帧粒度交替，全程顺序执行、没有并发。

## 6.3 缓冲余量

"正在写的时候对方发数据"不会丢，内核在兜：

```
MCU --TX--> UART FIFO --> 内核 tty flip buffer（典型 4KB） --> 你的 read()
                                  ^
                          你在这期间写 ACK，不影响它继续攒
```

| 参数 | 值 |
|---|---|
| 115200 8N1 | 11,520 B/s |
| 内核缓冲（保守估） | 4 KB |
| 可容忍的 pump 间隔 | 约 355 ms |

10ms 定时器有 35 倍余量。就算 TX 队列满（16 条 x 21 字节 = 336 字节，约 29ms 才发完），这 29ms 不读也毫无压力。

真正会溢出的唯一场景：LVGL 卡住超过 355ms（加载大图、从 SD 卡读资源、死循环）。表现是帧数莫名变少，或校验错暴增（丢字节导致错位重同步）。

## 6.4 三种会真出问题的情况

**1. MCU 链路是 RS-485（半双工）。** 那就不能同时收发，需要 DE/RE 方向控制引脚，而现在的 car_port_t 里没有这个接口。判断方法：看硬件上 MCU 那一路是 TTL 直连还是过了一颗 485 收发器（MAX485/SP3485 之类）。如果是 485，port 要加：

```c
typedef struct {
    ...
    void (*set_direction)(void *ctx, bool tx);   /* 485 收发切换 */
} car_port_t;
```

然后 posix_write 前后切换并 tcdrain()。

**2. MCU 要求严格的应答窗口。** 如果协议规定"MCU 发完一帧后 X 微秒内屏不得发送"，现在"收到就立刻回 ACK"可能撞上。要么在 link_reply 前插延时，要么把 ACK 挪到下一个 pump 周期。

**3. 流控。** 代码里 CRTSCTS 和 IXON 都是关着的（标准 raw 模式）。如果 MCU 缓冲区小、需要 XON/XOFF，得自己开；仪表盘这种小帧低频场景不需要。

## 6.5 想改成真并发：三种方案

**方案 1：先收后发（1 行改动，推荐）。** 把 link_drain_tx 从 car_link.c:300 挪到读循环之后。接收不再被发送阻塞，命令延迟最多 +1 个 pump 周期（10ms）。这是唯一零风险且有收益的改动。

**方案 2：TX 独立线程。** 现有 tx 队列已存在，加一个专门写串口的线程。代价：posix_write 必须串行化（要一把写锁）；关闭顺序要小心（先停 TX 线程再 close fd）；多一个线程。

**方案 3：poll(POLLIN|POLLOUT) 单线程事件驱动。** 一次 poll 同时等可读和可写，谁就绪处理谁，真正的单线程全双工。代价是破坏当前 port 抽象（要暴露 fd 或新增 wait_events 接口），car_link 里也要重写成状态机。

结论：默认别改。唯一必须确认的是链路是不是 485。

---

# 7. 已知问题与待办

## 7.1 转速上限改动导致两个测试失败（高优先级）

car_decode.c 第 102 行当前是 `if(rpm > 10000U)`，但测试仍按 9001 造越界帧：

| 文件 | 行 | 现在会怎样 |
|---|---|---|
| tests/test_decode.c | 21 | 9001 <= 10000，decode 成功，`CHECK(!...)` 失败 |
| tests/test_link.c | 91 | 该帧被解收，frames_rejected 变 0、acks +1、nacks -1，末尾 4 个统计 CHECK 全挂 |

改成 10001（即 0x2711）：

```c
/* test_decode.c:21 */
const uint8_t bad[4] = {0x00, 0x78, 0x27, 0x11}; /* 10001 rpm */

/* test_link.c:89, 91 */
/* ---- 5. 长度合法但内容非法（转速 10001）：回 0xF3 ---- */
const uint8_t payload[4] = {0x00, 0x10, 0x27, 0x11}; /* 10001 rpm */
```

或者把阈值改回 9000。但先确认 MCU 侧真实上限：如果这是柴油车，红线约 4500，9000 已足够宽松；10000 会把真正的错帧（比如高位错位）放进来。

## 7.2 尚未经过编译器验证

本文档所述全部代码，在本机未编译、未运行过测试（开发环境 shell 无法启动，错误码 0xC0000142）。所有结论来自静态推演与逐字节核对。

首次在 Linux/WSL 上执行时，建议顺序：

```bash
cd lvgl_two
make CFLAGS="-O1 -g -Wall -Wextra -Wpedantic -Wshadow"
make test
make demo
```

预期：demo 打印 4 秒车况，phase 从 init 走到 ready，统计里 acks > 0、nacks = 0、resyncs = 0。

## 7.3 stats 无锁读

car_link_stats() 返回裸指针，多线程模式下与 pump 线程的计数自增构成 C11 意义上的 data race。x86-64 上不会撕裂，纯诊断用途。若需严格，改为加锁拷贝接口 `car_link_stats_copy(link, &out)`。

## 7.4 待确认参数

| 待确认 | 影响 |
|---|---|
| MCU 链路是 TTL UART 还是 RS-485 | 485 需要加 set_direction |
| 内核版本（uname -r） | 决定 fbdev 还是 DRM 路径 |
| LVGL 版本（8.3 还是 9.x） | 决定双缓冲 API 写法 |
| 屏幕分辨率与 LVDS 通道数 | 决定带宽与缓冲大小 |
| fb0 的 virtual_size 是否支持双倍高度 | 决定能否用 pan |
| 里程/车外温度的真实换算系数 | UI 显示数值是否正确 |

---

# 8. 附录

## 附录 A：关键函数行号索引

| 功能 | 文件 | 行 |
|---|---|---|
| 主循环泵 | src/car_link.c | 282 |
| 收帧回调（唯一分发点） | src/car_link.c | 140 |
| 提交快照 | src/car_link.c | 203 |
| 发送队列出队 | src/car_link.c | 216 |
| 初始化 | src/car_link.c | 240 |
| 取状态快照 | src/car_link.c | 417 |
| 取按键事件 | src/car_link.c | 426 |
| 取诊断记录 | src/car_link.c | 446 |
| 起/停线程 | src/car_link.c | 345 / 370 |
| 下发命令入队 | src/car_link.c | 385 |
| 喂字节 | src/car_parser.c | 188 |
| 拆帧主循环 | src/car_parser.c | 87 |
| 先验校验 | src/car_parser.c | 70 |
| 组帧 | src/car_protocol.c | 22 |
| 校验和 | src/car_protocol.c | 5 |
| 单帧解析（调试用） | src/car_protocol.c | 56 |
| 命令表 | src/car_registry.c | 9-57 |
| 命令查找（二分） | src/car_registry.c | 75 |
| 长度接受判断 | src/car_registry.c | 105 |
| 车速解码 | src/car_decode.c | 95 |
| 快照 apply | src/car_state.c | 97 |
| 快照 view | src/car_state.c | 118 |
| 故障灯取值 | src/car_state.c | 16 |
| 握手收帧 | src/car_session.c | 93 |
| 握手取下一命令 | src/car_session.c | 153 |
| 串口打开（O_NONBLOCK） | src/port/car_port_posix.c | 96 |
| 串口读 | src/port/car_port_posix.c | 127 |
| 串口写（含 EAGAIN 重试） | src/port/car_port_posix.c | 171 |

## 附录 B：47 条命令表

| cmd | min | max | 变更掩码 | 解码器 |
|---|---|---|---|---|
| 0x00 | 1 | 1 | CAR_CH_INIT | car_decode_init |
| 0x01 | 1 | 4095 | 0 | NULL |
| 0x02 | 1 | 4095 | CAR_CH_WARNINGS | car_decode_warnings |
| 0x03 | 4 | 4 | CAR_CH_POPUP | car_decode_popup |
| 0x04 | 4 | 4 | CAR_CH_SPEED_RPM | car_decode_speed_rpm |
| 0x05 | 3 | 64 | CAR_CH_SYS_SETTING | car_decode_sys_setting |
| 0x06 | 6 | 64 | CAR_CH_FUEL | car_decode_fuel |
| 0x07 | 1 | 4095 | 0 | NULL |
| 0x08 | 10 | 10 | CAR_CH_MILEAGE | car_decode_mileage |
| 0x0A | 10 | 10 | CAR_CH_TIRE | car_decode_tires |
| 0x0B | 1 | 1 | CAR_CH_DOORS | car_decode_doors |
| 0x0C | 10 | 4095 | CAR_CH_TRIP_STATS | car_decode_trip_stats |
| 0x0D | 6 | 64 | CAR_CH_REFUEL | car_decode_refuel |
| 0x0E | 8 | 64 | CAR_CH_TRIP | car_decode_trip |
| 0x0F | 2 | 64 | CAR_CH_ECO | car_decode_eco |
| 0x11 | 2 | 2 | CAR_CH_INSTANT_FUEL | car_decode_instant_fuel |
| 0x12 | 13 | 13 | CAR_CH_IDLE_STOP | car_decode_idle_stop |
| 0x13 | 5 | 64 | CAR_CH_CRUISE | car_decode_cruise |
| 0x14 | 2 | 4095 | CAR_CH_CAR_MODEL | car_decode_car_model |
| 0x15 | 3 | 3 | CAR_CH_COOLANT | car_decode_coolant |
| 0x16 | 1 | 64 | CAR_CH_HIGH_FUNC | car_decode_high_func |
| 0x17 | 1 | 1 | CAR_CH_WARN_TONE | car_decode_warn_tone |
| 0x18 | 6 | 6 | CAR_CH_BACKLIGHT | car_decode_backlight |
| 0x19 | 1 | 4 | CAR_CH_DRIVE_MODE | car_decode_drive_mode |
| 0x20 | 2 | 64 | CAR_CH_KEY | car_decode_key |
| 0x22 | 11 | 11 | CAR_CH_RADAR | car_decode_radar |
| 0x26 | 2 | 64 | CAR_CH_TRAJECTORY | car_decode_trajectory |
| 0x29 | 1 | 64 | CAR_CH_VERSION | car_decode_version_wired |
| 0x30 | 1 | 64 | CAR_CH_VERSION | car_decode_version_main |
| 0x31 | 1 | 64 | CAR_CH_VERSION | car_decode_version_chip |
| 0x32 | 6 | 64 | CAR_CH_CLOCK | car_decode_clock |
| 0x33 | 5 | 64 | CAR_CH_GEAR | car_decode_gear |
| 0x34 | 1 | 1 | CAR_CH_ACC | car_decode_acc |
| 0x35 | 1 | 1 | CAR_CH_TURN | car_decode_turn |
| 0x36 | 8 | 8 | CAR_CH_MAINTENANCE | car_decode_maintenance |
| 0x37 | 2 | 2 | CAR_CH_SPEED_WARN | car_decode_speed_warning |
| 0x38 | 6 | 6 | CAR_CH_OTHER_INFO | car_decode_other_info |
| 0x39 | 3 | 64 | CAR_CH_UNITS | car_decode_units |
| 0x3A | 1 | 16 | 0 | NULL |
| 0x3B | 2 | 16 | 0 | NULL |
| 0x3D | 1 | 1 | 0 | NULL |
| 0x40 | 1 | 4095 | 0 | NULL |
| 0x41 | 1 | 4095 | 0 | NULL |
| 0xEE | 5 | 64 | CAR_CH_HUD | car_decode_hud |
| 0xFD | 1 | 64 | CAR_CH_FUNC_CONFIG | car_decode_func_config |
| 0xFE | 1 | 64 | CAR_CH_VERSION | car_decode_version_debug |
| 0xFF | 1 | 1 | 0 | NULL |

未登记命令：长度 <= 256 时放行（CAR_UNKNOWN_MAX_PAYLOAD），> 256 视为噪声。

应答字节：0xFF 成功，0xF0 校验错，0xF3 命令/长度/内容非法，0xFC 资源不足（保留）。

## 附录 C：术语

| 术语 | 含义 |
|---|---|
| 帧 | 0x2E 开头、含 cmd/op/len/payload/checksum 的完整报文 |
| 载荷 payload | 帧内 data 部分，长度 0~4095 |
| 先验校验 | 读到 ctrl/len 后、收全帧前就判断 (cmd,len) 是否可能存在 |
| 重同步 | 校验失败后前移 1 字节重新找帧头 |
| 变更掩码 mask | 64 位，每位代表一组状态是否被本帧更新 |
| pending | 写者私有的累积状态，submit 前的暂存区 |
| 快照 view | UI 拿到的状态副本，含 changed 与 version |
| 协作式 pump | 在 UI 定时器里调用，不创建线程 |
| 陷阱（tearing） | DE 正在扫描的 buffer 被改写导致的画面撕裂 |

---

**文档结束**

整理：基于 lvgl_two 源码（2026-11-02 状态）与开发对话。
源码位置：`C:\Users\Administrator\Desktop\lvgl\lvgl_two`
同目录另有 Markdown 版本便于版本管理。
