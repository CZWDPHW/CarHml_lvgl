# UI 复刻任务交接文档

> 目的：把 Qt 项目 `CarHml_XIAO` 的仪表界面复刻到当前 LVGL 工程，并保证**样式可切换**。
> 本文档供后续新任务接手使用，记录已完成进度、架构约定、踩过的坑和剩余工作。
> 2026-11-04 更新：接手者先读第10～13节的最新基线、详细复刻/测试方案和可复制任务指令；历史“完成”标记不代表视觉或板端验收通过。

---

## 1. 任务来源与目标

- 参考实现：`/home/xfy/CarHml_XIAO`（Qt/QML 仪表）
- 改造目标：`/mnt/hgfs/share/lvgl9.5demo/lvgl_two`（LVGL 9.5 + carmw 串口协议中间件）
- 实施策略：**在当前工程内独立重写**，Qt 工程只作参考，不搬运其代码
- 另一参考：`/mnt/hgfs/share/lvgl9.5demo/dashboard_t113`，仅用于协议语义与数据规则，不引入代码

### 已确认的决策

| 项 | 结论 |
|---|---|
| 复刻范围 | Style1 主仪表 + 中心卡片 + 覆盖层（六阶段）；设置体系/CarPlay/升级后置 |
| 目录与命名 | 镜像 Qt 的 QML 组件树（`PageModel/`、`SpeedDial/`、`RotationDial/` 等） |
| 样式可切换 | 一级架构约束。每个样式独立 `.c`，公共层禁止出现样式名判断 |
| 通用控件 | 所有样式共用的控件放 `ComControl/`，命名用 `ui_` 前缀，不带样式名 |

---

## 2. 当前架构

### 2.1 分层关系

```
main.c                     入口：初始化 carmw + ui_app + 刷新定时器
 └─ src/ui/ui_app.c        页面舞台：样式装载/卸载/切换，缓存最后一份车状态快照
     └─ ui_style_registry.c 样式注册表：id → ui_style_desc_t
         └─ PageModel/*.c   各样式顶层页（实现 ui_node_vtbl_t）
             ├─ SpeedDial/ RotationDial/ FuelTank/ WaterTemp/   样式专用组件
             ├─ ComControl/                                     跨样式通用复合控件
             └─ 直接摆放的静态元素（时钟、车外温度、中央车辆视图）
 └─ src/carmw/             串口协议中间件（parser / decode / state / session / command）
```

数据来源统一为 `car_state_view_t`（含 `car_state_t` + `has_data`）。

### 2.2 组件契约（对应 QML Item）

`src/ui/ui_node.h` 定义统一接口，所有页面实现：

```c
typedef struct {
    size_t size;                                                    /* 私有结构大小 */
    bool (*create)(ui_node_t *self, lv_obj_t *parent);
    void (*destroy)(ui_node_t *self);
    void (*apply)(ui_node_t *self, const car_state_view_t *view);
    void (*on_show)(ui_node_t *self);
    void (*on_hide)(ui_node_t *self);
    void (*on_key)(ui_node_t *self, const ui_key_event_t *evt);
} ui_node_vtbl_t;
```

对应关系：QML `Item` → `ui_node_t`；`Loader` → `create/destroy`；属性绑定 → `apply` 显式调用。

### 2.3 目录现状

```
src/ui/
├── ui_app.c/.h              页面舞台与样式切换
├── ui_node.h                组件契约
├── ui_style.h               样式描述
├── ui_style_registry.c      样式注册表
├── ui_assets.c/.h           资源路径拼接 + 需变换图片的 RAM 加载
├── ui_theme.h               颜色/字号/通用容器（纯头文件）
├── ui_common.c/.h           页面创建（含 0.975 缩放）、资源宏
├── widgets/
│   ├── ui_dial.c/.h         表盘：底图 + 旋转指针 + 刻度文本
│   ├── ui_tick.c/.h         竖向刻度条：裁剪窗 + 反向偏移
│   └── ui_lamp.c/.h         警示灯：OFF / ON / BLINK / SCAN
├── ComControl/              ★ 跨样式通用复合控件
│   ├── ui_gear.c/.h         档位
│   ├── ui_mileage.c/.h      总里程 / 小计里程
│   ├── ui_range.c/.h        剩余续航
│   ├── ui_clock.c/.h        时钟（含冒号 1s 闪烁）
│   ├── ui_outside_temp.c/.h 车外温度
│   ├── ui_turn_signal.c/.h  左/右转向灯（1000ms 硬闪）
│   ├── ui_lamp_row.c/.h     横向灯排：故障码 → 灯态，按宽度顺序排布
│   ├── ui_lamp_group.c/.h   绝对坐标灯组：按样式灯的散点布局摆放
│   ├── ui_cruise.c/.h       定速巡航：图标 + 数值 + 单位
│   └── ui_small_door.c/.h   小车门图（故障码 22 时显示）
├── SpeedDial/speed_forester1.c/.h     车速表（样式专用）
├── RotationDial/rotate_forester1.c/.h 转速表（样式专用）
├── FuelTank/fuel_forester1.c/.h       油量表（样式专用）
├── WaterTemp/water_forester1.c/.h     水温表（样式专用）
└── PageModel/
    ├── page_model.h                   样式入口声明
    ├── page_subaru_forester1.c        Style1 主仪表
    └── page_general1.c                占位样式（验证切换用）
```

### 2.4 命名规则（重要）

| 前缀/后缀 | 含义 | 例 |
|---|---|---|
| `ui_` | 通用、可跨样式复用 | `ui_dial`、`ui_gear`、`ui_assets` |
| `_forester1` 等样式后缀 | 绑定某个样式的素材与坐标 | `speed_forester1`、`fuel_forester1` |
| `page_` | 样式顶层页，实现 `ui_node_vtbl_t` | `page_subaru_forester1` |

**公共层（`ui_app`、`widgets/`、`ComControl/`）不得出现样式名判断分支。**

### 2.5 样式切换怎么工作

- `ui_app_set_style(id)`：旧节点 `on_hide → destroy → lv_free`；按 `vtbl->size` 分配并 `create`
- `ui_app` 缓存 `s_last_view`，切换后立即补一次 `apply`，**不丢状态**
- 指针图片为文件级 static `lv_image_dsc_t`，`ui_assets_load_image` 幂等，跨切换不重复 malloc
- PC 端 `main.c` 有右上角临时按钮循环切换样式；板端不创建该按钮

---

## 3. 已完成进度

### 阶段 0：基础框架（已完成）

1. `ui_node.h` / `ui_style.h` / `ui_style_registry.c` / `ui_app.c/.h` / `PageModel/page_model.h`
2. 原 `dashboard_ui.c/.h` 迁移为 `PageModel/page_subaru_forester1.c` 并删除
3. `main.c` 改为走 `ui_app`，PC 加样式切换按钮
4. `ui_theme.h`：颜色、字号、`ui_blank_container`
5. `ui_assets.c/.h`：路径拼接 + 图片 RAM 加载
6. `widgets/ui_lamp`、`widgets/ui_dial`、`widgets/ui_tick`
7. `tools/png2bin.py`：PNG/JPG → `bin32`(ARGB8888) + `bin16`(RGB565A8)，原有 243 张已全部转换；`Door/Style1/` 另有 3 张单独转换

### 阶段 1 / 阶段 3：Style1 组件拆分与灯群（进行中）

| 组件 | 状态 |
|---|---|
| `SpeedDial/speed_forester1` | 已接入；公英制接线、指针、单位标签及高亮缓存未闭环，见 B1 |
| `RotationDial/rotate_forester1` | ✅ 完成（角度递减、8 刻度高亮、`0` 恒为大字号） |
| `widgets/ui_tick` + `FuelTank/fuel_forester1` | ✅ 完成（≤25% 换红刻度图，见 B2） |
| `WaterTemp/water_forester1` | ✅ 完成（HIGH/OVERHEAT 切高温图标并移位） |
| `ComControl/ui_gear` / `ui_mileage` / `ui_range` | ✅ 完成并已接入页面 |
| `ComControl/ui_clock` | ✅ 完成并已接入（`car_clock_t`，`hour24` 决定 12/24 小时制，冒号 1s 闪烁） |
| `ComControl/ui_outside_temp` | ✅ 完成并已接入（`outside_temp_raw` → `(raw-0x7C)×0.5 ℃`，含 0xFF/0xFE/0xFC 特殊值） |
| 灯群（转向灯、LampControl 各灯） | ✅ 顶部灯排 + 左表盘灯群 20 盏 + 右表盘灯群 12 盏 + 底部灯排 5 盏已完成（详见 3.1~3.6） |
| 中央车辆视图 | ✅ 4 帧轮播（150ms）+ 左右转向灯（500ms）+ 示宽灯已完成（见 3.7） |

#### 3.1 转向灯（阶段 3 步骤 1，已完成）

- 控件：`ComControl/ui_turn_signal`，1000ms 硬闪（切 `LV_OBJ_FLAG_HIDDEN`，不做透明度渐变）
- 页面坐标：左 (660,98)、右 (1206,98)，54×48；图标 `WarnIcon/left_lamp.bin` / `right_lamp.bin`
- 数据：`car_turn_t`，`valid` 时按 `left/right` 点亮
- 资源：原图 64×64，需缩放 → `ui_assets_load_image` 整图入 RAM + `ui_lamp_create_sized`

#### 3.2 顶部灯排（阶段 3 步骤 2，已完成）

- 控件：`ComControl/ui_lamp_row`，从 (720,92) 起按宽度顺序排布，行高 64，图标垂直居中
- 灯表项 `ui_lamp_row_item_t`：`{icon, icon_alt, code, alt_code, w, h}`；`alt_code` 有故障时切 `icon_alt`，两码同时存在取更高灯态。复用到其他灯群时只改这张表
- 灯表在样式层 `s_top_lamps`：自动远光 16/56、示廓 12/56、近光 15（故障 229 换 `low_beam_faulty`）/56、远光 9/56、前雾 11/40、后雾 10/40
- 故障码 → 灯态：0 灭、1 常亮、2 快闪、3 慢闪
- `ui_lamp.blink_ms` 是**完整亮灭周期**（定时器每半周期切一次可见性）：快闪 600ms、慢闪 6000ms
- 布局结果：6 盏灯占 720→1024
- PC 假帧 `0x02` 初始按 `16,12,15,229,9,11,10,22,31,99,239,240,241` 每 1s 轮换一个码（241 那步用快闪）；后续已扩展到 64 个码、0.5s 一换，见 3.6

#### 3.3 定速巡航（阶段 3 步骤 2B，已完成）

- 控件：`ComControl/ui_cruise`，位于 (1024,98)，52×52 图标 + 24 号数值 + 14 号单位，文字贴行底
- 图标：31/95 绿、99 ACC 绿、239 ACC 白、240 定速白、241 ACC ready；故障码消失后保留上次图标
- 显示：有故障码或 `set_speed_kph > 0` 时显示图标；数值/单位仅在 `set_speed_kph > 0` 时显示
- 单位：`units.mile` 时按 `round(kph × 0.621371)` 显示并换 `"mile/h"`，否则 `"km/h"`
- PC 假帧 `0x13` 每 2s 改一次设定速度（60~99），payload 为 `{text_id, cruise_state, set_speed, acc_state, limit}`

#### 3.4 小车门（阶段 3 步骤 2B，已完成）

- 控件：`ComControl/ui_small_door`，64×64 位于 (1152,92)
- 行为严格对齐 Qt 顶部 Row：仅故障码 22 时显示背景 + 左右前门三张图，其余时间隐藏
- 素材：`Door/Style1/small_door_{bg,left_front,right_front}.bin`（64×64，流式解码，不需 RAM）
- 注意：Qt 顶部 miniDoor **不读 0x0B 门状态**；完整门状态留给阶段 5 的覆盖层

#### 3.5 左表盘灯群（阶段 3 步骤 3，已完成）

- 新增通用控件 `ComControl/ui_lamp_group`：**绝对坐标**摆放，不依赖行/列。灯表项 `ui_lamp_group_item_t`：
  `{ variants[4], variant_count, x, y, w, h }`，其中 `ui_lamp_group_variant_t = {icon, code, code2}`。
  `code2` 用于两个故障码共用一张图（自动驻车 46/91）。
- 选图规则：遍历该灯的变体，取灯态最高的一档对应图标；同档保留表中靠前者；故障消失后保留最后一张图。
  闪烁周期与 `ui_lamp_row` 一致（快闪 600ms、慢闪 6000ms，均为完整周期）。
- 灯表在样式层 `page_subaru_forester1.c` 的 `s_left_lamp_items`（20 盏，顺序与 Qt 声明顺序一致以保证重叠灯 z 序）：
  制动液 5、手刹 23、自动驻车 46/91、胎压 24、210、216/228、防碰撞 107/108、207、
  ESC 故障 19、ESC 关闭 20、LAS 49/52+50+51、8、204、智能模式 232/233/234、EV 235、220、
  外灯 13/14、EPS 6、ABS 4、雷达 59/129/173。
- 坐标 = Qt 速度表原点 (166,79) + 子项局部坐标；水平居中的按 `(620-宽)/2` 折算。
  灯组创建在 `bg_01` **之前**，保持 Qt 的 z 序（中央背景 z=1 压在 z=0 的灯群之上）。
- `s_left_lamps` 用文件级 `static`：样式切换重建页面时复用已加载的图片 RAM，避免重复 malloc。
- 素材：`WarnIcon/tire_faulty.svg` 原资源管线未转换，已用 Rsvg 栅格化为 48×48 PNG 并生成 `tire_faulty.bin`；
  其余图标 bin 均沿用现有资源。灯组尺寸与源图不一致，全部走 `ui_assets_load_image` 整图入 RAM 后缩放。
- 已知边界：Qt ABS 的 `289`（EBD）分支在协议层无法表示（`warnings.list[]` 为 `uint8_t`），当前只接码 4。
- 未包含：`IconDrivingMode`（驱动模式文字）与 `IconOverSpeed`（超速圆圈）不是故障码驱动的灯，留到阶段 4/6。
- PC 假帧 `0x02` 码表已扩展到 43 个码、每 0.5s 换一个（约 21s 一轮），追加了左灯群全部主/备图标码。
  注意 3.2 中「每 1s」已被本次改为 0.5s。

#### 3.6 右表盘灯群 + 底部灯排（阶段 3 步骤 4，已完成）

- 两者都复用 `ui_lamp_group`，只在样式层新增灯表，公共控件未改动。
- 右表盘灯群 `s_right_lamp_items`（12 盏，Qt 原点 1134,79 + 子项坐标，顺序同 Qt 声明顺序）：
  机油液位低 206、机油压力 25、READY 139/140/241、AWD 203、四驱故障 36、四驱打开 33、
  hole 238、行人接近 236/237、AVH 工作 222、主驾安全带 17、副驾安全带 18、气囊 3。
- 右表盘未接入的三项（原因已写在页面文件注释里）：
  `baoyang`（码 290 超出 `uint8_t`）、`coolant`（码 232 与智能模式冲突，Qt 后定义者生效）、
  `eco`（设置项 `ecoEnable/ecoIndicator` 驱动，不属于故障码，留到阶段 6）。
- 底部灯排 `s_bottom_lamp_items`（灯群 9 项，绝对坐标）：发动机 2、自动启停警告 27/219（`217.bin`）、
  自动启停未检测 28（`219.bin`，与上一盏完全重叠，靠创建顺序决定覆盖）、
  智能怠速工作 26、紧急制动 (828,630)、盲区 (886,630)、车道偏离 (944,630)、
  XMode (1002,630) 64×64、陡坡缓降 (1066,633) 30/94。
- 设置驱动的五项（14.8 已全部接线）：紧急制动/盲区/车道偏离由 `driver_assist` 开关 + `high_func.lane`
  选白/黄图；XMode 由 `drive_mode` 6/7/8 选图；Set 是文本徽标，在页面中单独创建，不占灯群。
  详见 14.8「底部灯排三项设置驱动灯」与「XMode / Set」。（右表盘 33/36 的 `show4h` 门控同属 14.8）
- 创建顺序：右灯群与底部灯排都在 `bg_01` 之前创建，保持 Qt 的 z 序（中央面板压在其上）。
- PC 假帧码表扩展到 64 个码（追加右灯群 14 个 + 底部 7 个），整轮约 32s。
- 验证方式：临时把 `0x02` 假帧改成「整组一次性下发」（payload 上限 16，右灯群 14 码 + 灯态刚好 15），
  截图确认位置与颜色后已还原。结论：两组的坐标、缩放、颜色（红/黄/绿/白）均与 Qt 一致。

**灯群复用约定（回答“其他表盘怎么办”）**：控件通用，**灯表按样式各写一张**。
其他样式的页面文件里各自定义 `static ui_lamp_group_t` + `s_xxx_lamp_items`（坐标按该样式 QML 抄），
`ui_lamp_group.c/.h` 不改、不出现样式名判断；同布局的灯可继续用 `ui_lamp_row`，两者可混用。
单实例上限 24 盏、每盏 4 种图标，超了就多开一个实例。

### 系统设置：版本信息（2026-11-07）

- 系统设置根菜单索引固定为 3；列表顺序为 LANGUAGE、DATE & TIME、VERSION INFO、FACTORY SETTINGS、CLEAR CACHE、BOOT ANIMATION、BACK。仅 VERSION INFO 与 BACK 可用，方向键跳过置灰项。
- 版本详情五项：APP VERSION（更新时间单独显示）、MAIN FIRMWARE、SMALL CHIP、WIRED、SYSTEM VERSION。应用版本和更新时间目前没有可信来源，显示 `---`，不使用 Qt 端推测值。
- MAIN FIRMWARE / SMALL CHIP / WIRED 分别来自 `car_state_t.version.main/chip/wired`，并受对应 `*_valid` 标志和可打印 ASCII 内容校验；无效值显示 `---`。
- SYSTEM VERSION 由标准 C `fopen`/`fgets` 读取 `/etc/dash_ver` 并剥除末尾换行；读取失败或空内容显示 `READ FAILED`。这是当前 LVGL 工程可用来源，与 Qt 版本页可能读取的其它应用/系统元数据并不等价。
- 返回栈为版本详情 → 系统设置列表 → 根页。

**当前验证状态**（2026-11-04 更新，含右表盘灯群、底部灯排、中央车辆视图与阶段 5 覆盖层）：PC 构建 0 错误 0 警告；
BOARD（aarch64）构建 0 错误 0 警告；`ctest` 1/1 通过；
`timeout 4 DISPLAY=:0 ./build/bin/carmw_ui_demo` 启动正常（exit=124）。
顶部灯排/巡航/小车门/左表盘灯群（20 盏）/右表盘灯群（12 盏）/底部灯排（5 盏）/中央车辆视图均已接入，
中心卡片已接入 10 张（导航/4x4-i/胎压/行程/底盘/短期/瞬时油耗/ECO/音乐/互联），**阶段 4 中心卡片全部完成**，
覆盖层 5 个（车门/胎压/雷达/定速巡航/提示层）全部完成，**阶段 5 全部完成**；
PC 假帧每 0.5s 轮换故障码，整轮约 32s；0x11 瞬时油耗每约 3s 切换一次显示状态，
0x0F ECO 每约 4s 在圆环/水平两种模式间切换，音乐卡每约 5s 轮换 FM/歌词/来电，
互联卡每约 5s 轮换 关闭/待连接/已连接/连接中（与音乐演示各自独立计数），
0x03 提示弹窗每约 5s 换一个 id、显示 3s，燃油低横幅每约 20s 出现 5s。

---

## 4. 后续待办（按顺序）

### 阶段 2：动态数据接线（已完成）

- ✅ 时钟：`car_clock_t` 驱动，`hour24` 决定 12/24 小时制
- ✅ 车外温度：`car_other_info_t.outside_temp_raw`，`units.fahrenheit` 决定温标
- ✅ PC 端假 MCU 补齐 `0x32`（Clock）与 `0x38`（OtherInfo）帧，时钟跟随本机时间
- ✅ 转向灯 `0x35`、警示灯 `0x02`、定速 `0x13` 已补假帧

### 阶段 3：警示灯系统 + 中央车辆视图（已完成）

- ✅ 转向灯（步骤 1）
- ✅ 顶部灯排 6 盏 + 定速巡航 + 小车门（步骤 2 / 2B）
- ✅ 左表盘灯群 20 盏（步骤 3，`ui_lamp_group` 绝对坐标灯组）
- ✅ 右表盘灯群 12 盏 + 底部灯排 5 盏（步骤 4，见 3.6；设置项驱动的 5 类灯留到阶段 6）
- ✅ 中央车辆视图（步骤 5，见 3.7）
- `0x02` 帧格式：`len==1` 为全局灯态；`len>=2` 时 `data[0]` 是灯态（0 灭 / 1 亮 / 2 快闪 / 3 慢闪），`data[1..]` 是故障码列表
- 灯态判据留在样式层，`ui_lamp` 只负责渲染
- 右表盘/底部灯排直接复用 `ui_lamp_group` 或 `ui_lamp_row`，只需新增样式层灯表，不改公共控件
- 注意 Qt `iconBindMap` 中 232 出现两次（冷却液 / 智能模式），后定义者生效；接右表盘冷却液灯时需确认协议侧该码的实际语义
- 注意码 289/290/291（EBD/保养）超出 `uint8_t` 故障码范围，当前协议模型无法承载

#### 3.7 中央车辆视图（阶段 3 步骤 5，已完成）

- 容器沿用静态版坐标：页面 (640,235)，尺寸 660×340，源图偏移 (-170,-330)。
- 8 张整图（`CheDao_01~04` + `lun_01~04`）一次性创建，切帧只改 `LV_OBJ_FLAG_HIDDEN`，
  避免每帧重建对象；图层顺序与 Qt 一致：车道 → 车身 → `lun_Color_mask` → 示宽灯 → 左/右转向灯。
- 示宽灯与左右转向灯素材来自 `SubaruForester/navigation/Deng_01~03.png`，
  经 `png2bin.py` 打平到 `SubaruForester/Deng_0x.bin`（bin32 4MB / bin16 3MB 各一张 1000×1000）。
- 帧轮播：`车速 > 0 且档位首字符非 P` 时每 150ms 切一帧，停车保持当前帧（对齐 Qt `runTimer`）；
  判据在 33ms 刷新里用 `lv_tick_elaps` 计时，不额外建定时器。
- 转向灯：500ms 亮灭（Qt 用 NumberAnimation 500ms），与顶部 1000ms 转向灯相互独立。
- 示宽灯：复用顶部灯排的码 12（Qt 绑 `iconPositionLight.show`）。
- 未接入（依赖设置项 SettingsMgr，留到阶段 6）：
  车色 multiply（`carColor`，默认白色等效于不处理）、右舵镜像（`rightHandDrive`，Scale -1 + Translate 100）、
  左右前/侧方视角图（Qt 中已注释掉）。
- PC 假数据为 D 挡 + 车速 40~160，因此车辆持续换帧；转向灯约 2s 左、2s 右交替。

### 阶段 4：中心卡片（10 张，已完成）

导航 / 4I / 胎压 / 行车信息 / 底盘 / 短期 / 瞬时 / ECO / 音乐 / 互联

| 卡片 | 文件 | 状态 |
|---|---|---|
| 导航（中央车辆视图） | `page_subaru_forester1.c` 内 `central_card_create` | ✅ 已迁入 `central_stack`，注册为第 0 张 |
| 4x4-i 驱动力 | `PageModel/page_4i.c` | ✅ 完成（见 3.10），注册为第 1 张 |
| 胎压监测 | `PageModel/page_tire.c` | ✅ 完成（见 3.11），注册为第 2 张 |
| 行程数据 | `PageModel/page_itinerary.c` | ✅ 完成（见 3.8），注册为第 3 张 |
| 底盘控制 | `PageModel/page_chassis.c` | ✅ 完成（见 3.12），注册为第 4 张 |
| 短期记忆 | `PageModel/page_short_term.c` | ✅ 完成（见 3.13），注册为第 5 张 |
| 瞬时油耗 | `PageModel/page_instant_fuel.c` | ✅ 完成（见 3.14），注册为第 6 张 |
| ECO 燃油经济性 | `PageModel/page_eco.c` | ✅ 完成（见 3.15），注册为第 7 张 |
| 音乐（多媒体/歌词） | `PageModel/page_music.c` | ✅ 完成（见 3.16），注册为第 8 张 |
| 互联 | `PageModel/page_connect.c` | ✅ 完成（见 3.17），注册为第 9 张 |

- 中心卡片容器 `central_stack`：页面 (553,140)，814×509，与 Qt `StackLayout` 对齐；卡片挂在其下，靠 `LV_OBJ_FLAG_HIDDEN` 切换
- 卡片不是独立样式节点（不进 `ui_app`），由样式页创建/销毁，接口 `xxx_card_create/apply/destroy`
- 通用模板 `ui_card_template_create`（标题区 150 高 + 内容区）与通用行控件 `ComControl/ui_info_row`（图标 + 文字）可被后续卡片复用
- 页签切换已完成（见 3.9）：`card_register` / `card_show` / `card_step`，按键 `UI_KEY_PREV`/`UI_KEY_NEXT` 循环切换；PC 端方向键经 `poll_keys()` 注入。**Qt 的 StackLayout 无平移动画**，直接换页

#### 3.8 行程数据卡片（阶段 4 步骤 1，已完成）

- Qt 坐标链：`StackLayout(553,140)` → Loader 居中 586×509 → 卡片在容器内 (114,0)；标题区 150，内容区 586×359
- 三行：`clock/kilometer/gauge` 图标 58×46 + 间距 25 + 文字，行距 35，Column 居中后偏移 (+50,-70)
- 素材：`settings/clock.png`、`kilometer.png`、`abb_gauge.png` → `assets/src/material2/MenuItem/{clock,kilometer,gauge}.png` → bin
- 数据源 `0x0C`（`car_trip_stats_t`），文字 `3h 12m` / `1560 km` / `62 km/h`；`units.mile` 时按 ×0.625 换算并换单位
- 两个坑：Qt 的 `Item 546×300` 不裁剪子项，LVGL 容器会裁剪，因此**不要**套中间容器，直接以内容区居中；
  字体 27 在本仓库 LVGL 中不存在（只有 16/28），已改用 `montserrat_28`，并删掉 `LV_FONT_MONTSERRAT_27`

#### 3.9 中心卡片切换（阶段 4 步骤 2，已完成）

- Qt 的 `StackLayout` **没有平移动画**（不是 300ms 滑动），`currentIndex` 直接换页；左右、上下键都切换卡片，左/上 = 上一张，右/下 = 下一张，循环
- 页面侧：`cards[UI_CARD_MAX]` 注册表 + `card_register/card_show/card_step`，靠 `LV_OBJ_FLAG_HIDDEN` 切换，同一时刻只显示一张
- 按键：`ui_node.h` 新增语义码 `UI_KEY_PREV/NEXT/OK`；`s1_on_key` 只认语义码，具体键位由输入层映射
- PC 演示：`main.c` 的 `poll_keys()` 用 `SDL_GetKeyboardState` 轮询方向键（按下沿触发），映射到 `ui_app_handle_key`；板端由真实方向盘按键驱动调用同一接口

#### 3.10 4x4-i 卡片（阶段 4 步骤 3，已完成）

- Qt `Page4I.qml` 全部由 Rectangle / Canvas 绘制，**无图片素材**，LVGL 侧同样用色块与描边矩形还原
- 坐标：root 400×450 居中于容器 → (207,29)；标题 `4x4-i` 28 号居中 top+20；内容区 360×356
- 左：车体 140×302 垂直居中，外廓 inset 10 圆角 35 边框 #66a3ff(40%)，前脸弧线、三根传动轴、四轮 24×45 + 外圈发光
- 右：185 宽，中刻度 0 / 50 / 100 %（22 号），上条 = 前轮、下条 = 后轮，外框 #7da7d9，填充 #a1caff→#4372b6 横向渐变
- **数据缺口**：carmw 协议没有四驱扭矩帧（Qt 用 0x20 动态驱动力分配，carmw 把 0x20 定义为转向盘按键），当前用 Qt 默认值 168 → 后轮 84% / 前轮 16% 占位，接真数据时改 `page4i_card_set_torque` 的入参
- 字体 22 两个 conf 都要开：`config/lv_conf.h`（PC）与 `config/lv_conf_board.h`（板端），漏一个则板端编译报 undeclared

#### 3.11 胎压卡片（阶段 4 步骤 4，已完成，Subaru 带车体图版）

- 采用 `TirePressure/SubaruTirePressure.qml` 布局（Qt 原版 Forester 分支的图片源全为空字符串，故此处按 Subaru 图样补全）
- 素材：`Image/TirePressure/TirePressure3/{car,left_front,right_front,left_after,right_after}.png`（396×386，5 张）已拷贝进 `assets/src/TirePressure/` 并转 bin32/bin16
- 坐标：卡片 586×509 居中于容器 → (114,0)；标题 topMargin 25（中文标题受限，用英文 `TIRE PRESSURE`）；胎压区 432×320 水平居中、**y 固定 58**（不垂直居中）
- 车体底图与四张高亮图都在卡片坐标 (95,58) 直接创建（396×386 高于 432×320 的胎压区，需要溢出父容器，故不放进胎压区）
- 文字块 (52,77)/(328,77)/(52,233)/(328,233)：左列 Row 固定 -18 → 起点 34；右列 `leftMagrin = -32` → 起点 296；数值 24 号 + 单位 18 号（底部对齐、下边距 2）
- 高亮图 `visible: state > 1`（2 偏低 / 3 偏高），与文字共用同一份四轮状态
- 单位随 `units.pressure_unit`：0 kPa、1 bar(÷102)、2 PSI(÷7.03)，4095 显示 `---`
- 0x0A 解码：`kpa = rd_u16 & 0x0FFF`，`state = 高 4 位`；假数据必须按大端构造（`(kpa>>8)|state<<4` + 低字节）
- **中文标题限制**：LVGL 自带 `lv_font_source_han_sans_sc_16_cjk` 只有 16 号且缺「胎」「压」字形（显示为方框），`transform_scale` 对该 label 不生效。故所有卡片标题统一用英文（`ITINERARY` / `4x4-i` / `TIRE PRESSURE`）。若后续必须显示中文，需用 lv_font_conv 生成 32~36 号的子集字体并放入 assets

#### 3.12 底盘控制卡片（阶段 4 步骤 5，已完成）

- Qt `CentralDisplay/ChassisControl.qml`，数据全部来自 0x26 轨迹帧（`car_trajectory_t`）
- 坐标：卡片 586×506 居中于容器 → (114,1)；标题 y=48（38 号，实际用 36）；转向角文字 y=100、#2ecc71、水平居中再左移 10，角度 0 时隐藏
- 车模：Qt `Image 1000×1000 + scale 0.39 + sourceClipRect(230,170,550,760)`，LVGL 无运行期缩放，改为**预处理**：PIL 把 6 张 PNG 裁剪+缩放成 215×296，存 `assets/src/OffRoad/` 再转 bin；屏幕位置 (184,152)（= 16+167.8, -80+231.8）
- 图层：车模 → `drive_bg_blue`（`chassis_lift`）→ 四轮指示 `2h_z/2h_y/after_2h_z/after_2h_y`（`wheel_lf/rf/lr/rr`），全部用 HIDDEN 切换
- 轨迹：两条 `lv_line` 按二次贝塞尔采 13 点；起点 (250,238)/(348,238)、终点 y=120、控制点 y=167、横向弯曲 `min(angle,128)/128×110`，`right=false` 取负。实测绿色像素包络 y=249~380、x=916~1069，与 Qt 坐标一致
- **与 Qt 的差异**：Qt 用 Canvas 画「半透明渐变扇形 + 15/8 流动虚线（800ms 循环）」，LVGL 版是两条实线，暂无渐变带与流动动画（实现需 lv_canvas 或 20+ 个虚线对象，板端 RAM 与复杂度取舍待定）
- 复用点：`set_shown()` 用 `uint8_t *last` 做可见性去重，**同一 last 变量不能给两个对象共用**（右轨迹线曾因此不显示）

#### 3.13 短期记忆卡片（阶段 4 步骤 6，已完成）

- Qt `CentralDisplay/PageShortTerm.qml`（CentralTemplate 分支），数据来自 **0x0E 帧**（`car_trip_t`：`avg_fuel_x10` / `avg_speed_kph` / `drive_minutes` / `distance_km`）
- 坐标：卡片 586×503 → 容器内 (114,3)；标题区 150 高（复用 `ui_card_template_create`）；内容 Item 546×264 于 (20,0)；图标 68×122 于 (89,95)；四行 Column 于 (182,40)，行高 38、间距 10
- 数值格式与 Qt 对齐：里程 `km` 整数 / `mi` 一位小数；时速 `km/h` / `mph`；时长 `3h 12m`；油耗 `avg==0 → "0"`、`avg>1200 → "--.-"`，其余按 `units.fuel_unit`（0 = L/100km 直接用 avg，1 = km/L = 10000/avg，2 = mpg = 28248/avg），单位 18 号紧跟数值
- **与 Qt 的差异**：中文标签 `小计里程：/平均时速：/行驶时长：/平均油耗：` 无 CJK 字形，改为英文 `TRIP DIST / AVG SPEED / DRIVE TIME / AVG FUEL`；字号 27 → 28
- 复用：`ui_card_template_create`、`row_create()`（名称 + 数值行，flex 底部对齐）、`set_text()`（文本去重）
- 踩坑：头文件被保存成 `page_short_term,h`（逗号），编译报找不到头；另 `UI_IMG` 在 `ui/ui_common.h`，只包含 `ui_assets.h` 会报隐式声明

#### 3.14 瞬时油耗卡片（阶段 4 步骤 7，已完成）

- Qt `CentralDisplay/PageInstantFuel.qml`（CentralTemplate 分支），数据来自 **0x11 帧**（`car_instant_fuel_t`）
- 坐标：卡片 586×503 居中于容器 → (114,3)；标题区 150 高（复用 `ui_card_template_create`）；内容区 546×264 于 (20,0)
- 图标 84×123 水平居中（x=231）、topMargin `(264-123)/2-20` → y=50；文本 34 号水平居中、topMargin `(264-h)/2+40` → LVGL 用 36 号、`LV_ALIGN_TOP_MID y=150`
- 素材沿用 `material2/MenuItem/instant_fuel.bin`（84×123，bin32/bin16 均已有；Style3 蓝色版 `instant_fuel0.bin` 当前不用）
- 解码：`value_x10 = raw & 0x7FFF`、`per_hour = raw & 0x8000`、`raw == 0xFFFE` → `visible = false`
- 显示规则：`per_hour` → `xx.x L/h`；`value_x10 == 0x7FFF` → `---.-`；`0` → `0.0 <单位>`；其余按 `units.fuel_unit`：0 = L/100km（x10 直接用）、1 = km/L（`10000/x10`）、2 = mpg（`28248/x10`）
- 与 Qt 的差异：Qt 原版的单位取 `SettingsMgr.fuelType`，这里改用 carmw 的 `units.fuel_unit`（与短期记忆卡片一致）；0xFFFE 时保留上一次显示值（Qt 该页未绑定 `isVisible`）
- 中文标题无字形，用英文 `INSTANT FUEL`
- PC 假帧 `0x11` 每约 3s 在「L/100km（5.0~12.9）→ L/h → 0x7FFF」之间轮换，覆盖三条显示分支

#### 3.15 ECO 燃油经济性卡片（阶段 4 步骤 8，已完成）

- Qt `CentralDisplay/EcoShow.qml`，数据来自 **0x0F 帧**（`car_eco_t`：`enabled` = `data[0]` bit4，`value` = `data[1]` 0~255）
- 平均油耗复用 **0x0E** 的 `trip.avg_fuel_x10`（Qt 取 `shortterm_avgFuel`）；ECO 卡内固定按 L/100km 显示，不随 `units.fuel_unit`
- 两种模式，靠 `LV_OBJ_FLAG_HIDDEN` 切换（等价 Qt 两个 Item 的 `visible`）：
  - **圆环模式**：`eco.enabled` 为真时显示。Qt 还需 `SettingsMgr.ecoIndicator && ecoOrfuel`，两者默认 true，设置体系留到阶段 6
  - **水平刻度模式**：`eco.enabled` 为假时显示
- 坐标：卡片 586×509 居中于容器 → (114,0)；标题区 150 高；内容区 546×300 于 (20,0)
- 圆环模式（内容区居中）：
  - 圆环 300×300 于 (123,0)；`lv_arc` 用 `pad_all=30` 把半径压到 120（`(300-60)/2`）
  - 底环宽 12、白色 10% 透明；进度环宽 14、`#00FFC3`；角度 `150° → 30°`（240° 弧，底部留 120° 缺口），进度角 `150 + 240×pct/100`
  - 末端光点**自绘** 20×20（白底 + 4px `#00FFC3` 边框），LVGL 内置 knob 会因半径 `r - width/2` 内缩 7px，故关闭内置 knob（`LV_PART_KNOB` 透明 + pad 0）
  - 光点位置用 `lv_trigo_sin` 计算（圆心半径 120、角度 150+2.4×pct），与 Qt 的 `cos/sin` 一致，不引入 libm
  - 中心列间距 15：`WarnIcon/244.bin`（65×48）+ Row（"ECO SCORE" 14 号 `#00FFC3` + 数值 24 号白色，间距 8）
  - 底部 "AVG x.x L/100KM" 18 号、白色 53%（`#88FFFFFF`）、离底 30
  - `pct = round(value×100/255) = (value×100 + 127)/255`
- 水平刻度模式：
  - 平均油耗行 (260,-10)：`AVG` 22 号（Qt 42 → 用 44 号）+ 数值 + `L/100km` 18 号，整体底部对齐（下边距 4/0/6）
  - 刻度区 420×120 于 (63,90)：条 420×26 圆角 4、`#22FFFFFF` 底 + `#44FFFFFF` 1px 边框
  - 填充条高 22、内缩 2，竖直渐变 `#74B2E0 → #337AB7`（Qt 三段渐变近似为两段）
  - 指针 `SubaruForester/point.bin`（原图 135×154）整图入 RAM，按 28×28 `CONTAIN` 缩放，位置 `x = 条宽 - 14 + 1`、`y = -22`
  - 刻度：主横线 420×2；大刻度 0/10/20/30（2×10 + 20 号数字，数字用 `align_to` 贴刻度下方 5px）；小刻度 5/15/25（1×6，白色 67%）
  - 量程固定 30 L/100km，超过按 30 截断
- **与 Qt 的差异**：Qt 水平模式容器的 `y:-100` 与 `anchors.fill` 冲突（实际被忽略），LVGL 侧按 0 处理；中文标题 "燃油经济性" → `FUEL ECONOMY`，`平均` → `AVG`
- PC 假帧 `0x0F` 每约 4s 在 enabled/disabled 间切换（验证两种模式），`value` 以 17/帧递增约 15s 走完 0~255
- 字体只用已开启的 14/18/20/22/24/36/44

- **下一步**：见 3.17（互联，阶段 4 最后一张）

#### 3.16 音乐卡片（阶段 4 步骤 9，已完成）

- Qt `CentralDisplay/PageMusic.qml`，**carmw 协议表无媒体/电话帧**（0x00~0xFF 已核对），数据改由演示链路注入：
  `main.c`（每约 5s 轮换）→ `ui_app_demo_media()` → 样式页 `on_demo_media` → `music_card_demo_set()`
- 新增通用演示钩子：`ui_node_vtbl_t` 末尾加可选 `on_demo_media`，`ui_app` 提供 `ui_app_demo_media()` 转发；板端不调用，样式不实现时静默忽略
- 坐标：页面 847×503，Loader `centerIn` 814×509 → **(-16,3)**；标题区 150；内容区 847×359（模板内高 353，body 用 359 让底边距与 Qt 一致，允许溢出 6px）
- 三种子模式（对应 Qt `media_mode`）：
  - **FM（0）**：列水平居中、底边距 179、间距 30；信源 64 号（LVGL 用 48）白色；频率 40 号 `#FEF2A4`（用 44 号）压在 `Audi/media.bin`（原图 280×120，按 260×80 `CONTAIN`）
  - **来电（2/5）**：列宽 847、底边距 151、间距 30；`Audi/call.bin`（168×168 → 120×120）；姓名 42 号粗体白色（用 44）；号码 32 号 `#CCCCCC`（用 36）
  - **歌词（其他）**：列宽 787、整体居中、间距 20；歌名 28 号粗体 `#FDA23B`（单行省略号）；歌词 32 号粗体白色（用 36，自动换行）
- 节奏柱：12 根 12×100、间距 10（总宽 254），底边距 230；柱宽 12、圆角 6、`#FDA23B` 70%，`#FFC247 → #FDA23B` 竖直渐变；`lv_timer` 80ms 用 `lv_trigo_sin` 做 14~70 高度正弦起伏（Qt 双段 SequentialAnimation 的近似），仅歌词非空时 `lv_timer_resume`，否则暂停
- 标题随模式变化（注入的 `title`），标题标签通过 `lv_obj_get_child(lv_obj_get_parent(content), 0)` 取模板首子项
- 素材：新增 `assets/src/Audi/{media,call}.png`（从 Qt `Image/Audi/` 拷贝），已用 `png2bin.py --only Audi` 生成 `bin32/bin16/Audi/*.bin`
- **两个易踩的坑**（已修）：
  - 需要缩放的图（media 280×120→260×80、call 168×168→120×120）必须 `ui_assets_load_image()` 整图驻留 RAM 后再 `set_size + CONTAIN`；直接用流式 `UI_IMG()` 只按对象尺寸裁切，看起来像“图片只显示一半”
  - 来电列按 Qt 底边距 151 排布时总高约 273，顶边到内容区上方约 65；**父容器裁子项用的是 `coords + ext_draw_size`，而 `ui_blank_container` 去掉了所有样式，ext 为 0，`LV_OBJ_FLAG_OVERFLOW_VISIBLE` 在此无效**。解决：电话列直接挂 `card->root`（底边距卡底 145，等价内容区底边距 151），FM/歌词/节奏柱仍挂 body（均在内容区内）
- 字体沿用 14/18/20/22/24/28/36/44/46/48；中文标题/文案受限，演示内容全英文
- **与 Qt 的差异**：Qt 歌词多行上限 4 行，LVGL 由 label 自动换行不截断；Qt 动画参数各自不同，这里统一为相位错开的正弦

#### 3.17 互联卡片（阶段 4 步骤 10，已完成）

- Qt `CentralDisplay/PhoneConnect.qml`（页签最后一张），**carmw 无手机互联数据**，同样走演示链路：
  `main.c`（每约 5s 轮换 4 态）→ `ui_app_demo_connect()` → `on_demo_connect` → `connect_card_demo_set()`
- 新增第二个演示钩子 `on_demo_connect(state, mode_text, bt_name)`，与 `on_demo_media` 并列；板端不调用
- 坐标：卡片 586×503 于 (114,3)；标题区 150；内容区 546×264 于 (20,0)
- 四态（Qt `conEnable` + `connected` / `connecting` 的组合）：
  - **关闭（0）**：两行 24 号白字居中，topMargin 50，宽 506
  - **已连接（2）**：预览图 406×174 居中 (70,45)，拉伸铺满 + 3px `#00C800` 边框；
    模式名 16 号（用 18）贴图上方左对齐。模式名/预览图按 CarPlay / Android Auto / HiCar 三套切换
  - **待连接（1）**：列居中、列间距 14；行内间距 10：
    模式图标 28×28 + `CURRENT MODE: xxx` 32 号（用 36）→ 蓝牙图标 28×28 + 提示 24 号 → 26×26 占位 + `BLUETOOTH: xxx` 24 号 `#00C6FF`
  - **连接中（3）**：240×140 居中、圆角 12、`#E0000000` + 1px `#40FFFFFF`；
    40×40 圆环 4px `#00C89F`（Qt 是整圈旋转，视觉静止，故不做动画）+ `CONNECTING...` 20 号
- 素材：`WarnIcon/{android-auto,carplay_fill,HiCar,bluetooth}.bin` 直接复用；新增 `AA.bin`（`AA.jpg`→406×174 预览）、`HC.bin`（`HC.png`），`carplay.bin` 原有多媒体大图复用
- 所有缩放图（三套模式图标、蓝牙、三套预览图）均 `ui_assets_load_image()` 懒加载驻留 RAM，预览图按需加载（不会一次占满三张）
- 预览图上方模式名挂在 `body` 上而非 406×174 容器上，避免被容器矩形裁掉（同 3.16 的裁剪坑）
- 字体只用 18/20/24/36；中文标题/文案受限，全部英文
- **与 Qt 的差异**：Qt PC 分支默认 `conEnable=false`，只显示关闭提示；板端才由 ZLink 状态驱动，这里用演示态覆盖


### 阶段 5：覆盖层（已完成）

`src/ui/overlays/`：`ov_door` / `ov_tire` / `ov_radar` / `ov_cruise` / `ov_tips`

覆盖层不是独立样式节点，由样式页创建/销毁并挂在页面根对象上（最后创建 = 最上层）。
共用规则：任一覆盖层激活时隐藏 `central_stack`（等价 Qt `contentShow=false`）；
互斥优先级 **车门 > 胎压 > 定速巡航 > 雷达**，提示条/弹窗可叠加显示。

#### 3.18 车门覆盖层（阶段 5 步骤 1，已完成）

- Qt `door/PageCarDoor.qml` + `main.qml` 的 `doorLoader`：页面 (620,100)；7 个 `DoorClip` 各 720×550
- 素材 `Image/Door/Style2/door_*.png`（1000×1000）已用 PIL 复刻 Qt 的 `sourceSize 1024 + scale 0.6 + 偏移(170,240) + 裁切 720×550`，
  输出 `assets/src/Door/Style2/ov_{bg,front,left_front,left_after,right_front,right_after,after}.png` → bin32/bin16，运行时零缩放
- 图层顺序同 Qt：底图 → 引擎盖 → 左前 → 左后 → 右前 → 右后 → 后备箱
- 触发：`0x0B` 门状态帧 `valid` 且任一车门/后备箱/引擎盖打开；全关立即隐藏
- 位映射同 Qt（非右舵）：`b0` 左前、`b1` 右前、`b2` 左后、`b3` 右后、`b4` 后备箱、`b5` 引擎盖
- 用 6 位状态位图去重，状态未变时一次样式设置都不做
- 未接入：右舵镜像（`SettingsMgr.rightHandDrive`，阶段 6）；其它车型分支的坐标与素材（本工程只复刻 Style1）

#### 3.19 胎压覆盖层（阶段 5 步骤 2，已完成）

- Qt `TirePressure/PageMainTirePressure.qml` 的 `mainVisible` 分支：586×509 位于页面 (668,150)
- 复用中心卡片的 `tire_card_t`：外层容器 814×509 @ (554,150)，卡片内部 (114,0) 偏移正好落到 (668,150)
- 显示：任一车轮 `state > 1`（偏低/偏高）；异常消失立即隐藏
- 10s 自动关闭：成功胎压帧递增 `tires.sequence`，仅新异常帧续期；普通 UI 刷新不续期，到期后旧快照不重开。车门遮挡期间仍计时。
- 事件计时/已消费进度保存在 `ui_session`（跨样式存活），样式重建不续期、不重放；详见 12.2 C1 记录。
- 数据/单位换算与中心胎压卡片共用 `tire_card_apply`，不重复实现
- 与 Qt 差异：Qt 的程序化 `NumberAnimation` 入场缩放未做（0.5→1，300ms）

#### 3.20 雷达覆盖层（阶段 5 步骤 3，已完成）

- Qt `CentralDisplay/PageRadar.qml`：290×430 @ (815,160)
- 素材：`Image/Subaru20/Radar/{radar_bg,car,radar_wave,radar_g,radar_y,radar_r}.png`
  - 背景 290×430、小车 114×246 居中
  - 前波 @ (50,10)，后波 @ (50,330)；Qt 的波与小弧带父子旋转（前波父旋转 180°、子弧 -93/-62/-31/0°）
  - 用 PIL 完整复刻该变换链，把 8 个传感器 × 绿/黄/红 共 24 个弧段各自裁到包围盒输出为独立精灵，
    运行时只做定位与显隐，零旋转零缩放
  - 输出到 `assets/src/Radar/`（`front_0..3_{g,y,r}`、`rear_0..3_{g,y,r}`、`wave_front/wave_rear`）→ bin32/bin16
- 图层顺序同 Qt：背景 → 前波 → 前弧 → 小车 → 后波 → 后弧（后弧叠在小车上）
- 数据 `0x22`（11 字节）：前 5 路 + 后 5 路，每字节 `color` 高 2 位 / `value` 低 6 位
  - `value == 0x3F` → 该传感器无目标，隐藏
  - `color` 0/1/2 → 绿/黄/红；color 3 视为无效隐藏
- 显示：`radar.display_on`（`data[10]` bit0）为真；D 挡（首字符 `D`/`M`）显示前雷达，R 挡显示后雷达
- 页面 (815,160) 与 0.975 页面缩放由样式页统一处理
- 用 `uint32_t last_sig`（active + 方向 + 8 路 × 2bit）去重，无变化时不碰任何对象
- 优先级：被车门 / 胎压抑制；命令行演示数据见 `main.c`（约 4s 开 4s 关，前 4 路颜色轮流变化）
- 未接入：真实 CarPlay 会话检测（阶段 6.6，需 ZLink SDK）；会话标志接口已就绪，PC 演示“已连接”可验证隐藏；`radaronoff` 已由设置项 `radar_on` 接入

#### 3.21 定速巡航覆盖层（阶段 5 步骤 4，已完成）

- Qt `CentralDisplay/PageNavigationInfo.qml`：830×470 @ (440,122)，Style1 默认 `scale 0.8`（绕 item 中心）
- 根容器 600×470 @ (560,200)；所有子图按页面坐标减 (560,200) 后的相对坐标摆放，详见 `ov_cruise.c`
- 图层顺序与 Qt 声明顺序一致：车道 → 左盲区 → 右盲区 → 前车 → 跟车距离 1~4
  → 左/右偏移车道 → 左/右偏移车辆 → 跟车小车 → 居中大车
- 素材：Qt `Image/navigation/` 下的 `road.gif`（取首帧）、`hzzzz.png`、`hyyy.png`、`qf.png`、
  `GenChe_01..04.png`、`l.gif`/`r.gif`、`pianyi.png`、`mincar.png`、`maxcar.png`
  - 整条变换链（`sourceClipRect` → 子图 `scale` → item 0.8 缩放 → 旋转）用 PIL 预渲染，
    输出 14 张页面级精灵到 `assets/src/CruiseNav/` → bin32/bin16，运行时零缩放零裁切
- 数据来源：
  - `0x13` `car_cruise_t`：`cruise_state` bit0 → controlLed；`acc_state` bits0-2 → distanceShow、
    bit3 → showFrontCar、bits4-5 → distancecolor
  - `0x16/0x0E` 解码后独立保存到 `car_high_func_t.lane`：`lane_center`、`car_state`、`show_area`、`show_car`。输入偏移仍为子帧 `[2]&3`、`[4]&7`、`[5]&7`、`[6]&7`；UI 不再读取最后一帧 `raw`，其他高配子帧不会清除车道状态。
- 显示条件：`controlLed || laneCenter || carState==2/3 || showarea!=0 || showcar!=0`，且未被车门/胎压抑制
- 子层显隐：
  - 车道 = `carState<=1`；左盲区 = `showarea` 2/3；右盲区 = `showarea` 1/3；前车 = `showcar==1`
  - 跟车距离 i = `carState<=1` 且 `distanceShow>=i+1`
  - 左偏移层 = `carState==2`；右偏移层 = `carState==3`
  - 跟车小车 y 按 `distanceShow` 0~4 查 `s_min_car_y[5] = {33,61,39,33,17}`
  - 居中大车 = `carState==1 || showcar>0 || showarea>0`，且 `carState` 非 2/3
- 用 `uint32_t last_sig`（display + carState + showarea + showcar + laneCenter + distanceShow + showFrontCar）去重
- `SettingsMgr.blindSpotWarning` / `emergencyBraking` / `laneDepartureWarning` 已接入：值来自 0x16 的 0x86 ack 回读，设置页 `DRIVING ASSIST` 只下发命令不本地翻转（见 14.8）
- 与 Qt 差异：Qt 车道用 `AnimatedImage` 播放，LVGL 用首帧静态图；跟车小车位置直接查表，不做动画

#### 3.22 提示层（阶段 5 步骤 5，已完成）

- 三个独立部件共用全页透明容器 `ov_tips`，最后创建（最上层），**不参与覆盖层互斥**，也不隐藏 `central_stack`
- **燃油低横幅**（Qt `FuelLowTipsWindow.qml` 默认分支）：230×60 @ (845,30)，黑底 + 1px `#FFF59A` 边框；
  42px 图标 + 24 号 `#FFF59A` 文字 `LOW FUEL!`；`fuel.icon_state == 2` 时显示
- **P 挡提示条**（Qt `DoorOpenTipsWindow.qml` 默认分支）：宽 = 10 + 42 + 12 + 文字宽（运行期量文本宽度），
  `x=(1920-宽)/2-20`、`y=30`、高 60；车门覆盖层激活且档位首字符非 `P` 时显示；
  文案 `SHIFT TO P, STAY SAFE!`
- **通用提示面板**（Qt `GeneralTipsWindow.qml`）：260×280 @ (834,253)，与 Qt `Item x:834 y:253` 对齐
  - 背景 `tips_bg_260.bin`（由 268×288 原图预缩放）+ 30% 黑遮罩 + 圆角 8
  - 标题行居中：28px 红三角 + 8px 间距 + 22 号 `WARNING`（id 27/28/29 时为 `PARK/START`）
  - 1px 白色 20% 分隔线；中部 64px 图标（id 39 偏左 x=48，其余 x=80）+ 18 号白字居中换行
  - 底部 16 号（板端用 14）`#CCCCCC` `SEE USER MANUAL`
- 数据：`0x03` 弹窗帧 `{text_id, icon, duration_s}`；相同 `(text_id, icon, duration_s)` 只触发一次，
  `duration_s == 0` 用 2000ms 默认值，到期后隐藏
- 素材：`WarnIcon/tips/` 下新增 `red_warning_28/42`（由 `triangle.png` 重着色为红色）、
  `fuel_low_42`（由 `left_fuel_low.png` 重着色为 `#FFC200`）、`tips_bg_260`、
  `icon_*`（64×64，按 Qt 的 `PreserveAspectFit` 预合成），运行期零缩放
- 文案全部英文（Montserrat 无中文字形），`id → text` 映射逐条对齐 Qt `GeneralTipsWindow` 的 switch
- PC 演示：`0x03` 的 id 23/35/4 每 5s 轮换、显示 3s；燃油低 `icon_state=2` 每 20s 出现约 5s
- `settingsPageOpen` / `ismedia` 时隐藏通用提示已实现（`ov_tips_set_obstructed()`，仅隐藏通用提示面板）

### 阶段 6（后置）

设置体系（明暗、单位、卡片可见性）、CarPlay、升级流程。

详细迁移步骤、依赖、验收及待确认项见本文第 14 节。该节是实施计划，不代表已实现或已批准执行；此前 E2/E3 未闭环项继续保留。

---

## 5. 构建与验证

### 5.1 命令

```bash
# PC 模拟器（SDL2）
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build
DISPLAY=:0 ./build/bin/carmw_ui_demo

# BOARD配置（只有该目录已配置正确目标工具链时才是交叉编译；首次配置见E1）
cmake -S . -B build_board -DCARMW_PLATFORM=BOARD
cmake --build build_board -j4
```

平台切换由 `CARMW_PLATFORM`（`PC` / `BOARD`）控制，宏 `CARMW_PLATFORM_BOARD` 自动定义。

### 5.2 ⚠️ 关键坑：hgfs 共享目录时间戳失效

本项目在 `/mnt/hgfs`（VMware 共享目录）下，源文件 mtime 显示为 `1970-01-01`，
导致 `make` 认为对象文件永远是最新的，**改完代码不会重新编译**，从而掩盖编译错误。

**改完源码后必须强制重建全部受影响目标：**

```bash
cmake -S . -B build
cmake --build build --clean-first -j4
ctest --test-dir build --output-on-failure
```

板端使用独立且已确认工具链的构建目录，同样 `--clean-first`。新的验收流程优先使用非 hgfs 构建目录，详见 A1/E1。

> 依赖 `make` 输出 `Built target` 判断成功不可靠；只删除 `carmw_ui` 对象也不足以验证状态结构和协议库变更。

### 5.3 运行时注意

- `SDL_VIDEODRIVER=dummy` 无法创建渲染器，必须用真实 X11（`DISPLAY=:0`）
- 用 `timeout 4 ./build/bin/carmw_ui_demo` 快速验证启动，`exit=124` 为超时正常终止

---

## 6. 已确认的数据契约（避免再猜）

来源：`include/carmw/car_state.h`

| 结构体 | 字段 | 说明 |
|---|---|---|
| `car_fuel_t` | `percent` | 油量百分比 0~100 |
| | `remaining_range_km` | 剩余续航公里 |
| | `icon_state` | 0 无 / 1 正常 / 2 低油量 / 3 报警 |
| `car_coolant_t` | `percent` | 水温百分比 |
| | `state` | `CAR_COOLANT_NORMAL / HIGH / OVERHEAT` |
| `car_mileage_t` | `trip_type` | **0 ODO / 1 TRIP A / 2 TRIP B**（与 Qt 一致） |
| | `odometer_raw` | 总里程，单位 km |
| | `trip_a_raw` / `trip_b_raw` | 小计里程，单位 **0.1 km** |
| `car_gear_t` | `text[4]` | 档位字符串，**不是枚举**；`car_gear_pos_t` 另有枚举定义 |
| `car_units_t` | `mile` | true 时按英里显示 |
| `car_speed_rpm_t` | `speed_kph` / `engine_rpm` | 车速、转速 |
| `car_warnings_t` | `levels[]` | 各码持久灯态；用 `car_state_warning_level(state, code)` 取 0/1/2/3；list/global 仅为最近指令信息 |
| `car_tires_t` | `sequence` | 每次成功解码胎压帧递增，包括内容相同的新帧；不是 UI 刷新计数 |
| `car_high_func_t` | `lane` | 独立车道辅助状态，仅完整 0x0E 子帧更新，与最后一条 raw 隔离 |
| `car_cruise_t` | `set_speed_kph` | 定速设定速度（km/h）；顶部巡航图标/数值的数值来源 |

### 换算公式

| 项 | 公式 | 出处 |
|---|---|---|
| 总里程→英里 | `odometer_raw / 1.60934`，保留 1 位 | `ComTotalMileage.qml` |
| 小计→公里 | `trip_raw / 10.0`，保留 1 位 | `ComSubTotalMileage.qml` |
| 小计→英里 | `trip_raw / 16.0934`，保留 1 位 | 同上 |
| 车速指针角 | `-1950 + speed × 15 / 2`（LVGL 单位 0.1°） | `ForesterSpeed1.qml` |
| 转速指针角 | `1800 - rpm × 6 / 25` | `ForesterRotate1.qml` |
| 油量/水温液面 | `y = 318 - p × 210 / 100` | `ForesterFuelTank1.qml` |

---

## 7. 其他约定

### 7.1 资源管线

- 源图：`assets/src/**.png|jpg`
- 产物：`assets/bin32/`（PC，ARGB8888）、`assets/bin16/`（板端，RGB565A8）
- bin 布局：`lv_image_header_t(16B) + 像素数据`；`magic=0x19`，`cf=0x10`(ARGB8888) / `0x14`(RGB565A8)
- 运行时资源根：`PC → "A:/bin32/"`，`BOARD → "A:/bin16/"`（`ui_common.h` 的 `UI_ASSET_ROOT`）
- 新增/替换素材后执行：
  ```bash
  python3 tools/png2bin.py --force
  ```
- 脚本含目录扁平化规则 `FLATTEN_DIRS`：`SubaruForester/navigation/` 与 `material2/RotateSheet/` 的文件会去掉中间层目录，与既有 `bin32` 布局保持一致
- 顶部小车门用到的 3 张 64×64 素材从 Qt 工程复制后单独转换（输出 `Door/Style1/`）：
  ```bash
  mkdir -p assets/src/Door/Style1
  cp /home/xfy/CarHml_XIAO/Image/Door/Style1/small_door_{bg,left_front,right_front}.png assets/src/Door/Style1/
  python3 tools/png2bin.py --only Door
  ```

### 7.2 需要整图驻留 RAM 的图片

`LV_BIN_DECODER_RAM_LOAD=0` 时 LVGL 是流式解码，无法做旋转/缩放变换。
需要变换的图（如指针）必须用 `ui_assets_load_image()` 整读进 RAM 转 `lv_image_dsc_t`。
静态背景图直接用 `ui_assets_path()` 交给 `lv_image_set_src`，走流式解码。

⚠️ `lv_image_set_src` 保存的是字符串指针，不会复制路径。`ui_assets_path()` 返回的是静态缓冲区，
**同一父对象里连续设置多张动态图时会全部指向最后一个路径**。静态图请直接用 `UI_IMG("...")`
字符串字面量（`ui_small_door` 就是传 `UI_IMG()` 的结果）；只有单张图才可以临时用 `ui_assets_path()`。

### 7.3 字体

- PC 与板端 `lv_conf` 字号已对齐：14/20/24/28/36/44/46/48（PC 端此前漏开 14，已在 `config/lv_conf.h` 补 `LV_FONT_MONTSERRAT_14`）
- LVGL 内置 Montserrat 没有粗体字重，Qt 的 `font.bold: true` 只能近似为同字号常规体
- 中文字库尚未转换，所有中文文案暂用英文占位（如 `"RANGE: "` 应为 `"续航里程:"`）
- Qt 参考字号：档位 46（forester 材质实际 62）、里程 24~28、转速刻度 44/48

### 7.4 性能与线程约束

- 刷新定时器周期 33ms，`apply` 内部必须做「值未变则跳过」，避免每帧重建字符串/重设样式
- 禁止每帧动态分配（组件状态一律放私有结构，不用文件级可变状态）
- 板端双线程模型：UI 线程不得调用 `car_link_pump()`
- 关闭顺序：停 UI 定时器 → `car_link_stop()` → `car_link_deinit()` → `ui_app_deinit()`
- 所有 `port.write` 仅在后台收帧线程执行

---

## 8. 踩过的坑（不要重犯）

| 坑 | 说明 |
|---|---|
| 猜测字段名 | 曾把 `fuel.percent` 写成 `fuel.fuel_percent`、把 `gear` 当枚举。**必须先读 `car_state.h` 核对** |
| 资源路径猜错 | `water_hig.bin` 实际在 `material2/`，不在 `SubaruForester/` |
| 样式专用 vs 通用 | 档位/里程/续航是**所有样式共用**的，放 `ComControl/` 且不带样式后缀 |
| `trip_type` 映射 | 曾按 `car_state.h` 旧注释实现（2=A/3=B），已修正为 `0 ODO / 1 TRIP A / 2 TRIP B` |
| 构建假成功 | hgfs 时间戳失效导致 make 不重编，见 5.2 |
| IDE 报错 | 编辑器的 `'lvgl.h' file not found`、`Unknown type name 'lv_obj_t'` 属 IntelliSense 未加载 CMake include 路径，以实际编译结果为准 |
| 闪烁卡顿 | 转向灯/警示灯一律用 `LV_OBJ_FLAG_HIDDEN` 硬开硬关，**不要用透明度动画**（板端会明显卡） |
| `blink_ms` 语义 | 它是完整亮灭周期，不是半周期；Qt 快闪“每 300ms 切一次”要传 600，慢闪 3000 要传 6000 |
| 多图路径 | 同一父对象连设多张图不能用 `ui_assets_path()`，会被静态缓冲区覆盖，改用 `UI_IMG()` 字面量（见 7.2） |
| 顶部 miniDoor 语义 | Qt 只由故障码 22 驱动（背景 + 左右前门），**不读 `0x0B` 门状态**；不要自行扩展成五门显示 |
| 删除文件 | `DeleteFile` 工具对 hgfs 目录失败（无法移动到废纸篓），改用 `rm -f` |

---

## 9. 关键参考文件

### Qt 参考（只读）

- `/home/xfy/CarHml_XIAO/qml/PageModel/PageSubaruForester1.qml` — Style1 页面布局与坐标主来源
- `/home/xfy/CarHml_XIAO/qml/main.qml` — 页面管理、样式切换
- `/home/xfy/CarHml_XIAO/qml/GlobalData.qml` — 数据项定义
- `/home/xfy/CarHml_XIAO/qml/SpeedDial/ForesterSpeed1.qml`
- `/home/xfy/CarHml_XIAO/qml/RotationDial/ForesterRotate1.qml`
- `/home/xfy/CarHml_XIAO/qml/FuelTank/ForesterFuelTank1.qml`
- `/home/xfy/CarHml_XIAO/qml/WaterTemp/SubaruForesterWater1.qml`
- `/home/xfy/CarHml_XIAO/qml/ComControl/PageGear.qml`
- `/home/xfy/CarHml_XIAO/qml/ComControl/ComTotalMileage.qml`
- `/home/xfy/CarHml_XIAO/qml/ComControl/ComSubTotalMileage.qml`
- `/home/xfy/CarHml_XIAO/qml/CentralDisplay/NavigationForester.qml`
- `/home/xfy/CarHml_XIAO/qml/LampControl/` — 各警示灯图标与 `blink()` 语义（转向灯、顶部灯排）
- `/home/xfy/CarHml_XIAO/qml/ComControl/LightTemplate.qml` — 灯图标默认 52×52、缩放方式
- `/home/xfy/CarHml_XIAO/qml/ComControl/IconConstantSpeed.qml` — 定速巡航图标/数值/单位
- `/home/xfy/CarHml_XIAO/qml/CentralDisplay/PageSmallDoor.qml` — 小车门 7 张图与 `setDoorStatus` 映射

### 本项目关键文件

- `src/ui/ui_node.h` — 组件契约
- `src/ui/ui_app.c` — 样式切换核心
- `src/ui/ui_theme.h` — 颜色/字号/`ui_blank_container`
- `src/ui/ui_common.h` — `UI_ASSET_ROOT`、`UI_DESIGN_W/H`、`UI_PAGE_SCALE`
- `src/ui/PageModel/page_subaru_forester1.c` — Style1 页面（拆分工作的参照样板）
- `src/ui/ComControl/` — 通用复合控件（新增通用控件请放这里）
- `src/ui/widgets/` — 通用原子控件
- `include/carmw/car_state.h` — 数据契约唯一权威
- `tools/png2bin.py` — 资源转换
- `CMakeLists.txt` — `carmw_ui` 源文件列表（新增 `.c` 必须登记）

---

## 10. 接手基线与本轮修复（2026-11-04）

### 10.1 状态口径

- 阶段 0～5 已有组件实现，不等于已通过 Qt 行为一致性、像素对比和板端验收。不要重新从中央车辆视图或中心卡片开始搭建。
- 当前优先级：剩余逻辑修复 → 生命周期与事件验证 → 同状态视觉对齐 → 板端验收 → 经用户确认后实施阶段 6。
- 旧章节与本节冲突时，以本节的修复记录、待办和实际代码为准；协议含义必须再核对 Qt 解析、Handler、QML 三层，不依据文档猜字段。
- 原 Qt 工程保持只读。默认只复刻 Style1；GeneralUi1 是切换回归用样式，不扩展其他车型。

### 10.2 已实施的三项修复

1. **警示灯按码持久保存**：`car_warnings_t.levels[]` 是灯态来源；局部帧只更新指定码，单字节全局帧更新全部码；到达顺序决定最终状态。`global/list` 保留最近指令信息，不再用于推算各灯状态。
2. **胎压按新帧续期**：成功解码递增 `car_tires_t.sequence`；覆盖层仅新异常帧续期，旧快照到期后不重开。正常帧关闭。车门遮挡不暂停计时，仅在有效期内解除遮挡时恢复。
3. **车道状态隔离**：`car_high_func_t.lane` 保存 `0x0E` 解码结果；其他子帧不覆盖，完整的新 `0x0E` 可以清零；`raw` 仍代表最近高配子帧，不再供巡航层读取车道状态。

相关代码：[car_state.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/include/carmw/car_state.h)、[car_decode.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/car_decode.c)、[car_state.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/car_state.c)、[ov_tire.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/overlays/ov_tire.c)、[ov_cruise.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/overlays/ov_cruise.c)。

已有回归：[test_decode.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/tests/test_decode.c)、[test_state.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/tests/test_state.c)、[test_overlays.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/tests/test_overlays.c)。

**验证记录边界**：上一轮报告 PC 构建成功、CTest 2/2 通过；覆盖层用无窗口 LVGL 对象测试，不执行渲染。未完成板端运行、真实串口接入、Qt 截图差异验证。鉴于 hgfs 时间戳问题，接手者必须重新执行 A 阶段干净构建，不能直接沿用“构建通过”结论。

### 10.3 修复后必须继续检查的两个衔接问题

- **演示警示灯输入**：[main.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/main.c) 的轮播只发 ON/BLINK，没有关闭上一盏灯。新的增量语义会使灯逐渐累积。若要保持单灯轮播，应在切换时明确发上一盏 OFF，再发下一盏 ON，不能退回错误的“最后列表即全量状态”。多灯并亮必须另设独立测试场景。
- **胎压计时跨样式生命周期**：[ui_app.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/ui_app.c) 切换样式会销毁覆盖层，返回时把缓存快照重新 apply。新对象的 `seen_frame=false`，可能把旧异常当成新事件重新计时。当前覆盖层测试没有跨销毁重建用例，必须在 C 阶段验证和修复，不能宣称三项修复已覆盖所有生命周期。

---

## 11. 后续复刻与测试执行方案

### 11.1 执行规则与完成定义

1. 按 A→B→C→D→E 执行；F 为用户确认后的后置功能。每次只领取一个任务编号，不能同时大规模改协议、布局和资源。
2. 每项按“读两端源码 → 写输入/预期 → 最小修改 → 自动化回归 → 必要的截图/板测 → 回填证据”执行。
3. 测试使用明确标注的本地合成数据、内存端口和模拟器，不能把模拟结果记为实车结果；不要向真实行驶车辆发送测试指令。
4. 区分四种状态：已实现、自动测试通过、视觉验收通过、板端验收通过。不以“编译成功”替代后三项。
5. 已约定的英文占位、动画简化、覆盖层优先级不可悄悄改掉；若要恢复 Qt 行为，先列差异并取得确认。
6. 新增 `.c` 必须登记到 [CMakeLists.txt](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/CMakeLists.txt)。优先扩充现有测试，不创建平行框架或通用插件系统。
7. 代码与结果不符时，以实际证据修正文档；原 Qt 自身异常应单列，不盲目复制。未运行的用例写“未验证”，禁止补写通过记录。
8. 不修改现有用户坐标来掩盖状态错误。布局调整必须有同状态 Qt 参照图和坐标推导。

### A. 冻结可重复基线与测试入口

#### A1：确认环境，执行干净构建

步骤：
1. 读取本节、数据结构、CMake 配置和现有测试。记录源码目录、编译器、LVGL 配置、资源根及当前改动；若目录不是 Git 仓库如实说明，不自动初始化或提交。
2. 优先在本机非 hgfs 文件系统创建新的构建目录；如目录已存在，先检查其源码和平台缓存，不能混用 PC/BOARD。
3. 强制清理全部目标后构建，不能只删 UI 对象，因为状态头文件还影响 carmw 和测试。下面是 PC 示例；在项目根目录执行。

```bash
cmake -S . -B /tmp/lvgl_two_pc_acceptance \
  -DCARMW_PLATFORM=PC -DCARMW_BUILD_UI=ON -DCARMW_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j4
ctest --test-dir /tmp/lvgl_two_pc_acceptance -N
ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure
```

4. 当前应发现 `carmw`、`carmw_overlays` 两个测试套件。记录实际测试名、退出码、失败断言和构建警告；已有警告与新增警告分开，不伪报零警告。
5. 每次修改后的验收构建仍使用 `--clean-first`，直到确认共享目录时间戳问题已解决。
6. 有可用 X11 会话时，从项目根目录启动 PC 程序；`DISPLAY=:0` 只是示例，必须以实际会话为准。

```bash
DISPLAY=:0 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo
```

验收：所有现有测试通过，程序可启动且资源无缺失；没有图形会话时，只记对象测试通过，视觉测试阻塞。无窗口对象测试不需要 SDL 窗口，但不能据此认定 SDL dummy 支持截图渲染。

#### A2：把动态演示与确定性验收分开

步骤：
1. 修正警示灯轮播的 OFF/ON 序列，保留专门的多灯并亮场景；不改协议状态保存逻辑。
2. 为截图/页面测试增加最小的 PC 专用固定场景入口，或独立测试目标；不得在板端编译路径启用假帧。
3. 固定车速、转速、单位、档位、时钟、卡片索引、覆盖层和灯态；关闭自动轮播以及真实系统时钟驱动。动画测试单独推进虚拟时钟，静态截图固定动画相位。
4. 场景至少包括：正常主仪表、每张中心卡片、各覆盖层、每组警示灯、单位切换和无数据。给每个场景稳定 ID。
5. 保留两条测试通路：直接状态快照用于组件显示；内存端口→parser→decode→snapshot→UI 用于端到端验证。后者只用正常协议业务帧及已定义业务边界，不将静态假值截图当成协议集成验证。
6. 命令行参数/场景接口目前尚未存在，先实现再把真实命令回填；不要直接写不存在的 `--scenario` 参数并声称可运行。

验收：同一场景重复执行能获得相同数字、图层和动画相位；普通演示与固定场景互不污染；测试结束恢复/销毁状态。

### B. 修复剩余已确认逻辑差异

#### B1：车速及距离单位闭环

文件：[speed_forester1.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/SpeedDial/speed_forester1.c)、[page_subaru_forester1.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/PageModel/page_subaru_forester1.c)、[ui_range.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/ComControl/ui_range.c)。Qt 参照：[ForesterSpeed1.qml](file:///home/xfy/CarHml_XIAO/qml/SpeedDial/ForesterSpeed1.qml)、[IconRange.qml](file:///home/xfy/CarHml_XIAO/qml/ComControl/IconRange.qml)。

步骤：
1. 核实车速原始单位、Qt 公英制指针公式、刻度表和取整规则；预期结果从 Qt 独立计算，不能用待测函数生成预期。
2. 将页面写死的 `false` 改为实际单位状态，保留协议层的 km/h，不在多个层重复换算。
3. 保存并更新单位标签对象，修正英里模式指针映射、普通/高亮刻度文字。
4. 修正缓存条件：速度落在同一刻度区间但单位发生变化，也必须刷新；当前仅比较 `highlight` 会漏掉单位切换。
5. 续航先判断 `0xFFFF`，显示 Qt 对应不可用占位，再对有效值进行单位换算；里程、续航、顶部巡航的单位口径一起核对，避免页面内混用。
6. 单位字段未有效时使用明确默认值，不能凭未初始化内容切换单位。

用例：
- 固定车速 0、40、100 及每个实际刻度阈值两侧；公制→英制→公制，数值输入不变。
- 当前刻度高亮不变时切单位，验证文字、单位和指针全部更新；高亮刻度也必须用当前单位文本。
- 续航 0、1、100、65535，公英制各一遍；65535 始终是占位，不能换算成巨大英里值。
- 协议帧只改变单位、不发新车速，UI 仍正确更新。

验收：组件断言和页面同帧截图均通过；不以“英里标签变了”替代刻度和指针一致性。

#### B2：短期平均油耗与油量阈值

文件：[page_short_term.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/PageModel/page_short_term.c)、[fuel_forester1.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/FuelTank/fuel_forester1.c)。

步骤：
1. 将单位选择与数值有效性分支分开，避免零值/特殊值强制切回 `L/100km`。
2. 对照 Qt 保留特殊值的显示语义，特别是倒数单位在零值时不能直接除零。
3. 油量红色边界按 Qt 的 `≤25%` 修正，同时更新旧文档的 `<25%` 描述。
4. 不顺带修改外温 0℃等已有 Qt 行为；发现疑问列入待确认表。

用例：油耗原始值 0、1、1200、1201 与 L/100km、km/L、mpg 的交叉组合；保持数值不变只切单位；油量 24%、25%、26% 来回切换，检查红白刻度及缓存。

验收：数值特殊值处理与单位切换互不干扰；油量 25% 为红色。

#### B3：补齐 Qt 通用警告事件入口

文件：[advance_parse.cpp](file:///home/xfy/CarHml_XIAO/src/advance_parse.cpp)、[Handler.js](file:///home/xfy/CarHml_XIAO/handlers/Handler.js)、[main.qml](file:///home/xfy/CarHml_XIAO/qml/main.qml)、[GeneralTipsWindow.qml](file:///home/xfy/CarHml_XIAO/qml/TipsWindow/GeneralTipsWindow.qml)、[ov_tips.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/overlays/ov_tips.c)。

步骤：
1. 画清两条链：`0x16/0x1C → messageNotify → showBriefly` 与现有 `0x03 → popup`；核实真实 payload 布局、ID、时长单位，不猜偏移。
2. 核实 Qt 实际调用是否传入时长。当前审查发现 Qt 该入口使用默认 2 秒，不能因为协议存在 duration 就擅自更改为协议时长。
3. 为警告建立独立事件状态，不复用最后一条 `high_func.raw`，不通过 UI 刷新间隔伪造收帧间隔。
4. 明确同 ID 的新事件、不同事件排队、重复事件去重、队列容量和满队列处理规则。先记录 Qt 实际行为，再决定最小有界实现。
5. 若要求保留同一刷新周期内的多条警告，不能只在快照中保存最后一条消息和一个序号；需要受控的事件队列，并沿用现有线程同步边界。
6. 不混淆两种入口的语义：保留现有 `0x03` 约定，除非用户确认修改。两个入口同时到来时，明确顺序和去重依据。
7. 为设置页/媒体遮挡留出已有状态接入点；尚未实现的功能明确标为后置，不能用硬编码“永不显示”掩盖缺口。

用例：ID 23；相同 ID 连续两次真实事件；一次事件重复 apply；两个不同 ID 在一次 UI 刷新前到达；到期隐藏；其他 `0x16` 子帧插入；`0x03` 与 `0x1C` 交错。每例先写预期队列顺序和显示时长，再实施断言。

验收：原 Qt 可显示的目标警告可从真实解析链进入 LVGL；没有旧快照重弹、无关子帧清除、事件静默丢失。队列规则有记录和测试。

### C. 事件生命周期与整页联动回归

#### C1：胎压计时跨样式切换

步骤：
1. 新增实际调用 `ui_app_set_style()` 的页面级测试，不仅是覆盖层对象测试。
2. 场景一：t=0 新异常帧；t=4s 切到占位样式；t=6s 切回。预期仍按原事件截止时刻关闭，不能重新获得完整 10 秒。
3. 场景二：异常已显示并到期；切走再切回，无新帧。预期不弹出。
4. 场景三：切走期间收到新的相同异常帧；返回时只根据最新事件剩余有效期显示。
5. 场景四：切走期间收到正常帧；返回时不显示。
6. 计时/事件消费状态应放到能跨样式存活的合适层，或使用同一单调时钟域的事件时间戳；不能把 LVGL tick 与串口后台时钟直接相减。
7. 明确“首次打开已有异常快照”“断线重连”“进程重启”的策略，避免序号重置造成错误续期。不要给协议解码层引入 LVGL 依赖。

验收：以上场景通过；已有单对象的超时、相同新帧续期、遮挡和 tick 回绕用例仍通过。

#### C2：覆盖层组合与中心卡片恢复

先以当前约定 `车门 > 胎压 > 巡航 > 雷达` 为验收基线。Qt 原实现不完全相同，若要求改变必须先确认，不能边测试边改预期。

| 场景 | 输入/操作 | 预期 |
|---|---|---|
| 单层显示 | 分别只激活门/胎压/巡航/雷达 | 目标层显示，中心卡片隐藏 |
| 多层竞争 | 门与胎压/巡航/雷达依次组合 | 只显示当前优先级最高的互斥层 |
| 高层退出 | 关门；正常胎压帧或计时到期；关闭巡航 | 恢复仍满足条件的下一层，不残留遮挡 |
| 完全退出 | 清除全部覆盖层条件 | 恢复之前的卡片，而不是跳回第一张 |
| 提示叠加 | 覆盖层显示时触发燃油低/通用提示 | 提示按约定叠加，不错误改变中心卡片状态 |
| 门与档位 | 门开，P→D→R→P | P挡提示条随条件变化，不残留 |
| 雷达方向 | D/M/R/P与前后目标组合 | 按已核实Qt分支选择方向，P挡等行为先核实 |
| 巡航子帧交错 | 0x0E→0x04→0x1C→0x0E清零 | 车道保留至明确清零，提示事件独立 |
| 数据不变 | 连续多次 apply 同一快照 | 显示不变，不重置事件计时或动画 |

补充检查：左右盲区、偏移车道、前车、跟车距离的全部有效枚举；Qt 的左右车道线 style==3 开启条件与现有简化规则列为“需确认差异”，不暗中扩展。

#### C3：状态保留、卡片交互与资源生命周期

步骤：
1. 警示灯补齐“25亮→4亮→25灭”的UI断言；验证全局OFF、全局ON、局部覆盖的顺序，以及两个故障码共用图标时的选图优先级。
2. 虚拟时钟检查快闪600ms/慢闪6000ms完整周期，重复apply不得把闪烁相位不断重置。
3. 对已有10张中心卡片按实际注册表逐一访问，记录ID/名称；左右切换、边界循环、覆盖层退出恢复、样式往返均检查。
4. 检查档位、车速、油量、里程等持久状态在样式切换后立即恢复；一次性提示不得仅因重建而重放。
5. 连续完成100次样式往返，检查对象/定时器数量和分配趋势；区分首次懒加载的固定资源增长与持续增长。
6. 执行正常关闭、重新初始化、再显示；只处理本任务涉及的普通UI生命周期问题，不顺带改造线程架构。

验收：状态不丢、一次性事件不误重放、无重复定时器，资源使用在预热后趋于稳定。没有检测工具时不得宣称“无内存泄漏”。

### D. 同状态视觉复刻与素材/字体收尾

#### D1：建立 Qt 与 LVGL 可比截图

步骤：
1. 为每个固定场景生成语义清单：状态数值、单位、主题、卡片、灯态、覆盖层、时钟、动画相位、缩放和分辨率。
2. Qt 和 LVGL 使用同一语义清单；Qt 可从现有入口设置状态，不修改参考源码。若原 Qt 环境跑不起来，记录原因，只能做源码坐标核对，不能把推导图冒充参考截图。
3. 统一1920×720设计坐标与窗口客户区，排除窗口边框、系统缩放和PC切换按钮对比较的干扰。不能把两张图任意拉伸到看似一致。
4. 记录 Qt 0.975 与 LVGL 250/256 的缩放差异。先解释全局偏差，再决定是否需要更准确的缩放方案，不能逐个控件补偿同一个全局误差。
5. 每个场景输出Qt图、LVGL图、半透明叠加图和差异图；动态区域只有明确记录后才能屏蔽。
6. 将截图和结果放在隔离的验收产物目录，记录生成命令；不要把构建目录/大量截图未经许可提交仓库。

建议场景：正常主仪表；每张中心卡片；档位P/R/N/D/M；车门六个位分别和组合；胎压正常/异常；雷达前/后各颜色；巡航距离0～4、左右偏移、盲区、前车；通用提示代表ID；公英制。

#### D2：按差异类别逐项修正

顺序：
1. 页面变换/父坐标/裁剪范围。
2. 图层遮挡、透明通道、资源选图与图片尺寸。
3. 关键数字、刻度、标签的基线、间距、对齐。
4. 字体、中文、字重和换行。
5. 已确认要恢复的动画节奏；板端性能不达标时保留已批准简化，不牺牲警示灯硬闪。

每次只修一类差异，保留修改前后同场景截图。当前用户调整过的巡航坐标必须先与参考变换链核对，不能仅根据单张视觉印象改动。

建议视觉门槛（作为新方案门槛，实施前确认）：关键位置在最终像素坐标中偏差不超过2px；没有缺图、截字、重叠和错误显隐；状态文本、单位、颜色语义完全一致。文字抗锯齿/字重差异单列，不用“整图相似度”掩盖关键告警缺失。

#### D3：中文字体与资源可重复生成

步骤：
1. 先确认是否本轮要求恢复中文；若仍允许英文占位，记录豁免，不将英文截图标成中文复刻完成。
2. 要恢复中文时，从已复刻QML文案、提示ID、单位和数字提取字形集合；确认字体授权和字重，生成最小子集，避免直接引入完整大字库。
3. 同时检查PC/BOARD字体配置、字号和换行；不只改文案而忽略容器尺寸。
4. 原图来自已有Qt素材；记录裁切、缩放、旋转和预合成参数。对仅有产物没有脚本的素材补齐必要的确定性生成步骤，不重新生成全部无关素材。
5. 使用现有 [png2bin.py](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/tools/png2bin.py) 生成bin32/bin16，核对路径、尺寸、透明通道和实际资源加载。
6. 旋转/缩放图片按现有RAM驻留机制处理；不要把需要变换的图直接交给仅支持流式读取的路径。

验收：目标文案无缺字；两种像素格式无黑边/错色/缺图；资源可由记录的输入重复生成。字体/动画尚未完成时明确登记，不影响已通过的逻辑验收。

### E. 板端编译、台架验证与发布门槛

#### E1：区分BOARD配置与真正交叉编译

当前已有BOARD缓存指向AArch64 GCC，但这不证明目标硬件就是AArch64。接手者先确认目标架构、libc、动态加载器、显示格式和串口参数，再选择工具链。

下面命令仅适用于已确认使用当前AArch64工具链的目标；若工具链不存在或目标不同，应更换经过确认的配置，不猜测路径。

```bash
cmake -S . -B /tmp/lvgl_two_board_acceptance \
  -DCARMW_PLATFORM=BOARD -DCARMW_BUILD_TESTS=OFF \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=/home/xfy/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j4
file /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo
```

检查：ELF架构、运行库依赖、BOARD宏、bin16路径、所需字体、没有链接SDL2。不能只加 `CARMW_PLATFORM=BOARD` 用宿主编译器构建，就宣称交叉编译完成；不在宿主机直接运行目标板二进制。

#### E2：隔离台架运行

前提：用户提供目标板访问、已确认资源路径和隔离台架环境。没有硬件/权限则记录阻塞，不补写结果。

步骤：
1. 部署可执行文件和bin16资源，保留上一版及对应资源，记录回退方法；不覆盖未知系统文件或修改开机服务。
2. 确认 `/dev/fb0` 分辨率、stride、像素格式；确认串口设备和波特率，不能把PC假帧路径带入生产入口。
3. 先验证无数据启动，再用台架/内存回放的正常业务状态验证接收与显示；合成回放必须标注为台架数据，不记为车辆实测。
4. 执行B/C核心矩阵：灯态增量、胎压超时、子帧交错、单位切换、提示入口、覆盖层恢复。
5. 执行正常断开与重连，记录界面实际行为；失联提示/过期数据策略如未定义，列为待确认，禁止自行把未知数据归零表示“正常”。
6. 检查停止顺序：UI定时器停止→后台线程退出→关闭串口→销毁UI；UI线程不直接执行串口pump/write。

#### E3：性能与资源

- 分别记录空闲、正常主仪表、雷达、巡航、中文长提示等场景的刷新耗时、CPU、内存和资源加载峰值。
- 33ms是当前刷新周期，不等于实际达到30FPS；区分apply耗时、渲染耗时、显示提交耗时，报告平均值及P95/最大值。
- 建议初始门槛：UI工作耗时P95不超过33ms；若目标硬件达不到，提交实测和瓶颈后确认降级策略，不能默默把门槛改成已测值。
- 检查数据不变时不重复重设文本/样式、不每帧分配；检查警示硬闪不被大图解码阻塞。
- 100次样式往返和固定场景循环后，预热后的内存/定时器数量无持续增长；说明测量方式和工具局限。

### F. 后置阶段6：仅在前述闭环后按用户确认实施

1. **设置体系**：先列设置项、默认值、持久化和协议/本地设置优先级，再做单位、明暗、卡片可见性；测试重启恢复及状态不变时设置即时生效。
2. **提示/覆盖层开关**：雷达、盲区、紧急制动、右舵和设置页遮挡，分别对照Qt；所有卡片隐藏时须有确认的退路。
3. **真实媒体/连接**：现有演示卡片不等于CarPlay/ZLink可用。先拿到实际SDK/API、硬件和授权，再对接状态；没有条件只报告界面演示完成。
4. **升级流程**：先确认平台接口、升级包校验、失败恢复与用户交互，不凭Qt界面臆造可用升级后端；本方案不安排自动执行真实升级。
5. 每项独立验收，不将阶段6缺失混入Style1基础逻辑修复，也不未经确认扩大工作范围。

---

## 12. 测试分层与交付记录

### 12.1 最小测试矩阵

| 层级 | 必测项 | 推荐现有落点 | 不代表什么 |
|---|---|---|---|
| 数据层 | 灯态增量/全局顺序、胎压sequence、独立lane、单位/警告解码 | test_decode / test_state | 不证明界面实际显示正确 |
| 组件层 | 文本、指针角、显隐、计时、重复apply、单位缓存 | test_overlays及必要的组件测试 | 不证明像素渲染正确 |
| 页面层 | 覆盖优先级、卡片恢复、样式往返、事件不重放 | 现有无窗口测试扩展 | 不证明SDL/fbdev正常 |
| 协议到UI | 正常业务帧→内存端口→状态→UI | 现有内存端口/链路测试扩展 | 不证明真实串口正常 |
| 视觉层 | 同状态Qt/LVGL截图、差异图、中文/资源 | 固定场景运行 | 不证明真实硬件性能 |
| 板端层 | bin16、fbdev、线程、串口、性能、退出 | 隔离台架 | 不等于已获实车使用验证 |

### 12.2 每个任务的回填模板

完成一个任务就在本节追加一条记录，不另建大量重复文档：

```text
任务ID：
状态：未开始 / 实施中 / 已实现待验证 / 验证通过 / 阻塞
本轮目标和实际改动：
Qt依据（文件链接、函数/分支）：
修改文件：
测试输入与预期：
实际执行命令及退出码：
测试结果（套件/断言，不只写“通过”）：
视觉证据路径（如适用）：
板端环境与实测（如适用）：
已知差异/未验证项：
后续依赖与下一步：
```

#### A1 执行记录（2026-11-04）

```text
任务ID：A1
状态：验证通过
本轮目标和实际改动：确认环境并执行强制清理构建，复核现有两个测试套件，再启动程序确认资源可加载。未改业务代码。
Qt依据：不适用（基线任务）
修改文件：无
测试输入与预期：PC 干净构建 0 错误、项目代码 0 警告；carmw / carmw_overlays 两个套件全部通过；程序在 X11 会话下启动无资源缺失。
实际执行命令及退出码：
  cmake -S . -B /tmp/lvgl_two_pc_acceptance -DCARMW_PLATFORM=PC -DCARMW_BUILD_UI=ON -DCARMW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug  -> 0
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j4                                                                                         -> 0
  ctest --test-dir /tmp/lvgl_two_pc_acceptance -N                                                                                                     -> 0（列出 carmw、carmw_overlays）
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure                                                                                     -> 0（2/2 通过）
  timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo（cwd=项目根）                                                                                -> 124（正常运行至超时）
测试结果（套件/断言，不只写"通过"）：carmw 1605 checks / 0 failure；carmw_overlays 84 checks / 0 failure。构建日志 warning 总数 108，全部来自 third_party/lvgl 的 vendor 代码（gltf shader），本项目源文件 warning 数为 0。
视觉证据路径（如适用）：无（本轮不做像素验收）
板端环境与实测（如适用）：仅编译验证，见 E1；本轮未上台架
已知差异/未验证项：
  - SDL_VIDEODRIVER=dummy / offscreen 无法创建渲染器或窗口，不能用于无窗口截图；有 X11 会话时必须用 DISPLAY=:0。
  - 未做 Qt/LVGL 同状态截图对比（属 D1）。
后续依赖与下一步：A2、B1。
```

#### A2 执行记录（2026-11-04）

```text
任务ID：A2
状态：验证通过
本轮目标和实际改动：
  1. 修正 PC 演示警示灯轮播：换码时先按上一盏的码发 OFF(0)，再发当前码 ON/快闪，避免灯态按码持久导致旧灯常亮。
  2. 新增 PC 专用固定场景入口 --scenario=<id>（main.c 内，位于 !CARMW_PLATFORM_BOARD 分支，板端不编译）：固定车速/转速/档位/单位/时钟/车门/胎压/雷达/巡航/提示/媒体/互联/灯组/卡片索引，关闭自动轮播与真实系统时钟；场景模式不创建右上角样式切换按钮。
  3. 场景启动后按 scenario->card 次数向 ui_app_handle_key 发 UI_KEY_NEXT，直接切到目标中心卡片。
Qt依据：不适用（演示/验收基础设施）
修改文件：main.c
测试输入与预期：23 个稳定场景 ID 均可启动；同一场景每 33ms 重发同一组固定帧，数值与图层不随时间变化；未知 ID 打印可用列表并以非 0 退出；无参数时保持原动态演示。
实际执行命令及退出码（cwd=项目根，DISPLAY=:0）：
  timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo --scenario=normal        -> 124，0 个资源打开失败
  timeout 5 ... --scenario=unit_mile                                               -> 124，0 个资源打开失败
  timeout 5 ... --scenario=no_data                                                 -> 124，0 个资源打开失败
  timeout 5 ... --scenario=card_music                                              -> 124，0 个资源打开失败
  timeout 5 ... --scenario=card_connect                                            -> 124，0 个资源打开失败
  timeout 5 ... --scenario=lamps_left                                              -> 124，0 个资源打开失败
  timeout 5 ... --scenario=overlay_radar                                           -> 124，0 个资源打开失败
  timeout 5 ... --scenario=tips_warning                                            -> 124，0 个资源打开失败
  /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo --scenario=bogus                   -> 1（打印 unknown scenario 与全部可用 ID）
  timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo（无场景）                 -> 124，0 个资源打开失败
测试结果（套件/断言，不只写"通过"）：场景入口本身不改变单元测试；全量 ctest 仍 2/2 通过，carmw_overlays 84 checks / 0 failure。
视觉证据路径（如适用）：未截图（本轮不要求）；场景 ID 供 D1 直接复用。
板端环境与实测（如适用）：BOARD 构建通过，场景代码未进入板端（s_scenario/scenario_* 均在 !CARMW_PLATFORM_BOARD 内）。
已知差异/未验证项：
  - 场景为"每帧重放固定帧"，0x03 提示与 3s 持续时间因此在场景中保持常显；静态截图可用，但动态超时行为仍需普通演示验证。
  - 尚未建立"重复执行像素级一致"的自动比对，重复一致性目前由固定帧+固定时钟在人工层面保证。
后续依赖与下一步：D1 用场景 ID 采集 Qt/LVGL 同状态截图；C 系列可复用该入口做页面级回归。
```

#### B1 执行记录（2026-11-04）

```text
任务ID：B1
状态：验证通过
本轮目标和实际改动：
  1. speed_forester1 保存并维护单位标签对象；指针角度按 Qt 双分支公式实现（公制 0.75°/kph；英里 ≤129 用 0.6214×0.75，>129 分段斜率 0.6214×1.5）。
  2. 高亮刻度阈值改为照抄 Qt 的严格开区间（含 220/260 档位跳变与 246~276、6~16 等空档），公制/英里都用 scaleSpeed 判定。
  3. 缓存条件由仅比较 highlight 改为 (highlight, mile)，使"速度不变只切单位""高亮索引不变只切单位"都会重刷刻度文本、字体与指针。
  4. 页面层用 st->units.valid && st->units.mile 取单位，单位帧未到达时按公制；speed/mileage/range/cruise/itinerary 全部统一。
  5. 续航 0xFFFF 先判占位（"- - KM" / "- - MI"，不做换算），有效值英里按 km/1.60934 保留 1 位小数。
  6. PC 演示补 0x39 单位帧（约 10s 公英制切换），此前完全没有该帧。
Qt依据：
  - ForesterSpeed1.qml：指针 angle 两分支、scaleSpeed、14 个 Label 的高亮严格不等式、单位文本 "mph"/"km/h"、英里刻度文本表。
  - IconRange.qml：remainingMile===1 ? 65535 ? "- -" : (remainingMile/1.60934).toFixed(1)，单位 " MI"/" KM"。
修改文件：src/ui/SpeedDial/speed_forester1.h、src/ui/SpeedDial/speed_forester1.c、src/ui/ComControl/ui_range.h、src/ui/ComControl/ui_range.c、src/ui/PageModel/page_subaru_forester1.c、src/ui/ComControl/ui_cruise.c、src/ui/PageModel/page_itinerary.c、tests/test_overlays.c、main.c
测试输入与预期：
  - 公制 100 → highlight 5、labels[5]="100" 且字号 LG、unit="km/h"、角度 -1200。
  - 同车速 100 只切英里 → highlight 3、labels[3]="60"、labels[5]="90"、unit="mph"、角度 -1484。
  - 切回公制全部恢复。
  - 车速 3 公英制 highlight 都是 0，但 labels[5] 由 "100" 变 "90"（缓存含单位）。
  - 边界 5/6、25/26、250、276、277（空档不高亮，277 命中末档 "280"）。
  - 英里 130 → 角度 -1344、highlight 4。
  - 续航 100 KM → 62.1 MI → 0.6 MI → 0 KM → 65535 "- - KM" → 65535 "- - MI"（占位不换算）。
实际执行命令及退出码：
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j4                    -> 0（项目 warning 0）
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure                -> 0（2/2）
  /tmp/lvgl_two_pc_acceptance/carmw_overlay_tests                                 -> 0（84 checks / 0 failure）
  /tmp/lvgl_two_pc_acceptance/carmw_tests                                         -> 0（1605 checks / 0 failure）
  aarch64 BOARD 构建（CMAKE_C_COMPILER=gcc-linaro-7.4.1 aarch64-linux-gnu-gcc，--clean-first）-> 0（项目 warning 0）
测试结果（套件/断言，不只写"通过"）：新增 test_speed_units / test_range_units 全部断言通过；LVGL 会把 image rotation 归一化到 0~3600，测试用 needle_rot() 还原为有符号角后再比较。
视觉证据路径（如适用）：未截图（属 D1）
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - 指针角度按整数近似（1/10000 与 0.1° 单位），与 Qt 浮点结果在 0.1° 量级上可能存在舍入差，未做逐点比对。
  - PageMusic/PageConnect 等无单位需求卡片未纳入本轮。
后续依赖与下一步：D2 做同状态视觉比对时重点看英里刻度文本与指针；B2 处理短期油耗与油量 25% 边界。
```


#### B2 执行记录（2026-11-04）

```text
任务ID：B2
状态：验证通过
本轮目标和实际改动：
  1. page_short_term 把"单位选择"与"数值有效性"拆开：单位标签只由 units.valid ? fuel_unit : 0 决定，avg<=0 / >1200 的特殊值不再把单位强制回 L/100km。
  2. 特殊值保留 Qt 语义：avg<=0 显示 "0"；avg>1200 显示 "--.-"；两者都不进入倒数分支，避免零值除零。
  3. 小计里程/平均时速同样改用 units.valid && units.mile，单位帧未到达时按公制。
  4. fuel_forester1 低油量判断由 "<25" 改为 "<=25"，与 Qt 的 "fuelPercentage > 25 ? 白 : 红" 一致；头文件注释同步。
Qt依据：
  - PageShortTerm.qml 147~190 行：avgFuel<=0 → "0"；avgFuel>1200 → "--.-"；case0/1/2 分别为 avg/10、100/(avg/10)、282.481/(avg/10)；单位文本由 fuelType 单独决定，与值是否有效无关。
  - ForesterFuelTank1.qml 第 25 行：source: fuelPercentage > 25 ? fuel1_white_tick : fuel1_tick。
修改文件：src/ui/PageModel/page_short_term.c、src/ui/FuelTank/fuel_forester1.c、src/ui/FuelTank/fuel_forester1.h、tests/test_overlays.c、docs/UI_REWRITE_HANDOFF.md（本记录 + 阶段 1 表格 "<25%" 旧描述）
测试输入与预期：
  - 油量 26 → 白图；25 → 红图（边界本身即红）；24 仍红且液面 24；26 回白；25 再红（缓存只在该换图时换一次）。
  - avg=0：三种 fuelType 都显示 "0"，单位标签分别为 L/100km / km/L / mpg。
  - avg=1：L/100km "0.1"、km/L "1000.0"、mpg "2824.8"（倒数单位不除零）。
  - avg=1200：L/100km "120.0"、km/L "0.8"、mpg "2.4"。
  - avg=1201：三种单位都 "--.-"，单位标签保持当前 fuelType。
  - units.valid=false 时即使字段残留 mile=true、fuel_unit=2，也按公制显示 "120.0"、"100 km"、"50 km/h"、"L/100km"。
实际执行命令及退出码：
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j$(nproc)      -> 0（项目 warning 0；108 条 warning 全在 third_party/lvgl）
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure          -> 0（2/2）
  /tmp/lvgl_two_pc_acceptance/carmw_overlay_tests                           -> 0（176 checks / 0 failure，含 B3 用例）
  /tmp/lvgl_two_pc_acceptance/carmw_tests                                   -> 0（1630 checks / 0 failure）
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j$(nproc)     -> 0（aarch64，项目 warning 0）
  DISPLAY=:0 timeout 6 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo（cwd=项目根）-> 124（正常运行至超时，无资源缺失）
测试结果（套件/断言，不只写"通过"）：新增 test_fuel_threshold / test_short_term 全部断言通过；B2 与 B3 用例共用同一套 ctest 入口。
视觉证据路径（如适用）：未截图（属 D1）
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - Qt 的 fuelType 来自设置模块，本工程来自 0x39/units 帧；units.valid=false 时按公制是本工程兜底约定，Qt 无"单位未有效"状态。
  - 值格式化仍按整数 x10 近似，与 Qt toFixed 的浮点舍入在极端值处可能有 0.1 级差异；本轮覆盖 0/1/1200/1201。
后续依赖与下一步：B3（见下一条记录）。
```

#### B3 执行记录（2026-11-04）

```text
任务ID：B3
状态：验证通过
本轮目标和实际改动：
  1. 数据层新增独立事件队列 car_warn_events_t（容量 8）：0x1C 子帧从 data[1..2] 大端取 textID（0xFFFF 归零）入队，seq 单调递增；满队列挤掉最旧并累加 dropped。不复用 high_func.raw，不用 UI 刷新间隔伪造收帧间隔。
  2. 解码：car_decode_high_func 增加 data[0]==0x1C && len>=4 分支；其他 0x16 子帧不产生警告事件。
  3. ov_tips 改为统一 FIFO 播放队列（容量 4，满时挤最旧）：0x1C 事件与 0x03 新帧都入同一条队列，不去重，严格先入先播；首次 apply 只记录 warn_seen_seq，不重放界面创建前的事件；同一快照重复 apply 不重播。
  4. 时长：所有 0x1C 警告固定 2000ms（Qt showBriefly(id) 未传 durationS）；0x03 仍沿用既有 duration_s（0 则 2000ms）。
  5. 24/25/26 按 Qt main.qml 隐藏正文、显示红色 FAULT（Qt 中文"故障"，按项目约定改英文）。
  6. 同一快照里两通道并存时，先补 0x1C 事件、再判定 0x03 新帧（单份快照无法还原真实到达先后）。
Qt依据：
  - advance_parse.cpp case WarningInfo(0x1C)：textID=(data[1]<<8)|data[2]，0xFFFF→0，showTime_s=data[3]。
  - Handler.js case 0x1C → GlobalData.messageNotify(textID, showTime_s)。
  - main.qml onMessageNotify(msgId,duration)：只用 msgId 做 24/25/26 与 show4h(id 32) 判断，随后 xiaotips.showBriefly(msgId)，不传 duration。
  - GeneralTipsWindow.qml：timeMs=(durationS&&durationS>0)?durationS*1000:2000；msgQueue.push；isRunning 时只入队，否则 processNext；队列无界、不去重、严格 FIFO；textshow=false 时正文隐藏、显示红字辅助行。
修改文件：include/carmw/car_state.h、src/car_state.c、src/car_decode.c、src/ui/overlays/ov_tips.h、src/ui/overlays/ov_tips.c、tests/test_decode.c、tests/test_overlays.c
测试输入与预期：
  - 解码：0x1C 00 17 05 → textID 23 / seq 1；FF FF → textID 0；3 字节短帧不产生事件；压满 9 条后 count=8、dropped=1、head=1、items[0].seq=9。
  - UI：界面创建前的事件首次 apply 不显示；插入 0x0E lane 子帧不吞事件；同 ID 连续两次都入队；同一快照重复 apply 不重复入队；每条 2s，按 FIFO 依次播放、到期隐藏。
  - 24/25：正文隐藏、FAULT 显示，两条依次播放。
  - 一次到达 6 条：播放队列挤掉最旧 2 条，从第 3 条开始显示。
  - 0x03+0x1C 同快照：先显示 0x1C(27)，再显示 0x03(23, duration_s=1 → 1s)。
  - 端到端：编码 0x16/0x1C 帧经内存端口 → parser → registry → decode → store 快照 → ov_tips 显示/到期。
实际执行命令及退出码：同 B2 记录的 6 条命令；ctest 2/2，carmw 1630 checks / 0 failure，carmw_overlays 176 checks / 0 failure。
测试结果（套件/断言，不只写"通过"）：新增 test_decode 0x1C 队列断言、test_tips_warn、test_warn_end_to_end 全部通过；既有 tire/cruise/speed/range 用例保持通过。
视觉证据路径（如适用）：未截图（属 D1）
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - Qt msgQueue 无界且不去重；本工程播放队列有界 4 条（满时丢最旧并计 queue_dropped），事件源队列有界 8 条。极端突发会丢最旧，不做去重，与 Qt "全保留"不同。
  - id 32 的 SettingsMgr.show4h 门控已实现（14.8，0x1C 与 0x03 两条入口）。
  - 0x40/0x41 的 warnInfo 也走 messageNotify（0.5s/1s），但本工程未解码这两条命令，payload 未核实，本轮不接入。
  - 设置页/媒体页遮挡通用提示已实现（14.8，`ov_tips_set_obstructed()`）；遮挡只作用于通用提示面板，燃油横幅与 P 挡提示条保持原行为。
  - 跨通道真实先后无法从单份快照还原，统一按 0x1C 优先。
  - 0x03 的图标仍按 text_id 映射（既有约定，本轮未改）；Qt 的 iconID 未使用。
后续依赖与下一步：D1 视觉对比时核对通用提示面板 260×280 位置与红字行；C2/C3 页面级回归可复用 car_warn_events_t。
```

#### C1 执行记录（2026-11-04）

```text
任务ID：C1
状态：验证通过
本轮目标和实际改动：
  1. 把胎压异常的事件计时与"已消费帧序号"从覆盖层对象移到新的 ui_session 层：
     该层随 ui_app 初始化/反初始化清空，跨 ui_app_set_style() 的页面销毁重建存活。
  2. ui_app_apply() 在把快照转发给当前样式之前先更新 ui_session，因此样式切走期间
     （如切到 general1）仍按 33ms 快照持续消费新帧与到期，不会在返回时重新起算。
  3. ov_tire 拆成 ov_tire_event_apply（计时）与 ov_tire_apply（显隐），显隐仍由
     event.pending && !suppressed 决定；车门遮挡语义不变。
  4. 计时只用 LVGL tick（UI 线程自己的单调时钟）记录事件到达时刻，不与串口后台
     时钟相减；事件识别用 tires.sequence，不给解码层引入 LVGL 依赖。
  5. 生命周期策略：进程启动/UI 重新初始化时事件为空，首个异常快照按新事件显示；
     tires.valid=false 时清除进度，恢复后的首个有效帧按新事件处理，覆盖断线重连导致
     sequence 从 0 重启时可能与旧序号相同而漏判的情况。
Qt依据：TirePressure/PageMainTirePressure.qml 的 mainVisible 与 tireTimer=10000ms；
        计时不因页面重建而重启（Qt 定时器属于页面对象，本工程状态外置后语义等价）。
修改文件：
  src/ui/overlays/ov_tire.h、src/ui/overlays/ov_tire.c
  src/ui/ui_session.h、src/ui/ui_session.c（新增）
  src/ui/ui_app.c、src/ui/PageModel/page_subaru_forester1.c
  tests/test_overlays.c、CMakeLists.txt
测试输入与预期：
  - 单对象旧用例（改用 event_apply + apply 两步驱动）：超时、相同新帧续期、车门遮挡、
    正常帧关闭、tick 回绕后 9999ms 仍有效 / 10000ms 到期，全部保持原预期。
  - 新增页面级 test_tire_style_switch（实际调用 ui_app_init/ui_app_set_style）：
    场景一 t=0 异常 → 4s 切 general1 → 6s 切回 → 9.999s 仍有效、10s 到期；
    场景二 到期后切走切回无新帧 → 不显示；
    场景三 4s 切走、5s 收到相同异常新帧（切走期间仍消费）→ 返回后 14.999s 有效、
          15s 到期，不按返回时刻(6s)重新起算；
    场景四 切走期间收到正常帧 → 返回不显示。
实际执行命令及退出码（cwd=项目根）：
  cmake -S . -B /tmp/lvgl_two_pc_acceptance -DCARMW_PLATFORM=PC -DCARMW_BUILD_UI=ON -DCARMW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -> 0
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j4 -> 0
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure -> 0（2/2 通过）
  /tmp/lvgl_two_pc_acceptance/carmw_overlay_tests -> 0（204 checks / 0 failure）
  cmake -S . -B /tmp/lvgl_two_board_acceptance -DCARMW_PLATFORM=BOARD -DCARMW_BUILD_TESTS=OFF ... -DCMAKE_C_COMPILER=.../aarch64-linux-gnu-gcc -> 0
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j4 -> 0
  file /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo -> ARM aarch64 ELF
  DISPLAY=:0 timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo -> 124（正常启动至超时）
测试结果（套件/断言，不只写"通过"）：carmw 1630 checks / 0 failure；carmw_overlays
  204 checks / 0 failure（较 B3 的 176 增加 28 条，全部来自 C1 用例）。构建日志 warning
  总数 108，全部来自 third_party/lvgl，本项目源文件 0 警告。
视觉证据路径（如适用）：未截图（属 D1）
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - UI 显隐由 event.pending 派生，页面级用例断言的是会话事件状态；像素级显隐仍由
    组件层用例覆盖，未做真实截图。
  - 样式切走期间若连续多帧到达，只有最新一份快照的事件在本帧被消费（旧帧的新事件
    按快照粒度合并），与 33ms 刷新粒度一致，未逐帧排队。
  - 未接入真实串口；断线重连策略按"valid=false 清除进度"实现，真实 car_link 是否会
    在重连间隙发布一张 valid=false 快照尚未台架核实。
后续依赖与下一步：C2（覆盖层组合与中心卡片恢复），可复用本次页面级 ui_app 测试骨架。
```

#### C2 执行记录（2026-11-04）

```text
任务ID：C2
状态：验证通过
本轮目标和实际改动：
  1. 新增页面级 test_overlay_composition（经 ui_app_apply 真实快照分发，不直接调覆盖层）：
     定位页面根的直接子对象（中心卡片栈 553,140,814,509；车门 620,100,720,550；
     胎压 554,150,814,509；巡航 660,200,600,470；雷达 815,160,290,430；
     提示层 0,0,1920,720），按坐标+尺寸唯一定位，需先 lv_obj_update_layout。
  2. 覆盖层优先级基线保持 车门 > 胎压 > 巡航 > 雷达，未改实现：
     单层分别激活、四层同时竞争只显示车门、车门退出→胎压（异常事件仍有效）→
     正常帧退出→巡航→0x0E 清零退出→雷达，完全退出后恢复之前选中的卡片（非第一张）。
  3. 雷达方向：D 前波 / R 后波 / P 两波都不显示，退出后中心卡片恢复。
  4. 门与档位：门开 + D 显示 P 挡提示条，切 P 立即消失；提示层通用面板与
     P 挡提示条可叠加，且不影响中心卡片隐藏状态。
  5. 巡航子帧交错 0x0E→0x04→0x1C→0x0E：车道保持至显式清零，0x1C 警告面板独立
     2s 到期，不复位巡航层。
  6. 数据不变：连续 apply 同一快照不续期胎压事件（5s 后重复 apply ×3，仍按原
     10s 截止）。
  7. 组件级 test_cruise 扩充枚举：carState 0/1/2/3（居中/左偏/右偏）、showArea
     1/2/3（右/左/双盲区）、showCar=1、跟车距离 distanceShow 0~4 与跟车小车 y
     33/61/39/33/17、carState=2 时距离 4 仍不点亮、controlLed 单独点亮即可显示。
  8. 测试辅助改用 lv_obj_get_child(parent, i) 索引遍历（v9.5 无
     lv_obj_get_next_sibling），并在读取坐标前调用 lv_obj_update_layout。
Qt依据：
  PageRadar.qml 前波 geartext ∈ ["D","M1".."M6"]、后波 geartext==="R"；
  main.qml pageRadarOpen = displayOn（与档位无关，P 挡 root 可见但两波隐藏）；
  PageNavigationInfo.qml 的盲区/前车/偏移/距离显隐分支，与 ov_cruise.c 对照。
修改文件：tests/test_overlays.c（仅测试；本轮未改产品代码）
测试输入与预期：见“本轮目标和实际改动”1~8；全部断言预期值取自 Qt 分支条件，
  不用被测函数反推。
实际执行命令及退出码（cwd=项目根）：
  cmake -S . -B /tmp/lvgl_two_pc_acceptance -DCARMW_PLATFORM=PC -DCARMW_BUILD_UI=ON -DCARMW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -> 0
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j8 -> 0（项目 warning 0，日志 warning 总数 108 全为 third_party/lvgl）
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure -> 0（2/2 通过）
  /tmp/lvgl_two_pc_acceptance/carmw_overlay_tests -> 0（388 checks / 0 failure）
  /tmp/lvgl_two_pc_acceptance/carmw_tests -> 0（1630 checks / 0 failure）
  cmake -S . -B /tmp/lvgl_two_board_acceptance -DCARMW_PLATFORM=BOARD -DCARMW_BUILD_TESTS=OFF \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=.../aarch64-linux-gnu-gcc -> 0
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j4 -> 0
  file /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo -> ELF 64-bit LSB executable, ARM aarch64
  DISPLAY=:0 timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo -> 124（正常启动至超时）
测试结果（套件/断言，不只写"通过"）：carmw 1630 checks / 0 failure；carmw_overlays
  388 checks / 0 failure（较 C1 的 204 增加 184 条，来自 C2 页面级组合用例与
  test_cruise 枚举扩充）。PC/BOARD 构建均 0 错误，本项目源文件 0 警告。
视觉证据路径（如适用）：未截图（属 D1）
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - Qt 的盲区/紧急制动/车道偏离还受 SettingsMgr.blindSpotWarning /
    emergencyBraking / laneDepartureWarning 门控；本工程未接设置页，按"条件满足即显示"。
    列为需确认差异，未扩展门控。
  - PageNavigationInfo.qml 的左右车道线在 style==3 分支另有开启条件，本工程按
    carState=2/3 简化；同列为需确认差异。
  - 页面级断言基于对象显隐标志，非像素截图；巡航几何 660,200,600,470 以源码为准
    （ov_cruise.h 注释中的 830×470@440 是过时说明，未改注释）。
  - 0x03 弹窗快照在页面级用例中以 100ms 真实刷新粒度推进；同一快照 1s 内的
    重复 apply 不重播（与 ov_tips 的 POPUP_FRAME_GAP_MS 语义一致）。
后续依赖与下一步：C3（状态保留、卡片交互与资源生命周期）。
```

#### C3 执行记录（2026-11-04）

```text
任务ID：C3
状态：验证通过（含 1 处产品代码资源修复）
本轮目标和实际改动：
  1. 警示灯 UI 断言（组件级 ui_lamp_group / ui_lamp_row）：
     - 全局 OFF → 全局 ON → 局部 {码} 覆盖，再全局 OFF/ON，确认局部码不反向污染全局。
     - 25亮 → 4亮 → 25灭：两盏灯各自由独立故障码驱动，不互相清除。
     - 共用图标码 46/91 取较高灯态，单码清除不影响另一码。
     - 同档多图按灯表顺序选图；前图对应码熄灭后切到后图。
     - 灯排主码 15 与备码 229：备码非零时优先显示故障图，与灯态高低无关。
  2. 快闪/慢闪虚拟时钟：删除测试显示刷新定时器后仅用 lv_timer_handler 驱动灯定时器。
     快闪半周期 300ms（299ms 不变、300ms 灭、600ms 回到亮）；慢闪半周期 3000ms
     （2999ms 不变、3000ms 灭、6000ms 回到亮）。两次均在一相位内重复 apply 10 次，
     断言定时器实例指针与当前相位均不变。
  3. 10 张中心卡片按注册表顺序逐一访问（对象指针唯一性 + 左右边界循环）：
     0 导航车辆视图 / 1 4x4-i / 2 胎压 / 3 行程数据 / 4 底盘控制 /
     5 短期记忆 / 6 瞬时油耗 / 7 燃油经济性 / 8 多媒体 / 9 互联信息。
     第 10 次 NEXT 回到第 0 张，第 1 次 PREV 回到第 9 张；覆盖层退出恢复原卡片；
     样式往返后新页面从第 0 张开始，左右切换仍循环。
  4. 样式往返后档位 D、车速 137（刻度 140 放大）、油量 25%（刻度窗 100,266,130,210）、
     ODO 5000 KM、续航 300 KM 立即恢复；0x1C 一次性提示播放到期后往返，不再重放。
  5. 100 次 general1↔forester1 样式往返：冷启动/首次往返后/100 次后对象 436 个、
     定时器 3 个，数量保持不变。
  6. ui_app_deinit → 根对象/当前样式为空 → ui_app_init → set_style → apply → 中心栈可见，
     重复一遍关闭/初始化后仍可见。
产品代码修复：
  s1_destroy 未销毁音乐卡片柱状动画定时器（music_card_create 中创建的 lv_timer 不随
  LVGL 对象树释放），每次 forester1 页面重建泄漏 1 个暂停定时器；100 次往返实测由
  3 增至 113。已在 page_subaru_forester1.c 的 s1_destroy 中调用 music_card_destroy，
  修复后 100 次往返定时器数量保持 3。该修复属于 C3 资源生命周期范围，未改线程架构。
Qt依据：WarnIcon 灯表映射与 0.3s/3s 闪烁节奏、StackLayout 页签顺序、
        Main 页面字段持久性（QML 属性不因 Loader 重建而清零由状态层承接）。
修改文件：tests/test_overlays.c（新增 C3 用例与辅助）
          src/ui/PageModel/page_subaru_forester1.c（music_card_destroy）
实际执行命令及退出码（cwd=项目根）：
  cmake -S . -B /tmp/lvgl_two_pc_acceptance -DCARMW_PLATFORM=PC -DCARMW_BUILD_UI=ON -DCARMW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -> 0
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j8 -> 0
  /tmp/lvgl_two_pc_acceptance/carmw_overlay_tests -> 0（798 checks / 0 failure）
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure -> 0（2/2 通过，carmw 1630 checks / 0 failure）
  cmake -S . -B /tmp/lvgl_two_board_acceptance -DCARMW_PLATFORM=BOARD -DCARMW_BUILD_TESTS=OFF \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_C_COMPILER=.../aarch64-linux-gnu-gcc -> 0
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j8 -> 0
  file /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo -> ELF 64-bit LSB, ARM aarch64
  DISPLAY=:0 timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo -> 124（正常启动至超时）
测试结果（套件/断言，不只写"通过"）：carmw_overlays 798 checks / 0 failure（较 C2 的
  388 增加 410 条）；carmw_tests 1630 / 0；ctest 2/2。PC/BOARD 构建 0 错误，项目源文件
  0 警告（日志 108 条 warning 全部来自 third_party/lvgl）。
视觉证据路径（如适用）：未截图（属 D1）
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - 资源结论仅为“对象数 436、定时器数 3 在预热后不变”，未启用内存统计工具，
    不能据此宣称“无内存泄漏”；图片 RAM 驻留与 LVGL 堆分配未计量。
  - 卡片逐一访问以注册顺序和对象指针唯一性为准，名称写在本记录；除导航卡的
    660×340 几何外未逐卡断言内部文案/图形内容。
  - 样式往返会把中心卡片索引重置到第 0 张（当前实现未跨样式持久化页签选择），
    本轮按观察结果记录，未改语义。
  - 闪烁相位断言基于测试中的虚拟时钟与删除刷新定时器；真实显示刷新节奏未截图验证。
后续依赖与下一步：第三批 C1/C2/C3 已全部完成；按 13.2 进入第四批（D 视觉对比 /
  中文与素材收尾），先做 D1 固定场景与截图证据，不改已通过的灯态/覆盖层语义。
```

#### D1 执行记录（2026-11-04）

```text
任务ID：D1
状态：已实现待验证（截图与差异清单已产出；板端与中文/素材项未覆盖）
本轮目标和实际改动：
  同一固定状态下分别采集 Qt6 参考工程与 LVGL 复刻工程的主表盘整屏截图，
  生成并排图/混合图/差异图与像素统计，整理差异清单。
  约束执行情况：Qt 原工程 /home/xfy/CarHml_XIAO 保持只读（md5 与副本不同，
  原文件未改动），所有 Qt 侧补丁只打在 /tmp/qt_snap/CarHml_XIAO 拷贝上。
Qt依据（文件链接、函数/分支）：
  /tmp/qt_snap/CarHml_XIAO/qml/main.qml：debugModel 属性、Component.onCompleted、
  GlobalData.items.* 字段（速度/转速/档位/续航/ODO/时钟/胎压等）、loadingPage()。
修改文件：
  Qt 侧（仅 /tmp/qt_snap 副本，原工程只读）：
    qml/main.qml —— D1 截图补丁：
      a. debugModel=false（关调试面板/强制警告灯/真时钟）；
      b. Component.onCompleted 中非调试分支跳过开机视频，直接 loadingPage(true)；
      c. 窗口改不透明 #000000 并 raise/requestActivate（透明无边框窗口在
         Xwayland 上可能不被映射，曾导致截到纯黑）；
      d. 测试按钮 Flow 补 visible: debugModel，避免出现在正式截图；
      e. Component.onCompleted 末尾注入固定状态 20 项，对齐 LVGL normal 场景：
         speed=100, rpm=2000, geartext="D", remainingMile=520, odo=1122232,
         fuelPercentage=78, instantFuel=8.5, outsideTempCValue=22.0,
         hour/minute/second=12/34/0, waterTempPer=60, waterTemp=88,
         四轮胎压 240/245/238/243, setSpeed=80, 转向灯/巡航灯关闭。
      注：QML 一个对象只允许一个 Component.onCompleted，注入合并进已有
      回调，未新增第二处。
    snap.cpp + setting.cfg（[ActiveFlag] activeflag=true）+ make：
      qt_snap dummy <out.png> <delayMs> <grabMs> 抓帧工具，QT_QPA_PLATFORM=
      offscreen 下 grabWindow 全黑，实际用 DISPLAY=:0（Xwayland）运行。
  LVGL 侧（本仓库）：
    src/ui/ui_png.c / ui_png.h（新增）：ARGB8888 → 8bit RGBA PNG 编码器
      （zlib + 手写 IHDR/IDAT/IEND chunk），无第三方图片库依赖。
    src/ui/ui_snapshot.c / ui_snapshot.h（新增）：PC 分支用
      lv_display_get_buf_active() 直读整屏显示缓冲（DIRECT 模式下缓冲即
      整屏 ARGB8888）写 PNG。
    main.c：新增 --snapshot=<path> 参数与 SIGUSR1 信号，ui_loop 每帧检查
      请求后调用 ui_snapshot_save(lv_screen_active(), path)。
    CMakeLists.txt：PC 分支新增 ui_png.c/ui_snapshot.c 并链接 ZLIB::ZLIB。
测试输入与预期：
  固定状态：100km/h、2000rpm、D 档、续航 520km、油量 78%、水温 88、
  车外 22.0℃、时钟 12:34、胎压 240/245/238/243、ODO 1122232、ECO 80、
  巡航 80、瞬时油耗 8.5。
实际执行命令及退出码：
  # Qt 侧（副本 /tmp/qt_snap/CarHml_XIAO）
  cd /tmp/qt_snap/CarHml_XIAO && make -> 0
  DISPLAY=:0 QT_QPA_PLATFORM=xcb /tmp/qt_snap/CarHml_XIAO/build_snap/CarHml_XIAO \
    （经 snap.cpp 入口：qt_snap 延时 6000ms 抓帧，等待开机动画完成）-> 0
  产出 /tmp/d1_shots/qt_forester1.png（1920x720, 440311B）
  # LVGL 侧（cwd 必须是仓库根：LV_FS_STDIO_PATH="./assets"）
  DISPLAY=:0 SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    build/bin/carmw_ui_demo --scenario=normal --snapshot=/tmp/d1_shots/lvgl_normal.png \
    启动后向进程发 SIGUSR1 触发截图 -> 0
  产出 /tmp/d1_shots/lvgl_normal.png（1920x720, 392375B）
  # 对比（无 PIL/numpy，纯 python3+zlib 手写 PNG 读写，/tmp/lvgl_shot/compare.py）
  python3 /tmp/lvgl_shot/compare.py /tmp/d1_shots/qt_forester1.png \
    /tmp/d1_shots/lvgl_normal.png /tmp/d1_shots/d1 -> 0
测试结果（套件/断言，不只写"通过"）：
  两图均 1920x720。像素统计：diff pixels(>24) = 101572（7.35%），
  mean delta 122.9；差异最集中行带 y=360-440（约 30322 px，车身与
  ODO/续航底栏区域）。
视觉证据路径：/tmp/d1_shots/
  qt_forester1.png / lvgl_normal.png（原始截图）
  d1_side.png（上下并排）、d1_blend.png（50% 混合）、d1_diff.png（>24 红色高亮）
板端环境与实测（如适用）：未做（属 D2/E 批次）。
已知差异/未验证项（依 d1_side/d1_diff 判读）：
  1. 车身颜色：Qt 蓝色 / LVGL 白色。Qt 由 SettingsMgr.carColor 换肤，
     LVGL 中央车辆视图固定用白色车素材，未接颜色切换。
     ——已修复，见下方"D1 补充记录：车身颜色接线"（整屏差异 7.35%→5.59%）。
  2. 档位文本：Qt 显示 "D" / LVGL 显示 "D1"（D+档位序号）。
  3. ODO：Qt "ODO: 1122232 KM" / LVGL "ODO:" 后为空——normal 场景未发
     0x08 Mileage 帧，属截图输入差异，非渲染缺陷；补帧后需复采。
  4. 续航标签：Qt "续航里程： 520 KM"（中文）/ LVGL "RANGE: 520 KM"
     （英文），中文文案属 D 批次中文收尾范围。
  5. Trip：Qt 右下 "Trip A: 399.8 KM" / LVGL "Trip:" 后为空（同 ODO，
     场景未注入 Trip 数据）。
  6. 车外温度：LVGL 顶部有 "22.0℃" 标签，Qt 截图中该位置无——待核对
     Qt 侧 22.0℃ 的显示条件（可能依赖特定卡片或设置态）。
  7. 渲染细节：指针边缘抗锯齿、刻度/底栏渐变存在像素级差异（差异图
     两表盘边缘与文字均描红），属软件渲染 vs Qt 光栅化的预期差别。
  未验证项：以上均基于 PC 离屏截图；板端渲染、Qt 真机开机视频路径、
  中文文案最终方案（D 批次）均未覆盖。LVGL 侧 --snapshot 走信号触发，
  进程退出路径未单独复查（读显示缓冲方案下未观察到段错误；早期段错误
  出自已废弃的自建 layer 重绘方案）。
后续依赖与下一步：差异 1/2/3/5 转入 UI 修复清单（车身换肤接线、档位
  文本口径、场景补 0x08 Mileage 与 Trip 帧）；差异 4/6 属中文与显示条件
  核对，按 13.2 与 D2（中文/素材收尾）合并处理。
```

#### D1 补充记录：车身颜色接线（2026-11-04）

```text
任务ID：D1-车身颜色（对应上节"已知差异 1"）
状态：已实现并验证（PC 截图对比 + ctest），板端未验证
Qt依据（文件链接、函数/分支）：
  /tmp/qt_snap/CarHml_XIAO/qml/CentralDisplay/NavigationForester.qml
    L149-170：白色剪影 lun_mask + 隐藏 Rectangle{color: carColor} 经
    Blend{mode:"multiply"} 合成车身颜色；L17 property string carColor。
  /tmp/qt_snap/CarHml_XIAO/src/SettingsManager.cpp L74-78：设置项
    CarSetting/carColor 缺省 "#004a6b"。
  /tmp/qt_snap/CarHml_XIAO/qml/PageModel/PageSubaruForester1.qml L241：
    页面 carColor 绑定 SettingsMgr.carColor。
做法（不新增素材、不改层序）：
  lun_Color_mask 实测为白色剪影 + 全透明背景（RGB 全 255），因此对
  car_mask 设 LV_BLEND_MODE_MULTIPLY 并用 lv_obj_set_style_image_recolor()
  + image_recolor_opa=LV_OPA_COVER 染色，等价于 Qt 的
  "剪影 × 纯色 multiply"；剪影 alpha 决定染色范围，车身明暗由掩码灰度保留。
  车身颜色属本地设置项、不由串口帧产生，故不伪造协议帧，PC 侧用
  --car-color=#RRGGBB 启动参数覆盖视图字段。
修改文件（本仓库）：
  include/carmw/car_color.h（新增）：car_color_t {red,green,blue,valid}、
    car_color_parse()、car_color_default()。
  src/car_color.c（新增）：#RGB/#RRGGBB 解析（接受可省略 '#'，非法输入返回
    false）、缺省色 #004a6b。
  CMakeLists.txt：carmw 库源文件加入 src/car_color.c。
  include/carmw/car_state.h：car_state_t 末尾新增 car_color 字段
    （纯数据，保持结构体可拷贝）。
  src/car_state.c：car_state_init 在 memset 后设 car_color = car_color_default()。
  src/ui/PageModel/page_subaru_forester1.c：
    a. central_card_create 中 car_mask 设 LV_BLEND_MODE_MULTIPLY，
       初始 image_recolor_opa=LV_OPA_TRANSP（未收到颜色时保持素材原色）；
    b. 新增 car_view_apply_color()，仅在 RGB 变化时重设样式，避免每帧重绘；
    c. car_view_apply() 开头调用 car_view_apply_color(ui, &st->car_color)。
  main.c：新增 car_color_parse_args()（--car-color=#RRGGBB，非法值打
    stderr 并忽略）、s_car_color/s_car_color_set；main() 中解析参数；
    refresh_timer() 在 car_link_view() 与 ui_app_apply() 之间按
    CARMW_PLATFORM_BOARD 分支覆盖 view.state.car_color（板端不生效）。
实际执行命令及退出码：
  cmake --build build -j8 -> 0（本仓库源文件零 warning；仅 third_party/lvgl 原有告警）
  cd build && ctest --output-on-failure -> 0（2/2 passed：carmw、carmw_overlays）
  # 截图（cwd 必须是仓库根：LV_FS_STDIO_PATH="./assets"）
  DISPLAY=:0 SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    build/bin/carmw_ui_demo --scenario=normal --car-color=#004a6b \
    --snapshot=/tmp/d1_shots/lvgl_normal_color.png
  启动后向进程发 SIGUSR1 -> 0
  产出 /tmp/d1_shots/lvgl_normal_color.png（1920x720, 372769B）
  python3 /tmp/lvgl_shot/compare.py /tmp/d1_shots/qt_forester1.png \
    /tmp/d1_shots/lvgl_normal_color.png /tmp/d1_shots/d1c -> 0
测试结果：
  整屏 diff pixels(>24) 由 101572（7.35%）降至 77320（5.59%），
  mean delta 122.9 -> 115.6；差异行带列表中不再有单带 >1% 的集中区
  （上一版 y=360-440 车身+底栏集中带消失，剩余差异分散在刻度/指针/
  文字的描边上）。
  车身区像素抽样（x 760-1180, y 150-420，条件 b>50 且 b>r+25）：
    Qt          : 17860 px，主色 (0,74,107) = #004A6B，1687 px
    修改前 LVGL : 0 px（白色车身，无蓝色像素）
    修改后 LVGL : 18434 px，主色 (0,73,106)，1740 px
  主色逐通道差 1，来自 LVGL recolor/multiply 的 >>8 取整，肉眼不可辨。
  注：截图助手 /tmp/lvgl_shot/shot.py 的 cwd 为 /tmp/lvgl_run，资源路径
  不对时会产出"只有文字、无表盘与车辆"的无效截图（20598B），该结果已弃用，
  不作为证据。
视觉证据路径：/tmp/d1_shots/
  lvgl_normal_color.png（本次）、d1c_side.png（上下并排）、
  d1c_blend.png、d1c_diff.png（>24 红色高亮）
未验证项：
  1. 颜色切换只有 PC 启动参数入口，板端尚无设置界面/持久化（Qt 侧为
     SettingsManager + 设置页）；板端渲染未复核。
  2. GUI 内运行时改色未做交互入口（仅启动时注入）。
  3. car_color_parse 尚无独立单测用例，当前由编译 + 截图链路间接覆盖。
后续：差异 2/3/5（档位文本口径、场景补 0x08 Mileage 与 Trip 帧）仍待处理；
  差异 4/6 归 D2 中文与显示条件核对。
```

#### D2 执行记录（2026-11-05）

```text
任务ID：D2
状态：本轮已确认项完成并验证；中文/字体与像素级渲染差异未处理（按用户确认豁免至 D3）
用户确认范围（2026-11-05）：
  1. 中文文案保留英文占位，不把 D3 的中文字体子集提前，D3 前登记豁免。
  2. 本轮不要求截图证据，视觉差异由用户本人对比；因此 D2 只做可验证的
     状态/显示条件修正，不声称像素验收通过。
本轮目标和实际改动（按 D2 差异类别）：
  1. 状态文本口径（对应 D1 差异 2）：固定场景的 0x33 档位帧原发 'D'+'1'，
     显示为 "D1"；改为只发单字符档位，与 Qt D1 固定状态 geartext="D" 一致。
     未在 UI 层裁剪档位字符（协议语义允许 "D1"/"M1" 等组合，裁剪会破坏真实数据）。
  2. 状态输入缺失（对应 D1 差异 3/5）：固定场景未发 0x08 Mileage，导致 ODO/Trip
     为空。场景新增 0x08 帧：trip_type=1(TRIP A)、odo=1122232、
     tripA=3998(0.1km)、tripB=3222(0.1km)，与 Qt GlobalData.qml 默认值
     （tripType=1 / odo=1122232 / tripA=3998 / tripB=3222）一致，
     显示为 "ODO: 1122232 KM" 与 "Trip A: 399.8 KM"。
  3. 图层/显示条件（对应 D1 差异 6）：Qt IconTemperature 的可见性由
     SettingsMgr.outTemperature 控制，SettingsManager.cpp 中缺省读取
     OutTemperatureOnOff=false；LVGL 原先无条件显示 22.0℃。新增
     car_state_t.outside_temp_on（缺省 false，属本地设置映射，不由串口帧产生），
     页面创建时隐藏温度行、apply 时按该字段显隐；PC 增加 --out-temp 启动参数
     用于显式打开，板端等待阶段6设置体系接线。
  4. 中文（对应 D1 差异 4）：保留英文占位（RANGE 等），字重/换行/字体子集
     归 D3，本轮不生成 CJK 字体，不将英文截图标为中文复刻完成。
  5. 渲染差异（对应 D1 差异 7）：指针抗锯齿、刻度/底栏渐变为软件渲染与 Qt
     光栅化的预期差别，不做逐像素补偿。
Qt依据（文件链接、函数/分支）：
  /home/xfy/CarHml_XIAO/qml/GlobalData.qml：tripType=1 / odo=1122232 /
    tripA=3998 / tripB=3222；geartext 缺省 "P"。
  /home/xfy/CarHml_XIAO/qml/ComControl/ComTotalMileage.qml：公里整数、
    英里 1 位小数。
  /home/xfy/CarHml_XIAO/qml/ComControl/ComSubTotalMileage.qml：tripA/10.0、
    英里 tripA/16.0934，保留 1 位小数；前缀 Trip A/B/Trip。
  /home/xfy/CarHml_XIAO/qml/PageModel/PageSubaruForester1.qml L408-415：
    IconTemperature 可见性 = SettingsMgr.outTemperature && !carPlayModel。
  /home/xfy/CarHml_XIAO/src/SettingsManager.cpp L136：
    OutTemperatureOnOff 缺省 false。
  /home/xfy/CarHml_XIAO/src/system_parse.cpp L253-313：0x08 的 tripSet 与
    u24 ODO/TripA/TripB 布局。
修改文件：
  main.c：scenario_push_frames 档位帧与 0x08 里程帧；--out-temp 解析与视图覆盖。
  include/carmw/car_state.h：car_state_t 新增 outside_temp_on。
  src/car_state.c：car_state_init 显式置 false。
  src/ui/PageModel/page_subaru_forester1.c：温度行初始隐藏，apply 按字段显隐。
实际执行命令及退出码（cwd=项目根）：
  cmake -S . -B /tmp/lvgl_two_pc_acceptance -DCARMW_PLATFORM=PC -DCARMW_BUILD_UI=ON \
    -DCARMW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -> 0
  cmake --build /tmp/lvgl_two_pc_acceptance --clean-first -j4 -> 0
  ctest --test-dir /tmp/lvgl_two_pc_acceptance --output-on-failure -> 0（2/2 通过）
  /tmp/lvgl_two_pc_acceptance/carmw_tests -> 0（1630 checks / 0 failure）
  /tmp/lvgl_two_pc_acceptance/carmw_overlay_tests -> 0（798 checks / 0 failure）
  cmake -S . -B /tmp/lvgl_two_board_acceptance -DCARMW_PLATFORM=BOARD \
    -DCARMW_BUILD_TESTS=OFF -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=/home/xfy/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc -> 0
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j4 -> 0
  file /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo -> ELF 64-bit LSB, ARM aarch64
  DISPLAY=:0 SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    timeout 5 /tmp/lvgl_two_pc_acceptance/bin/carmw_ui_demo --scenario=normal -> 124
    （正常启动至超时，无资源缺失）
测试结果（套件/断言，不只写"通过"）：carmw 1630 checks / 0 failure；
  carmw_overlays 798 checks / 0 failure；ctest 2/2。PC/BOARD 构建 0 错误，
  项目源文件 0 警告（日志 warning 全部来自 third_party/lvgl）。
视觉证据路径（如适用）：本轮未采集（用户明确不要求截图，由用户自行对比）。
板端环境与实测（如适用）：仅交叉编译通过，未上台架运行。
已知差异/未验证项：
  - 未采集修改前后截图，像素级位置/字重/抗锯齿差异未量化；不能宣称 D 项
    视觉验收通过，12.3 的 D 项保持未勾选。
  - 中文文案仍是英文占位（RANGE/Trip 等），CJK 字体子集、字号与换行未做，
    归 D3。
  - 车外温度仅 PC 有 --out-temp 入口；板端无设置界面/持久化，默认隐藏，
    属阶段6设置体系范围。
  - --out-temp 未单独写单元测试，当前由构建与启动链路间接覆盖。
  - 场景 0x08 为固定演示输入，不代表实车里程数据。
后续依赖与下一步：D2 的剩余布局类差异需用户自行对比后反馈具体项，再按
  D2 的第 1/2/3 类逐项处理；中文与字体进入 D3（需确认是否恢复中文）；
  之后按 13.2 进入第五批 E1。
```

#### D3 执行记录（2026-11-05）

```text
任务ID：D3
状态：中文按用户确认豁免；资源可重复生成已全量核对（332 张源图逐字节一致）
用户确认（2026-11-05）：保留英文占位，不生成 CJK 字体子集，D2/D3 不把
  英文界面标为中文复刻完成。
中文与字体：
  - PC/BOARD 均只用 LVGL 内置 Montserrat；lv_conf_board.h 启用 14/18/20/
    22/24/28/36/44/46/48 号，24/28/36/46/48 等由页面实际使用。
  - RANGE/Trip/提示等文案仍为英文占位；CJK 子集、字重与换行未做，登记豁免。
  - 未评估中文字体授权与字重，本轮不引入任何新字体文件。
资源可重复生成验证：
  方法：把 tools/png2bin.py 复制到 /tmp/png2bin_check/tools，软链
    assets/src，执行 python3 /tmp/png2bin_check/tools/png2bin.py --force，
    输出到临时 assets/bin32、assets/bin16，再与仓库现有 bin 逐文件比对。
  结果：0（处理 332 张源图）；diff -rq 无任何 differs，重新生成的 bin32/
    bin16 与仓库文件逐字节一致；bin32 差异项 28、bin16 差异项 15 全部是
    "Only in 仓库"（仅存在于仓库、无对应源图的旧文件）。
  无源图的仓库残留（未删除，均未被当前 UI 引用）：根目录 CheDao_01..04、
    Deng_01..03、lun_01..04、lun_Color_mask、hy、hz、qf；material2/ 下
    range0/range/instant_fuel0/instant_fuel/long_term0/long_term/menu_bg/
    out_temperature0/out_temperature/short_term0/short_term 以及旧目录
    RotateSheet；SubaruForester/navigation 旧目录。UI 引用的是
    SubaruForester/ 下的同名可再生文件。
  像素格式：bin32=ARGB8888（PC，LV_COLOR_DEPTH=32）、bin16=RGB565A8
    （BOARD，lv_conf_board.h LV_COLOR_DEPTH=16）；BOARD 资源根
    UI_ASSET_ROOT="A:/bin16/" 已生效（ui_common.h 按 CARMW_PLATFORM_BOARD）。
未验证项：未采集 PC/BOARD 渲染截图；未核对所有旧残留资源是否在历史版本
  被引用；字重/字距仍与 Qt 字体不同。
后续依赖与下一步：按 13.2 进入第五批 E1（工具链干净构建与产物架构核实）。
```

#### E1 执行记录（2026-11-05）

```text
任务ID：E1
状态：交叉编译与产物架构核实通过；实机环境参数待用户确认
实际执行命令及退出码（cwd=项目根）：
  cmake -S . -B /tmp/lvgl_two_board_acceptance -DCARMW_PLATFORM=BOARD \
    -DCARMW_BUILD_TESTS=OFF -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=/home/xfy/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc -> 0
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j4 -> 0（GCC 7.4.1）
  file /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo -> 0
  readelf -d /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo / strings -> 0
核实结果：
  - 产物：ELF 64-bit LSB executable, ARM aarch64, dynamically linked,
    interpreter /lib/ld-linux-aarch64.so.1。
  - NEEDED：libpthread.so.0、libm.so.6、libc.so.6；未链接 SDL2。
  - 符号含 GLIBC_2.17（目标 libc 下限 2.17）。
  - BOARD 宏：CMakeLists.txt L167 定义 CARMW_PLATFORM_BOARD=1。
  - 显示：lv_linux_fbdev / /dev/fb0，lv_conf_board.h LV_COLOR_DEPTH=16、
    LV_USE_SDL=0。
  - 资源：UI_ASSET_ROOT "A:/bin16/"；bin16 为 RGB565A8。
  - 串口：main.c SERIAL_DEVICE "/dev/ttyS2"、SERIAL_BAUD 115200。
  - 未在宿主机运行板端二进制；板端二进制未部署。
未验证项（需用户/硬件确认，E2/E3 阻塞）：
  - 目标板确实是 aarch64 + glibc 2.17 及以上、loader 路径与产物一致。
  - /dev/fb0 实际分辨率/stride/像素格式是否匹配 1920x720 + RGB565。
  - /dev/ttyS2 与 115200 是否为目标硬件实际串口参数（当前为工程约定值）。
后续依赖与下一步：E2 需目标板访问、资源路径与隔离台架；E3 需实机性能
  测量。没有硬件时保持阻塞，不补写结果。
```

#### E2 实机启动记录（2026-11-05，进行中）

```text
任务ID：E2（目标板实机启动）
状态：已定位并修复板端资源打不开的问题；等待用户重新部署后复测
现象（用户提供板端串口日志）：
  sh-4.4# ./carmw_ui_demo
  大量 [Warn] lv_fs_open: Can't open file (A:/bin16/...): unknown driver
  letter lv_fs.c:88，随后 image_decoder_get_info / lv_image_set_src 失败；
  应用本身继续运行并打印 "fbdev /dev/fb0 + /dev/ttyS2@115200"。
根因：config/lv_conf_board.h 没有开启 LV_USE_FS_STDIO，板端二进制中
  lv_fs 未注册 'A' 盘符驱动。PC 配置 config/lv_conf.h 有该段，板端配置遗漏。
修复：在 lv_conf_board.h 增加
  LV_USE_FS_STDIO 1 / LV_FS_STDIO_LETTER 'A' / LV_FS_STDIO_PATH "./assets" /
  LV_FS_STDIO_CACHE_SIZE 0，与 PC 配置保持一致；lv_init() 会按
  LV_USE_FS_STDIO 自动调用 lv_fs_stdio_init()。
实际执行命令及退出码：
  cmake --build /tmp/lvgl_two_board_acceptance --clean-first -j4 -> 0
  nm /tmp/lvgl_two_board_acceptance/bin/carmw_ui_demo -> lv_fs_open(T)、
    lv_fs_stdio_init(T)，确认驱动已链接。
测试结果：板端二进制重新构建成功；尚未在目标板复测，不能判定资源已显示。
未验证项/注意事项：
  - 资源目录按相对路径 "./assets" 解析，需保证在启动 carmw_ui_demo 的
    工作目录下存在 assets/bin16/...；若资源放在其他绝对路径，需要调整
    LV_FS_STDIO_PATH 或启动 cwd。
  - 板端 fb0 与串口已能打开，但实际分辨率/stride/像素格式、串口数据仍需
    复测确认（E1/E2 未验证项不变）。
后续依赖与下一步：用户重新部署最新 board 二进制与 assets/bin16 后再跑
  ./carmw_ui_demo，确认不再出现 "unknown driver letter"；若仍有打开失败，
  日志会变为 "No such file or directory"，据此核对资源相对路径。

复测 1（2026-11-05 09:3x，仍失败，确认是旧二进制）：
  - 用户再次提供的板端日志仍全部为 "unknown driver letter"，且启动即为
    旧行为；核对仓库 build_board/bin/carmw_ui_demo 时间戳为 2026-11-04
    16:47、md5=cc541ac3c754712606a42c297efad6dd，确定板上运行的是修复前
    的旧二进制，不是本次配置遗漏。
  - 已对仓库内 build_board 执行 cmake --build build_board --clean-first -j4
    （退出码 0），产物 build_board/bin/carmw_ui_demo 更新为 2026-11-05
    09:32，md5=ae3267042b6107f9026adde8e1b4cb05，与 /tmp 板端构建产物
    完全一致；nm 确认含 lv_fs_stdio_init(T)/lv_fs_open(T)。
  - 结论：代码侧修复已验证，待用户重新拷贝 build_board/bin/carmw_ui_demo
    到板上再复测。部署前应核对板上二进制 md5 为
    ae3267042b6107f9026adde8e1b4cb05，避免再次运行旧文件。
```

#### E3 性能问题记录（2026-11-05，进行中）

```text
任务ID：E3（板端帧率/交互流畅度）
状态：已定位指针卡顿的主因并完成第一轮优化；待板端复测帧率
现象（用户反馈）：板上运行后十分卡，指针一顿一顿，整体刷新很慢。
分析（静态代码 + 资源头信息）：
  - 车速表/转速表指针图均为 620x620（RGB565A8，单张约 1.15MB），
    通过 lv_image_set_rotation() 做软件旋转。
  - LVGL 旋转会把整个图片矩形的包围盒加入重绘区：
    620x620、轴心在中心的指针，旋转包围盒约 876x876 像素；
    两张指针表每帧各一次，约 1.5M 像素/帧，还要叠加底图 alpha 混合。
  - main.c REFRESH_MS=33（约 30fps），渲染耗时超过周期时 ui_loop 无休眠，
    表现为指针跳动、整体刷新慢。
  - 资源头信息确认：speed_point1 / rotate_point1 的实际非透明区域只有
    (278,71)-(342,338)，即 64x267，周围 95% 是透明像素却参与了旋转。
优化（第一轮，已实施）：
  - tools/png2bin.py 新增 CROP_TO_ALPHA_BBOX，转换前按 alpha 包围盒裁剪
    speed_point1 / rotate_point1，输出 bin 由 620x620 变为 64x267。
  - ui_dial_create() 增加 needle_x/needle_y/pivot_x/pivot_y 参数；
    speed_forester1.c / rotate_forester1.c 传入裁剪偏移 (278,71) 与
    轴心 (32,239)，保持表盘中心 (310,310) 不变。
  - 预期：单指针旋转包围盒由约 876x876 降到约 482x482，重绘像素降约 3.3 倍；
    视觉位置与旋转中心不变。
实际执行命令及退出码（cwd=项目根）：
  python3 tools/png2bin.py --force --only SubaruForester -> 0
    仅 4 个文件变化：bin32/bin16 下 speed_point1.bin、rotate_point1.bin；
    其余 SubaruForester bin 逐字节不变。
  cmake --build build --clean-first -j4 -> 0（项目源码 0 warning）
  ctest --test-dir build -> 2/2 通过（carmw、carmw_overlays）
  cmake --build build_board --clean-first -j4 -> 0（项目源码 0 warning）
    产物 build_board/bin/carmw_ui_demo 2026-11-05 09:58，
    md5=a657be94448dafdc79d773f989432a22
测试结果：构建与回归通过；板端帧率尚未测量，E3 不能标完成。
未验证项：裁剪后的指针在板端实际帧率、长时间运行稳定性、视觉边缘是否
  与 Qt 一致（PC 端未采集截图，环境无显示服务）。
后续依赖与下一步（以本次补充为准）：先采集性能监视数据，不先降低刷新率
  或关闭抗锯齿。前述“主因已定位”为静态推测，尚无板端耗时证据。

补充：帧率显示与部署核对（2026-11-05）
  - 用户澄清当前板为 T507，后续才迁移 T113；本次沿用 aarch64 构建。
  - 用户提供的板端 MD5 确认程序为裁剪版，但两张指针仍是旧资源，
    每张 1153212 字节；程序/资源不匹配可解释指针偏移。
    需同步部署新的 bin16 指针，每张 51276 字节；整页卡顿仍待测量。
  - config/lv_conf_board.h 开启 LV_USE_SYSMON=1、LV_USE_PERF_MONITOR=1，
    LV_USE_PERF_MONITOR_POS=LV_ALIGN_BOTTOM_RIGHT，LOG_MODE=0。
    右下角显示 FPS、CPU 指标与平均刷新耗时（绘制 | 刷屏）。
  - 本次仅开启监视器；刷新周期、抗锯齿、渲染模式、构建优化级别均未改变。
  - cmake --build build_board -j4 -> 0；file 确认 ELF 64-bit ARM aarch64。
    build_board/bin/carmw_ui_demo MD5=a94a76bd8832908890943832add1dca1。
  - 下一步：部署新程序与配套指针资源，记录指针运动、车门开关时的两行
    监视器数值；根据实际耗时再定位性能瓶颈。尚未板端验证，E3 保持未完成。

补充：T507 Release 优化构建（2026-11-05）
  - 用户实测：快速切门低点 3 FPS、31% CPU、300ms（绘制299 | 刷屏1），
    大部分切门约15 FPS。该样本主要耗时在绘制阶段，具体读取/混合占比未测。
  - 按用户确认仅启用 Release，不改刷新周期、抗锯齿、资源及帧率显示。
  - 执行 cmake -S . -B build_board -DCMAKE_BUILD_TYPE=Release
    && cmake --build build_board --clean-first -j4，退出码0。
  - 核实 LVGL、carmw、carmw_ui、carmw_ui_demo 的 flags.make 均有
    -O3 -DNDEBUG；file 确认 ELF 64-bit ARM aarch64。
  - 新产物 build_board/bin/carmw_ui_demo，
    MD5=03c6910d6ed78c1b783b96e213257d7b。
  - Release 已保存在现有 build_board 的 CMake 缓存；新建构建目录需显式
    传入 -DCMAKE_BUILD_TYPE=Release，本次未修改项目默认构建类型。
  - 下一步：只替换程序（指针资源须已配套），重复切门并记录两行监视数据。
    尚无 Release 板端性能结果，不宣称达到50 FPS，E3 保持未完成。

补充：转向灯统一相位（2026-11-05）
  - 用户反馈切门平均约20 FPS；随后确认断针已不再出现，怀疑之前同时运行
    两个程序。双进程原因未经证实，本次不修改渲染路径。
  - ADB 已确认 /opt/car_dashboard 内 Release 程序及两张裁剪指针 MD5 配套；
    车门、故障灯、转向灯由真实串口输入，不注入测试帧。
  - 用户仍确认仪表与车模转向灯不同步。原来仪表左右各用500ms定时器，
    车模另在页面apply中计时；独立相位及换向重置规则不能保证同步。
  - ui_turn_signal_apply 统一计算1000ms完整周期的相位，左右仅用ON/OFF，
    车模直接消费同次apply的灯态，移除车模独立闪烁计时。
    左右切换/加入双闪沿用相位，全关后重开立即亮；延迟保留周期余量。
  - 新增 tests/test_turn_signal.c 及 carmw_turn_signal CTest，覆盖组件与
    页面灯态、连续刷新、换向、双闪、长延迟、重开、tick回绕及生命周期。
  - PC构建成功，ctest 3/3通过：核心1630、覆盖层798、转向灯5369检查均通过。
    BOARD Release构建成功，新程序MD5=55d3af5ad8235df2b8b829a098077c45。
  - 下一步：停掉旧程序、只替换二进制并确保单实例，实机验证箭头/车模同步；
    不改刷新周期、图片资源、fbdev；未部署本次产物，E3仍未完成。

补充：指针短时线性插值与50fps调度目标（2026-11-05）
  - main.c 当前 REFRESH_MS 已为20ms，本轮未改 main.c；前述33ms是历史记录。
    config/lv_conf_board.h 新增 LV_DEF_REFR_PERIOD=20；PC默认配置不变。
    50fps仅为调度目标，不保证实际绘制帧率，车门绘制瓶颈未解决。
  - ui_dial 使用 LVGL 原生 lv_anim，持续100ms、线性路径，不新增线程或定时器。
    创建时直接设置传入初始角度；重复相同目标不重启动画；新目标从最近实际
    显示的有符号 current_value 续接，避免图片角度取模导致跨零走长路径。
    新目标等于当前显示值时取消旧动画。动画绑定针图对象，LVGL对象析构自动
    lv_anim_delete(obj, NULL)，父页面销毁与样式切换不会留下指针动画引用。
  - speed_forester1 / rotate_forester1 持有完整 ui_dial_t，统一保存目标；
    仅平滑视觉指针，真实速度/RPM文本、单位和刻度高亮仍按本次数据立即更新。
    未修改串口、资源、其他闪烁逻辑、fbdev或其他渲染策略。
  - 精确修改文件：config/lv_conf_board.h、src/ui/widgets/ui_dial.c/.h、
    src/ui/SpeedDial/speed_forester1.c/.h、src/ui/RotationDial/rotate_forester1.c/.h、
    tests/test_dial.c（新增）、tests/test_overlays.c、CMakeLists.txt及本交接文档。
  - 新增 carmw_dial 回归覆盖：相同目标重复apply；0/1/20/50/99/100/150ms
    重定向及两次动画处理之间的新目标；负角度跨零；改向当前显示值；组件重复
    创建/父树销毁；运动中样式切换与ui_app_deinit；真实数字文本立即更新。
    原公英制测试适配结构字段与100ms到达时间，单位/刻度仍先断言即时更新。
  - 首次PC构建因旧测试引用sp.needle/sp.dial失败，修正后执行：
    cmake --build build -j4 && ctest --test-dir build --output-on-failure -> 0。
    ctest --test-dir build -V -> 0，4/4通过：核心1630、覆盖层798、
    转向灯5369、指针173项检查，均0失败；100次样式往返仍为436对象/3定时器。
  - cmake -S . -B build_board -DCARMW_PLATFORM=BOARD -DCMAKE_BUILD_TYPE=Release
    -DCARMW_BUILD_TESTS=OFF && cmake --build build_board -j4 -> 0。
    file确认PC为x86-64、板端为ARM aarch64；本轮执行增量构建，并非干净构建。
    build/bin/carmw_ui_demo MD5=61019a9b5a4cb7531f85a14b447b57e2。
    build_board/bin/carmw_ui_demo MD5=6f083d60d49cabf3f6e846658645c82d。
  - 本轮未部署板端、未提交git，未做实机帧率/视觉验收；E3保持未完成。
    下一步需经用户确认后在单实例板端验证指针连续性、转向灯同步、切门时绘制/
    刷屏耗时及实际FPS，再定位车门绘制瓶颈，不能把20ms配置视为50fps实测。

补充：0x20 方向盘按键接通界面（2026-11-05）
  - 现象：板端串口发 0x20 按键不能翻中心卡片。根因有两点：
    1) src/car_decode.c 里 car_decode_key 取键值用了 data[0] & 0x1F，与 Qt
       的 data[0] & 0xFF 不一致；0x20 以上键值被截断（如 0x25 变 5）。
    2) 板端 main.c 的 refresh_timer 只跑 car_link_pump，从不取按键队列，
       按键进了 link->keys[] 却没有任何消费者。
  - 修复：
    * car_decode.c：state->key.code = data[0]（整字节透传）；同步更新
      include/carmw/car_state.h 中 code 字段注释。
    * 新增 src/ui/car_ui_key.h / .c：car_ui_key_dispatch(link, ctx) 从
      car_link_take_keys 取事件并逐条映射为界面语义码；car_ui_key_pump(link)
      在 dispatch 后处理 31 的样式切换。
    * main.c 板端分支 refresh_timer 调用 car_ui_key_pump(&s_link)。
    * src/ui/ui_node.h 新增 UI_KEY_UP/DOWN/LEFT/RIGHT，UI_KEY_PREV/NEXT
      保留给 PC 演示键盘。
    * page_subaru_forester1.c 的 s1_on_key：上/左 -> 上一张，下/右 -> 下一张，
      与 Qt PageSubaruForester1.qml onKeyPressed 一致。
  - 映射口径（对齐 Qt Handler.js handleSteeringKey + main.qml）：
    pressed 0 释放 / 1 短按 / 2 长按。
    短按 13 置 ctx.nav_home（返回首页）；19 上、20 下、21 左、22 右、
    23 确定；31 切表盘样式（ui_app_next_style）。
    长按只有 19/20 有动作，映射为左/右推摇杆（上一张/下一张），其余长按忽略。
    Qt 侧无去抖与去重，按住连发即连续触发，本实现同样不做事后去重。
  - 未做分支：LVGL 侧尚无 CarPlay 与设置页，故 13 / 23 / 26 的
    “CarPlay 中 / 设置页中”分支未实现，当前一律按“非 CarPlay、非设置页”
    处理（23 无动作，13 只置标志，26 未接）。
  - 测试：新增 tests/test_steering_key.c + carmw_steering_key CTest，用内存
    端口构造真实 0x20 帧，覆盖整字节透传、短按 19/20/21/22 翻卡与首尾循环、
    释放/23/13/31 过滤、长按 19/20 左右、按住连发、批量顺序、空队列。
    同步修正 test_decode.c / test_link.c 中旧的 & 0x1F 期望值。
    注意：卡片下标从第 2 个 child 起算（第 0 个是车辆视图 660x340，
    child1 为内部容器），断言按实测对象树顺序编写。
  - 结果：cmake --build build -j8 通过，ctest 5/5 全部通过；
    BOARD Release 构建通过，build_board/bin/carmw_ui_demo
    MD5=963f8cf9113e4fd21f27057540fd308c（aarch64，1599120 字节）。
  - 下一步：部署该二进制到 /opt/car_dashboard 后，实机验证串口发
    19/20/21/22/23/13/31 的行为；23 与 26 需等设置页 / CarPlay 实现后再补。
```

### 12.3 总体验收清单

- [x] A：干净PC构建、现有回归、固定场景入口可复现。（2026-11-04 验证通过，见 12.2 A1/A2 记录）
- [x] B1/B2：单位、无效续航、油耗特殊值和25%边界修复并测试。（B1/B2 已于 2026-11-04 验证通过，见 12.2 B1/B2 记录）
- [x] B3：警告真实入口、事件序列/队列和计时规则完成并测试。（2026-11-04 验证通过，见 12.2 B3 记录；后置项见该记录）
- [x] C1：胎压跨样式生命周期不续期、不重放。（2026-11-04 验证通过，见 12.2 C1 记录）
- [x] C2：覆盖层组合、优先级、雷达方向、巡航枚举与中心卡片恢复通过页面级回归。（2026-11-04 验证通过，见 12.2 C2 记录）
- [x] C3：灯态、卡片逐一访问、状态保留与资源生命周期通过页面级回归。（2026-11-04 验证通过，见 12.2 C3 记录）
- [ ] D：同状态视觉对比有证据；中文/动画差异已完成或获得明确豁免。
  （D1 于 2026-11-04 产出截图与差异清单；D2 于 2026-11-05 修正档位口径、
  场景 0x08 ODO/Trip、车外温度默认显隐，用户自行对比确认无误；D3 于同日
  登记中文英文占位豁免，并验证 332 张源图可逐字节重现，见 12.2 D1/D2/D3 记录。
  像素级全场景证据仍未采集，D 项保持未勾选）
- [x] E1：使用正确目标工具链干净构建，并核实产物架构。（2026-11-05 验证通过，
  见 12.2 E1 记录；实机 fb0/串口参数与目标 libc 仍待硬件确认）
- [ ] E2/E3：板端台架与性能有实际记录；未具备条件时明确标为未完成。
  （2026-11-05：E2 实机已跑起来，定位并修复 A: 盘符未注册；E3 针对指针
  卡顿完成第一轮裁剪优化；已采集切门低点300ms（绘制299 | 刷屏1），
  完成 T507 Release（-O3）干净构建，用户报告切门平均约20 FPS。
  转向灯共享相位修复及PC回归已完成，板端同步效果待复测。
  本轮已完成100ms原生线性指针插值、板端20ms刷新配置及PC 4/4回归，
  BOARD Release构建通过，未部署；50fps仅为调度目标，不保证实际帧率，
  车门绘制瓶颈未解决。下一步经用户确认后单实例实机复测指针连续性、
  转向灯同步与切门绘制/刷屏耗时、实际FPS。两项保持未勾选，见12.2 E2/E3记录）
- [ ] 所有已知偏差、后置项、测试限制已回填；没有把模拟数据当实车结果。

上述复选框均为待执行项，写入方案不表示已验证。

---

## 13. 可直接交给下一位 AI 的任务指令

### 13.1 首轮指令（建议复制）

> 阅读本交接文档第10～13节。工作目录为 `/mnt/hgfs/share/lvgl9.5demo/lvgl_two`，Qt参考目录为 `/home/xfy/CarHml_XIAO`，Qt源码保持只读。
>
> 本轮仅执行A1、A2和B1：先干净构建并复核现有两个测试套件，再修正演示警示灯轮播的关闭语义，建立最小可重复场景入口，最后修复车速及距离单位闭环。不要改其他布局，不推进设置/CarPlay/升级。
>
> 优先读取真实代码、现有测试和Qt数据链路，不依赖旧文档“已完成”字样。所有单位预期独立取自Qt，不用被测函数自证。必须覆盖“速度不变只切单位”“高亮索引不变只切单位”和续航65535。遵守hgfs干净构建要求。
>
> 若图形会话不可用，完成可执行的对象/协议测试并记录视觉验证阻塞，不伪造截图结果。完成后按12.2回填实际命令、结果、修改文件及未验证项，再报告下一批任务。不要自动提交代码。

### 13.2 后续分批顺序

- 第二批：B2 → B3；B3涉及公共状态字段，避免与其他代理同时修改同一文件。
- 第三批：C1 → C2 → C3；重点验证上一轮三项修复在整页生命周期中仍成立。
- 第四批：D1 → D2 → 经确认后的D3；没有Qt可运行参考时先完成布局清单，不声称像素验收通过。
- 第五批：E1 → E2 → E3；无板卡时只完成E1，其余保留阻塞。
- 第六批：用户确认范围后执行F，按设置、覆盖开关、真实连接、升级分别交付。

并行规则：允许不同AI只读核对Qt组件；允许互不重叠的组件任务并行。数据结构、解码器、样式主页面、CMake、同一交接文档由单一执行者整合，禁止多个AI同时覆盖。每个任务都需回报输入契约和测试证据，整合者重新跑全量回归。

---

## 14. 阶段6详细迁移计划（2026-11-05）

### 14.1 基线、目标与边界

**状态：仅编写计划，未开始阶段6功能实现。** 用户已反馈板端0x20按键能够切换；这只作为按键翻卡的实机确认，不推定13/23/26完整路由、31全部车型风格、CarPlay或设置已通过。

目标：迁移Qt的设置模型、设置交互、状态相关按键、设置驱动的显示规则、真实互联适配与升级交互/后端。每个子项分别记录“代码完成 / PC验证 / 板端验证 / 阻塞依赖”，不能合并宣称全部完成。

本轮核对后的纠正：

1. **不新增臆造的独立首页。** Qt以仪表Loader为主体，叠加设置栈与互联状态；13表示按上下文返回。普通状态还涉及`reSettings()`与仪表`aflag`切换，不能统一实现成中心卡索引归零。
2. **23不是始终无动作。** Qt存在音乐卡发命令、互联卡进入投屏、低速打开设置、设置项确认等分支。此前LVGL忽略23只是功能未迁移，不是完整Qt行为。
3. **31不是全局样式注册表简单轮询。** 必须按车型可用风格映射并保存；当前注册的forester1/general1不等于同一车型的两个合法风格。
4. **CarPlay不是占位页面。** Qt使用ZLink回调、H.264硬解和硬件视频图层；UI原型或模拟状态不能记作真实互联完成。
5. **升级不是新增一个设置按钮即可完成。** Qt现有入口为USB设备检测；设置菜单未发现对应升级项。是否保留自动检测、是否增加确认须单独确认。
6. 现阶段目标板为T507/aarch64；T113为后续单独平台适配，不默认工具链、SDK二进制、硬解/显示接口可以直接复用。
7. E3车门绘制性能、实际50fps目标和未完成视觉项继续独立跟踪。阶段6实现前先完成E2/E3闭环，或经用户确认带着明确未完成项推进；不能因新功能开始而勾选旧验收。

### 14.2 Qt依据与现有LVGL落点

| 范围 | Qt参考（只读） | LVGL当前落点/缺口 |
|---|---|---|
| 设置定义、默认值 | [SettingsManager.h](file:///home/xfy/CarHml_XIAO/src/SettingsManager.h#L15-L125)、[SettingsManager.cpp](file:///home/xfy/CarHml_XIAO/src/SettingsManager.cpp#L72-L214) | 尚无完整设置模型；需与车辆快照分开 |
| 防抖写入与路径 | [SettingsManager.cpp](file:///home/xfy/CarHml_XIAO/src/SettingsManager.cpp#L14-L70)、[配置路径](file:///home/xfy/CarHml_XIAO/src/SettingsManager.cpp#L1216-L1221) | 需明确本地配置格式、位置与保存时机 |
| 原始按键映射 | [Handler.js](file:///home/xfy/CarHml_XIAO/handlers/Handler.js#L914-L966) | [car_ui_key.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/car_ui_key.c)，13仅置标志、23忽略、26缺失 |
| 状态相关路由 | [main.qml](file:///home/xfy/CarHml_XIAO/qml/main.qml#L753-L1191) | [ui_app.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/ui_app.c)、[ui_session.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/ui_session.h)，目前只具备仪表舞台与部分跨样式会话 |
| 设置菜单 | [PageMainSettings.qml](file:///home/xfy/CarHml_XIAO/qml/Settings/PageMainSettings.qml#L24-L105) | 尚未迁移根菜单/子页/焦点与编辑状态 |
| 互联SDK与硬解 | [main.cpp](file:///home/xfy/CarHml_XIAO/main.cpp#L121-L140)、[zlinkmanager.h](file:///home/xfy/CarHml_XIAO/src/CarPlay/zlinkmanager.h)、[h264decoder.cpp](file:///home/xfy/CarHml_XIAO/src/CarPlay/h264decoder.cpp) | 当前互联卡和演示注入不等于设备后端 |
| 升级 | [upgrademanager.cpp](file:///home/xfy/CarHml_XIAO/src/upgrademanager.cpp)、[UpgradePopup.qml](file:///home/xfy/CarHml_XIAO/qml/ComControl/UpgradePopup.qml) | 尚无完整升级服务/串口交接/恢复流程 |

### 14.3 实施顺序与交付批次

| 步骤 | 交付内容 | 前置依赖 |
|---|---|---|
| 6.0 | 功能契约、车型范围、性能基线与验收矩阵 | 用户确认实施范围 |
| 6.1 | 设置数据模型、变更通知、持久化 | 6.0 |
| 6.2 | 页面状态、设置栈骨架、逐事件按键路由 | 6.1 |
| 6.3 | 设置菜单与本地设置交互 | 6.2 |
| 6.4 | 单位/明暗/卡片/覆盖层即时生效 | 6.1～6.3 |
| 6.5 | 车型风格、车辆设置命令与反馈 | 6.2～6.4，MCU协议已核对 |
| 6.6 | ZLink、硬解、视频图层与真实互联 | 6.2、6.4，SDK及硬件条件齐备 |
| 6.7 | USB升级状态机、界面与平台后端 | 6.2，升级/恢复契约确认 |
| 6.8 | 全链路、视觉、生命周期、板端验收 | 所有已纳入范围的前置项 |

推荐按6.0→6.1→6.2先交付最小闭环，再逐批推进。6.6和6.7可分别调查外部依赖，但不得并行覆盖公共UI状态、串口所有权或同一构建文件。任何缺少SDK/平台接口的子项保留阻塞，不用模拟后端替代真实验收。

### 14.4 步骤6.0：冻结迁移契约与当前基线

**操作步骤**

1. 先确定本轮车型范围：建议先闭环当前Forester Style1；Qt其他车型、赛道专用分支先登记，不擅自扩到全部车型。
2. 建立设置逐项清单：属性名、INI节/键、类型、缺省值、合法范围、是否持久化、车型可见条件、MCU读写、影响的控件、Qt依据与验收用例。
3. 建立按键矩阵：原始键/短长按 × 仪表卡片/设置根页/设置子页/互联/升级；另记车型特例和同一信号被多个QML组件监听的情况。
4. 记录现有PC回归、板端程序/资源校验值、单实例启动、串口、分辨率和当前FPS/绘制/刷屏耗时。旧300ms样本保留为历史，不代替本次测量。
5. 固定同一输入序列下Qt的选中项、卡片、风格及返回行为。对象创建顺序不能作为业务卡片顺序的依据。
6. 记录中文字体和菜单文案要求；此前仪表英文豁免不自动延伸到新增中文设置页。

**验收**：清单中每项能追到Qt实现；未知默认值/硬件依赖标“待确认”；无未经确认的新首页、全车型或升级入口扩展。

### 14.5 步骤6.1：设置模型与持久化

**操作步骤**

1. 划分四类状态：本地持久设置、MCU反馈状态、工厂菜单可见性、运行时UI状态。设置不能直接覆盖真实车速/故障等车辆快照。
2. 以Qt`loadAllSettings()`为缺省值依据，不直接复制头文件初值。例如中央胎压卡默认开启和行车信息胎压显示默认关闭属于不同设置。
3. 首批字段覆盖单位、明暗、风格、中心卡、卡片显隐、外温及常用显示开关；完整字段清单在6.0冻结，避免UI先写一批无存储归属的变量。
4. 核对本地/协议优先级：单位和车型等若同时有MCU与本地来源，逐项定义谁生效、何时保存、是否等待回读，不用“最后一次写入覆盖”代替规则。
5. 确认是否兼容原Qt`setting.cfg`。Qt路径为可执行文件所在目录，不是当前工作目录；计划优先保留既有节/键语义，实际落盘位置、文件权限及首次导入由6.0确认。
6. 实现读取、默认值补齐、合法范围约束与变更通知；车况不变化时，设置变更也必须触发相关显示更新。
7. **采用ACC关闭统一保存，不采用Qt约5秒防抖落盘。** 启动时读取配置到内存，运行期间只修改内存、通知UI并标记dirty，不周期写文件。检测ACC由开转关时立即提交待保存设置，无修改则不写；重复收到ACC关闭帧不重复触发。正常退出时补保存未落盘修改，不重复写已保存版本。
8. **用户已确认与MCU约定：ACC关闭后保留3秒供电时间用于保存。** 这是硬件约定，不是已验证的存储耗时。收到关闭事件立即启动保存，不能等满3秒再写；串口传输/排队、事件处理、文件写入及同步都占用该窗口。保存必须在断电前完成并留有余量，板端需测量端到端最坏耗时；不能把“收到事件后的3秒”当作完整剩余预算，也不假设已有保存完成回执或延长供电协议。
9. 使用单一后台存储执行者，提交带版本号的设置快照，避免阻塞LVGL绘制；后台不得操作LVGL对象。完成旧快照保存不得清除更新版本的dirty。ACC关闭后的设置修改、再次ACC开启及退出与保存并发需明确处理，窗口内出现新修改应合并补存，不可无界等待或重试。
10. 采用同目录临时文件写入并检查结果 → fsync临时文件 → rename原子替换正式配置 → fsync目录。只有全部成功才标记对应版本已保存；失败保留待保存状态并记录具体阶段，不显示已保存。替换前失败保持原配置；替换后目录同步失败需明确标记持久性未确认，不能保证旧文件仍在。实际断电耐受性依赖板端文件系统与存储验证，不把原子替换等同于绝对不会丢数据。
11. 异常断电、未收到ACC关闭事件或3秒内保存未完成时，本次运行中尚未持久化的设置可能丢失；该风险随ACC集中保存方案登记，不自动增加周期写盘或引入数据库。正常退出补保存不能覆盖强制终止或突然掉电。
12. 时间源当前Qt读写被注释，不能按“已持久化功能”迁移；运行时车型ID和持久化的车型子类型也不得混为一项。

**验收**：首次启动、配置缺项、合法值恢复、非法值回退、运行期连续修改不写盘、ACC下降沿集中保存、重复关闭帧/无dirty不写、正常退出补存、保存期间新修改不丢、ACC快速开关、写入/同步失败、重启恢复均有证据。板端单独记录MCU关闭通知至配置同步完成的端到端耗时及供电余量，在实际存储负载下确认满足3秒窗口；只记录平均耗时或仅看到write返回不能判通过。实际断电测试需在用户确认的可恢复台架上进行。

**实现状态（2026-11-05，首批仅设置模型+ACC保存，未接设置页面）**

范围：只实现“设置数据模型 + ACC关闭统一落盘 + 退出补存”，不迁移设置菜单、不接管现有UI设置项（明暗/单位/中心卡等仍按原逻辑显示，尚未由设置模型驱动）。

新增/修改文件：

| 文件 | 内容 |
|---|---|
| [car_settings.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/include/carmw/car_settings.h) | 设置值结构、变更位、通知回调、版本/dirty接口；不依赖LVGL |
| [car_settings.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/car_settings.c) | INI解析与默认值回退、`mark_changed`版本自增与通知、快照、临时文件+fsync+rename+目录fsync |
| [car_settings_saver.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/include/carmw/car_settings_saver.h) | `CAR_SETTINGS_SAVE_BUDGET_MS=2000`、后台保存线程与ACC采样接口 |
| [car_settings_saver.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/car_settings_saver.c) | 单后台线程串行保存；仅`true→false`下降沿触发；锁内取快照、锁外写文件；超时等待 |
| [test_settings.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/tests/test_settings.c) | 8组用例：默认值、缺文件、非法值回退、dirty/通知、保存重载、快照后新修改、保存失败保留dirty、ACC下降沿 |
| [main.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/main.c) | 启动`car_settings_load`+`car_settings_saver_start`；`refresh_timer`中ACC有效才采样；退出`flush(2000ms)`后`stop` |
| [CMakeLists.txt](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/CMakeLists.txt) | carmw加入两个源文件；新增`carmw_settings`测试目标 |

关键行为：

1. 配置路径固定`/opt/car_dashboard/setting.cfg`，临时文件为同目录`setting.cfg.tmp`；不导入Qt旧配置。
2. 默认值：里程/温度单位公制、`dark_mode=0`、`dial_style="forester1"`、`center_card_mask=0x3FD`（同Qt，4X4-I 默认关闭；`0` 表示全部关闭）。
3. 文件不存在视为正常首次启动（保留默认、不置dirty）；读取失败/非法项保留默认并继续；未知键忽略。
4. 运行期只改内存：`version++`并回调通知，不写文件。
5. ACC采样仅在`acc.valid`（收到真实0x34帧）后进行；首帧只建立基线，`seen`置位后仅在`last_on && !acc_on`时触发保存；窗口内重新开启不取消已发起的保存，新修改按新版本合并补存。
6. 写成功才把`saved_version`提升到快照版本；失败保留dirty并向stderr输出阶段信息。
7. `flush`超时返回false不阻塞退出，`stop`前会再请求一次当前版本。

PC验证（本次实际执行）：

- `cmake --build build -j8`：通过，新增文件零告警（严格C11+`-Wpedantic`）。
- `ctest --test-dir build --output-on-failure`：6/6通过（`carmw`、`carmw_settings`、`carmw_overlays`、`carmw_turn_signal`、`carmw_dial`、`carmw_steering_key`）。
- 板端构建`build_board`（Release/aarch64 T507）：通过，`bin/carmw_ui_demo` MD5 `cd85c12f38ff7e3655ee55faadb94d79`。

待验证（未完成，不计入通过）：

1. 板端实机：改设置 → ACC关闭 → 3秒内落盘 → 重启恢复；当前没有UI入口修改设置，需临时用调试手段写入`setting.cfg`或后续6.3接入后再测端到端。
2. MCU关闭通知到同步完成的端到端耗时与供电余量（目标预算2000ms）。
3. 异常断电/掉电风险按第11条登记，未做断电台架测试。

### 14.6 步骤6.2：页面状态与完整按键路由
**操作步骤**


1. 沿用现有仪表舞台，增加设置栈及互联显示状态，不再造一张“首页”。业务状态与对象生命周期分开，样式重建不能清空设置、重复触发警告或遗留视频层。
2. 区分“设置打开标志”与“设置实际可见”：Qt可见条件包含`speed < 5`。核对车速变化时是隐藏还是退出，不能自行选择。
3. 建立焦点/列表选择/编辑/子页返回状态；Qt部分菜单第一次确认只是进入操作状态，不能所有OK都直接执行设置项。
4. 原始0x20继续整字节取值（`data[0]`与`data[0] & 0xFF`等价），保留0释放/1短按/2长按，不添加Qt没有的去重。
5. 改为逐条执行语义事件，收到一条就按当时状态处理一条。当前31用批次结束时的布尔标志处理，会折叠同批多个31并改变31与方向键的相对顺序；此项须在完整路由中修正。
6. 13、23、26、31进入统一状态路由，不由独立卡片各自抢占；方向键再按Qt主界面和设置子页实际监听规则分发。测试路径必须经过生产用pump，不能只检查ctx标志。
7. 对齐设置8秒超时及调试页例外，确认哪些输入重置计时；切页/返回后不遗留定时器或焦点。

**按键契约**

| 键 | 短按 | 长按 |
|---|---|---|
| 13 | 按上下文退出互联/设置等；普通仪表还需核对`aflag`行为，不强制中心卡归零 | 忽略 |
| 19 / 20 | 上/下；普通仪表翻卡，设置按实际焦点处理，互联转SDK对应事件 | 左/右语义，不能在转换层提前固定为翻卡 |
| 21 / 22 | 左/右；按Qt主界面与子页信号处理核对，不假设设置中一定只移动焦点 | 忽略 |
| 23 | 依Qt优先级处理切换锁、音乐卡、车型分支、互联确认/进入、低速打开设置或设置项确认 | 忽略 |
| 26 | NextOrBack；设置中切根分类、普通仪表调用对应动作，其他分支按Qt确认 | 互联显示中返回，否则OK |
| 31 | 按当前车型切合法风格、保存并执行必要通知；不能简单轮询全部注册样式 | 忽略 |

赛道、NissanXTrail16等专用分支只在确认纳入车型后实现；否则显式标为未迁移，不宣称全车型一致。Qt SDK的19/20是旋转类事件，不是SDK上/下键，不能按数字名称直接映射。

**验收**：包含13→方向键、31→方向键、同批31→31、23打开设置后紧接方向键、26长按、释放/未知状态、设置超时、速度阈值前后及样式重建；结果与独立Qt预期一致。

**实现状态（2026-11-05，最小可测闭环）**

范围：按用户确认，本轮只迁移 Qt 设置界面中与本批设置模型对应的部分——设置根菜单、`MY CAR` 分类下的 `DIAL STYLE / UNITS / CENTER CARDS` 三个子页，其余分类项置灰占位（不可被焦点选中）。设置状态与页面对象分离：样式重建不清空层级状态。

新增/修改文件：

| 文件 | 内容 |
|---|---|
| [ui_settings_page.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/settings/ui_settings_page.h) | `ui_settings_state_t`（view/root/mycar/style/unit/card 焦点与锁定标志）、页面对象、按键/应用/刷新接口 |
| [ui_settings_page.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/settings/ui_settings_page.c) | 594×504 覆盖层（x=663,y=120）与根页/我的座驾/风格/单位/卡片五页的创建、样式、按键状态机；写操作经 `car_settings_set_*` 落到设置模型 |
| [ui_app.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/ui_app.c) / [ui_app.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/ui_app.h) | 设置会话：`settings_open/speed_ok/open/close/back/next_or_back/refresh`、8 秒超时、`speed<5` 门控、设置存储注入 |
| [car_ui_key.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/car_ui_key.c) / [car_ui_key.h](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/src/ui/car_ui_key.h) | 23 低速打开设置、设置已打开时 23 继续转发子页；13 返回/关闭；26 `setNextOrBack`；31 设置中屏蔽；19/20/21/22 设置中继续转发 |
| [main.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/main.c) | 启动后把持久设置注入 UI 并注册变更通知，设置变更即时刷新覆盖层 |
| [CMakeLists.txt](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/CMakeLists.txt) | `carmw_ui` 登记设置页源文件 |
| [test_steering_key.c](file:///mnt/hgfs/share/lvgl9.5demo/lvgl_two/tests/test_steering_key.c) | 新增设置开关/超时/写入用例，按键序列经生产用 `car_ui_key_dispatch` + 0x20 帧注入 |

关键行为：

1. 标签全部英文（`MY CAR` / `DIAL STYLE` / `UNITS` / `CENTER CARDS` 等）——当前字体集无中文字形。
2. 两级 OK：根页首次 OK 进入 `MY CAR` 并把焦点落到第一项；我的座驾首次 OK 锁定列表并选中第一项，之后 OK 才进入子页；单位页首次 OK 只锁定并选中「里程」，之后 OK 才切换/确认；卡片页无锁定步骤，OK 直接勾选。
3. 单位页 `KM/MILE`、`C/°F` 直接写 `car_settings_set_unit`；确认行只负责返回（与 Qt `goBack` 一致）。卡片页按位异或写 `car_settings_set_center_card_mask`；风格页写 `car_settings_set_dial_style`。
4. 8 秒无按键自动关闭；打开设置时状态重置为根页并强制激活，不依赖车速快照的下一帧。
5. 偏差登记：设置态下 21/22 仍按 Qt 主界面规则转发（未做设置内左右移动）；31 在设置中屏蔽；长按 19/20 的 PREV/NEXT 语义未在设置态启用（设置态只处理短按）。

PC 验证（本次实际执行）：

- `cmake --build build -j8`：零错误零告警。
- `ctest --test-dir build --output-on-failure`：6/6 通过；`carmw_steering_key` 184 checks / 0 failures（含设置超时、单位与卡片写入、dirty 置位）。
- 板端构建 `build_board`（Release/aarch64 T507）：通过，`bin/carmw_ui_demo` MD5 `29f2bead3b3d47476686d4ddad12d53a`（上一版 `9326d2c4e2e37f486bc8b2e97538f604`）。

**修正（2026-11-05，板端反馈“按23后看不到设置项”）**

1. 覆盖层补底色：`page->root` 由 `ui_blank_container` 创建（无底色/边框），根页只有 5 个 52×52 细框图标，板端几乎看不出已打开；现改为黑色 `LV_OPA_80` 底 + 8px 圆角。
2. 根分类横排 `UI_SETTINGS_ROOT_Y` 由 90 改为 10，对齐 Qt `defaultLocalOffSet()` 的 y=10（此前相对 `root_bar` 又叠加 80px，整体偏下）。
3. 打开条件兜底：`KEY_OK` 增加 `|| !ui_app_has_data()`，即从未收到含有效数据的车身帧时也允许打开设置（原逻辑会因 `has_data=false` 静默吞键）；PC `poll_keys` 同步。新增 `ui_app_has_data()`。
4. 车速达标但 `speed>=5` 时打印一行 `[key] OK ignored: speed>=5`，便于现场区分“按键没进”和“被速度门控拦截”。

PC 验证：`cmake --build build` 零错误（改动文件无新增告警）；`ctest` 6/6 通过（`carmw_steering_key` 184 checks）。

**修正（2026-11-05，板端反馈“设置单位后上电没恢复”）**

结论：**不是落盘失败，而是落盘时机未到 + 单位没有作用到仪表显示**。落盘严格只在 ACC 下降沿（`car_settings_saver_acc`）或退出时 `car_settings_saver_flush` 触发，运行期改了单位但没关 ACC 就断电，文件里仍是旧值。

1. **提示**：设置页底部新增 `CHANGES SAVED ON ACC OFF`（`ui_settings_page_set_pending`），只要 `car_settings_saver_pending()` 为真就显示，让用户知道“改动还在内存里，等 ACC 关闭才写盘”。`ui_app_settings_set_pending_cb` 由 main 注入，`ui_app_apply` / `ui_app_settings_open` 每帧同步。
2. **显示生效**：`ui_app_apply` 里用 `settings.values.unit_mileage` 覆盖快照的 `state.units.mile`，否则 MCU 帧的 unit 位会把设置盖掉，改了也看不出变化（Qt 以 GlobalData 设置为准）。
3. `car_settings_saver_pending()` 新增（含 `CARMW_NO_THREADS` 空实现）。

PC 验证：6 个测试二进制全部 OK；板端 `build_board` 通过，`bin/carmw_ui_demo` MD5 `4b3a175072f9e8d869c057feddde93af`。

**修正（2026-11-05，板端“发了 ACC 关闭但 cfg 文件都没创建”）**

根因三条，全部与“保存时机”无关：

1. **目录不存在导致写盘必然失败**：`/opt/car_dashboard/` 首次启动时不存在，`open(tmp, O_CREAT)` 直接 `ENOENT`，而失败日志走 stderr 容易被忽略。`write_config()` 现在先 `make_dirs()` 逐级建目录（含最后一级），建目录失败也打印原因。
2. **ACC 下降沿可能永远不发生**：原逻辑首帧 ACC 只建立基线，若上电时 ACC 帧本来就是“关闭”（或整段运行没见过开启），`dirty` 永远不会被提交。新增 `ever_on`，首帧即关闭 / 从未见过开启时的关闭帧也会提交。
3. **（顺带修）部分帧会把其它字段清零**：`link_on_frame` 直接 `decode(..., &link->pending)`，而 decode 只写自己那几个字段，同一 tick 若只收到 ACC 0x34，`car_state_store_apply` 就会把车速等字段整体覆盖成旧 `pending` 里的 0/valid=false —— 这正是此前“按 23 没反应、仪表数字不刷新”的来源。现在改为 `car_state_t decoded = link->pending; decode(..., &decoded) → pending = decoded`，未涉及的字段保留上一次值。

另外 `main.c` 启动时打印一行 `settings: load <path> (unit=... style=... cards=...)`，方便确认到底读到了什么。

测试：新增 `test_saver_first_acc_off`（首帧即关闭也要落盘）、`test_save_creates_missing_dir`（目录不存在自动创建）；`test_save_failure_keeps_dirty` 由“不存在的目录”改为“只读目录”（root 下自动跳过断言）。`carmw_settings_tests` 102 checks 全通过，六个测试二进制全 OK。

板端 `build_board` 通过，`bin/carmw_ui_demo` MD5 `8dd229c95e3c3f3abb72ffcfed14b523`。

**板端实测结论（2026-11-05，用户确认）**：23 进入设置正常；ACC 关闭后 `/opt/car_dashboard/setting.cfg` 正常创建并写入；上电读回正确。14.6 的“设置模型 + 最小可测闭环”到此闭环，后续为 14.7 起的内容迁移。

### 14.7 步骤6.3：设置菜单与本地交互

按根分类迁移，不一次铺开所有车型菜单：

1. **根页和通用列表**：迁移布局、分类顺序、选中态、边界循环规则、开关/选项/数值编辑方式及返回行为。
2. **我的座驾**：单位、常规显示、中央卡片开关、风格/颜色入口、ECO相关设置；其中硬件背光和车辆命令先留真实接口依赖，不用本地变量冒充成功。
3. **信息选项**：行车信息中的车速、挡位、油耗、外温等开关；与中央卡片显隐分别绑定。
4. **行程资讯**：按当前车型迁移已有项目；驾驶辅助、车辆设置进入6.5命令/反馈闭环；赛道项目不自动纳入Forester。
5. **系统设置**：语言、日期时间、版本、工厂设置、清缓存、开机动画、返回。逐项确认权限与平台接口；不得用清缓存按钮顺带删除配置或业务数据。
6. **互联设置**：名称、开关、连接模式选项，后端依赖6.6；后端不可用时如实显示不可用，模拟画面明确区分。
7. 中文字体先提取所需字符、验证字号与资源占用；视觉对齐使用Qt截图，不能把无窗口对象测试当成字体/布局验收。

**验收**：每个可见设置有实际绑定、取消/确认语义与返回路径；隐藏项目不可被焦点选中；首次OK和后续OK与Qt相同；不保留能点击但假成功的入口。

### 14.8 步骤6.4：设置驱动的即时显示与覆盖层门控

**操作步骤**

1. 单位优先接现有换算/格式化链，避免再写一套：速度、里程、温度、胎压和油耗的数值与单位文字同步变化，原始车况保持不变。
2. 明暗/颜色/背光分层：主题色由UI处理，物理背光走核实后的设备/MCU接口；不能用调低页面透明度冒充背光控制。
3. 卡片以稳定业务标识维护顺序和可见集合，不依赖LVGL child序号。隐藏当前卡后选择哪一张、所有卡被关后保留哪一项，先查Qt；无确定结果则由用户确认退路。
4. 把外温、ECO、4H、车门开关等逐项接到已登记的显示规则；所有原“阶段6后置”注释逐一核对并回填状态。
5. 接雷达、盲区、紧急制动、车道偏离等门控时区分菜单可见性、MCU真实状态与本地显示偏好，不用一个布尔值代替三者。
6. 将设置页/互联显示接入通用提示及其他覆盖层的显隐条件；保留此前车门/胎压/雷达等优先级和事件时序。
7. 右舵与车色只按Qt指定对象/变换迁移，不把整个屏幕包括文字都镜像；如增加图像处理，重新记录成本。
8. 对齐风格切换后设置重用、警告剩余时长和中心卡恢复，不因组件重建重置业务状态。

**验收**：车速等输入完全不变时改设置立刻生效；当前卡被关闭、全关、重新开启、跨样式、设置遮挡中警告到期等均覆盖；保留现有回归，并补视觉/性能测量。

**本轮已完成**

- 单位（里程/温度）与中心卡片勾选即时生效（`car_settings_set_*` → notify → `ui_app_settings_refresh` → `ui_app_apply`），默认 `center_card_mask=0x3FD`，`0` 表示全部关闭。
- 设置页新增 **信息选项 → 行车信息**（Qt `PageSettingDrivingInfo`）：只迁移在 Forister1 页面真正生效的 3 项——车外温度 `out_temperature`（缺省关）、档位 `gear_on`（缺省开）、平均油耗 `avg_fuel_on`（缺省关）。均已落盘（`setting.cfg` 三个新键）并驱动仪表：档位显隐在 `page_subaru_forester1.c`，外温行同处，平均油耗行在 `page_short_term.c`。
- 按键：根页“信息选项”先 OK 锁定列表，再 OK 进入行车信息子页；开关行 OK 取反，BACK 回上一级；13 在子页回信息选项，在根页关闭设置。
- 设置页新增 **我的座驾 → 常规设置**（Qt `PageGeneralSettings`）：白天/黑夜亮度（0~100，OK 每次 +10，超过 100 归 0，对应 Qt `adjustBrightness`）、超速报警开关（`speed_limit_on`）、超速预警行（未迁移，置灰显示 `NOT SET`，仅开关打开时可用色）。背光与超速命令分别走 `car_cmd_set_backlight(2, day, night)` / `car_cmd_set_speed_warning(0, on)`，由 `ui_app_settings_set_cmd_cb` 注入板端 `car_link_exec`，PC 演示不注入时静默丢弃。
- 设置页新增 **我的座驾 → ECO 设置**（Qt `EcoSettings`）：迁移 `eco_indicator`（缺省开）与 `eco_or_fuel`（Qt `ecoOrfuel`，缺省“油门引导”）。Qt 的“显示”是 `PageListSelect` 两项子页，这里收敛为同一行 `ECO DISPLAY`，状态显示 `THROTTLE` / `FUEL`，OK 直接翻转（两项时与进子页再选等价）。仪表 `page_subaru_forester1.c` 的 `eco_lamp`（`WarnIcon/eco.bin`，516,459）按 `eco.valid && eco.enabled && eco_indicator` 显隐；ECO 卡形态按 Qt `EcoShow.showCircularMode = ecoIndicator && ecoOrfuel && ecoEnable` 决定圆环/水平。
- 设置持久化新增 4 个键：`daylight` / `nightlight` / `speed_limit_on` / `eco_indicator`（`setting.cfg`，写盘缓冲区扩到 512）；ECO 显示形态另增 `eco_or_fuel` 键（随缓冲区扩到 640）。
- 设置页新增 **我的座驾 → ECO 历史信息**（Qt `EcoavgShow.qml`）：单张 500×280 柱状图卡片，Y 轴量程 30.0 l/100km（每格 10），4 根历史柱（蓝渐变 `#4A90E2`→`#1D4E89`）+ 当前白柱 + 最佳绿柱（`#00FF7F`），柱高 = `value/300 × 180px`；底部 `CUR` / `BEST` 数值。
- 历史数据落在 `car_settings_values_t.eco_history_x10[4]`（×10，索引 0 最旧、3 最新，0 表示空），`setting.cfg` 新增 `eco_history0..3` 四个键，写盘缓冲区相应扩到 640；默认值对齐 Qt `historyModel` 的 9.0 / 10.5 / 8.0 / 9.2。
- 语义：Qt `updateHistory()` 在源码中无调用点、`onSigOk` 调 `resetAllData()`；LVGL 侧改为 **OK = 记录当前平均油耗**（`car_settings_eco_history_push`，满 4 条丢最旧一条，同值/超 300 返回 false），BACK/上下键只返回或空转。`car_settings_eco_history_reset` 保留 `resetAllData` 的全清零语义供上层调用。
- “当前”白柱取 `ui_app_apply` 注入的 `state.trip.avg_fuel_x10`（与 Eco 卡同一数据源）；“最佳”取历史非 0 项与当前值的最小值。全部 0 时柱高保底 1px。
- **14.8 覆盖层门控（2026-11-06）**：
  - 新增本地偏好 `radar_on`（Qt `SettingsMgr.radaronoff` 名字取反，实为“雷达开启”，缺省 true）与 `show_4h`（缺省 false），落盘 `setting.cfg` 两个新键（写盘缓冲扩到 768），notify 位 `CAR_SETTINGS_CHANGED_RADAR`（bit14）/`CAR_SETTINGS_CHANGED_SHOW_4H`（bit15）。
  - `ui_app_settings_refresh` 链路把两项写入视图快照；`ov_radar.c` 的 display 追加 `state->radar_on`，关闭后即使有 0x22 帧也不显示。
  - `ui_lamp_group` 新增 `ui_lamp_gate_t` 与 `ui_lamp_group_item_t.gate`；`page_subaru_forester1.c` 的码 33（`four_g.bin`）与码 36（`4WD_l.bin`）标注 `UI_LAMP_GATE_SHOW_4H`，`show_4h=false` 时保持熄灭（Qt `visible: SettingsMgr.show4h`）。
  - `ov_tips.c` 新增 `tip_allowed()`：Qt `msgId 32` 受 `show4h` 门控，0x1C 事件与 0x03 弹窗两条入口都会丢弃被门控的 id；其余提示不受影响。
  - 驾驶辅助三项走 MCU 回读闭环：`car_state_t.driver_assist` 缺省全开（`valid` 标记已收到回读），由 `car_decode_high_func()` 的 0x86 ack 更新——`len==3 && data[2]==0` 按压缩编码 `index=data[1]&0x3F` / `status=data[1]>>6`（index 1 紧急制动 / 2 车道偏离 / 3 变道盲区）；`len==3 && data[1]==0` 为扩展 ack（control 1/2 = index 32/33，本工程不保存）；其余按位图 `data[1]` bit0/1/2 回退。
  - `car_cmd_set_driver_assist()` 构造 0x16 SetGet：payload `[0x86, (status<<6)|index, 0]`；设置页 OK 只发送取反后的命令，不本地翻转，等 0x86 ack 回读更新。
  - `ov_cruise.c` 按 Qt `main.qml` 巡航页 display 表达式加入 `assist_on = emergency || lane || blind`，逐子图门控：左/右盲区 = `blind`、前车 = `showcar==1 && emergency`、左/右偏移轨道与车辆 = `lane_dep`、`max_car` 使用同一可见性判定；三项全关时整层不激活。
  - 设置页新增多级列表 **行程资讯 → 驾驶员辅助 → 驾驶辅助**：根 `ITINERARY`（根下标 0）→ `DRIVER ASSIST` → `DRIVING ASSIST`；三项显示 MCU 回读 ON/OFF，未迁移项（行程资讯 / 车辆设置 / 驻车辅助 / 底盘控制）置灰；BACK/13 逐级返回，根页 13 关闭设置。
  - 测试：`test_settings` 增加默认值/notify/落盘断言；`test_decode` 增加 0x86 压缩编码与位图回退用例；`test_overlays` 增加雷达 `radar_on`、巡航三项门控、4H 灯 `gate`、提示 id32 用例。`cmake --build build -j8` 通过，`ctest` 6/6 通过（overlays 830 checks / 0 failure）。
  - 互联会话隐藏雷达（Qt `GlobalData.items.carPlayModel`）：新增 `car_state_t.carplay_active`（缺省 false），由 `ui_app_set_carplay_active()` 注入视图快照；`ov_radar` display 增加 `!state->carplay_active`。无 ZLink SDK 时，PC 演示的互联“已连接”（`ui_app_demo_connect(2)`）会置位以便验证；板端无调用则保持 false，真实 SDK 在 6.6 接入会话回调后驱动。
  - 设置页/媒体页遮挡通用提示（Qt `visible: !settingsPageOpen && !ismedia`）：`ov_tips` 新增 `obstructed` 状态与 `ov_tips_set_obstructed()`；`page_subaru_forester1.c` 用 `ui_app_settings_is_open()`（对齐 Qt 的 settingsPageOpen，而非按车速隐藏后的可见性）与当前中心卡是否为多媒体卡（`ismedia`）合并判定。遮挡只隐藏通用提示面板，燃油低横幅与 P 挡提示条不受影响；遮挡期间播放队列与 2s 计时照常推进，解除后继续显示当前条目。
  - 设置页打开时遮挡覆盖层（Qt `main.qml`：车门 `visible: settingsPageOpen ? false : doorPageOpen`、胎压 `!settingsPageOpen`、雷达 `(doorPageOpen || settingsPageOpen) ? false : pageRadarOpen`、中心内容 `contentShow` 排除 `settingsPageOpen`）：`ov_door_apply()` 新增 `suppressed` 参数（门状态位照常记录，解除后恢复），`ov_tire_apply()`/`ov_radar_apply()` 的 suppressed 条件加入 `settings_open`，中心卡片在设置页打开时隐藏；定速巡航不受设置页影响（与 Qt 一致）。
  - 测试补充：`test_overlays` 增加 `ui_app_set_carplay_active()` 雷达隐藏/恢复用例，以及 `ov_tips_set_obstructed()` 遮挡时面板隐藏但计时继续、解除后恢复显示用例；覆盖层组合用例加入 `ui_app_settings_open()` 车门/雷达/中心内容隐藏与关闭后恢复断言。`cmake --build build -j8` 通过，`ctest` 6/6 通过（overlays 842 checks / 0 failure）；`cmake --build build_board -j8` 通过。
  - **底部灯排三项设置驱动灯（对齐 Qt `Image/` 与 QML 源码）**：Qt 侧这三盏灯不是故障码驱动，而是 `IconGeneral` 白/黄两层叠加，因此未作为普通码位接入 `car_state_warning_level`。
    - `ui_lamp_group.h` 新增门控 `UI_LAMP_GATE_EMERGENCY_BRAKING` / `UI_LAMP_GATE_BLIND_SPOT` / `UI_LAMP_GATE_LANE_DEPARTURE`；`ui_lamp_group.c` 对这些门控走独立分支：开关（`car_state_t.driver_assist`，0x16 回读）关闭时整盏熄灭，打开时常亮，并按 `high_func.lane` 选择图标（variant 0 白图 / variant 1 黄图）。
    - Qt 对应关系：紧急制动 `jin_w` 常显 + `jin_y` 在 `showcar > 0`；盲区 `hide_w`(`BSD_w.png`) + `hide_y`(`BSD_y.png`) 在 `showarea > 0`；车道偏离 `lda_w`(`real_road_w_n.png`) + `lda_y`(`real_road_y_n.png`) 在 `carState > 1`。注意 Qt `ResourceLib.qml` 的 `jin_y` 实际指向 `jin_w.png`，原工程即如此，迁移未擅自改色。
    - 未收到 0x0E 时 `lane` 字段为 0，只显示白图（与 Qt 初始状态一致）；`ui_lamp_group_apply()` 对门控灯按 `active` 差值重设图标，不会每帧重载。
    - 位置按 Qt Row 顺序累加：紧急制动 (828,630)、盲区 (886,630)、车道偏离 (944,630)，均 58×58；陡坡缓降位置见下方 XMode/Set 记录（已顺移到 1066）。
    - 测试：`test_overlays` 新增 `test_assist_lamp_gates()`，覆盖缺省白图、三项转黄图、状态清零回白、开关关闭熄灭及单项重开。`cmake --build build -j8` 通过，`ctest` 6/6 通过；`cmake --build build_board -j8` 通过；PC 模拟运行无 PNG 解码警告。
  - **底部灯排 XMode / Set（14.8 收尾）**：
    - XMode：`ui_lamp_group.h` 新增 `UI_LAMP_GATE_XMODE`。数据用 `car_state_t.drive_mode`（0x19，与 Qt 0x86 `DriveModeSelection` 的 `data[1]` 等价）；`drive_mode_valid` 为假时一律熄灭，避免结构体残留值误发光。6/7/8 依次对应 `xmode_w/snow/deepsnow`（Qt `ResourceLib.warnlight.xmode_w/xmode_s/xmode_ds`），其余模式熄灭。灯位 (1002,630) 64×64。
    - Set：Qt 是白底圆角 + 加粗 `set` 文本的 `Rectangle`，不是图片，因此页面单独创建 `ui->set_badge`（64×36 @ (1066,644)，白底、圆角 6、`lv_font_montserrat_28`），按 `cruise.cruise_state` bit1（Qt `speedok`）显隐，只在状态变化时改标志位。灯组里同时保留 `UI_LAMP_GATE_SPEED_SET_OK` 供其他样式复用。
    - 陡坡缓降随之顺移：Qt 底排顺序为 …占位 50 → XMode 64 → Set 64 → 陡坡 58，因此陡坡由 (1052,633) 改为 (1066,633)。
    - 测试：`test_assist_lamp_gates()` 扩展 XMode 的“无有效帧熄灭 / 6→7→8 换图 / 非 3 档熄灭 / 熄后保留图标”与 Set 的 bit1 显隐；另断言陡坡码 30 不受影响。PC/BOARD 构建通过，CTest 6/6，模拟运行无 PNG 警告。
- **下一步**：真实 CarPlay 会话状态需等 6.6 拿到 ZLink SDK 后替换演示标志；14.8 灯位部分已全部接线。

### 14.9 步骤6.5：车型风格与车辆设置命令闭环

**操作步骤**

1. 建立Qt车型/风格枚举到LVGL实际样式的显式映射。缺少真实样式时列为待迁移，不把general1冒充Forester后续风格。
2. 为31和设置菜单共用同一风格切换入口，处理保存、旧页面释放、最新快照补绘以及Qt要求的MCU通知。
3. 核对背光、超速提醒、驾驶辅助、系统时间等每个命令的编码、参数、反馈和失败语义；复用中间件发送能力，避免UI直接写串口。
4. MCU控制项采用“请求/等待反馈/实际值”区分；仅本地设置可即时落地。需要回执却缺少回执的操作不得立即显示车辆已执行。
5. 工厂开关只影响菜单可见性；车型专有命令必须在对应车型启用，未知车型不误发。
6. 仪表、互联及后续升级共用串口的所有权规则提前明确，避免多个模块并发打开或相互抢读。

**验收**：PC通过内存端口核对发送字节和响应状态；实机测试需用户确认并在静止台架执行；无响应/断链保持真实状态，切风格无残留对象和重复计时。

### 14.10 步骤6.6：真实CarPlay / Android Auto / HiCar迁移

**准入条件**：确认可用的SDK、有效使用授权、T507对应库和头文件、运行依赖、蓝牙/Wi-Fi服务与脚本、视频解码/显示接口。条件不齐只完成接口契约和独立模拟测试，不宣称互联可用。

**操作步骤**

1. 整理SDK会话枚举、手机类型、启动/停止/切模式、按键/触摸与回调生命周期；Qt的`connected`为会话枚举，不能压成一个真假值。
2. 拆出平台互联适配层，UI仅消费状态与发送语义命令；是否保留独立C++平台模块在验证SDK接口后决定，不把Qt对象模型硬塞到现有C界面层。
3. 先接会话状态和真实互联卡内容，再接视频。Qt蓝牙名称来自设备配置、版本来自设备文件；缺失时按明确规则显示，不拿演示值冒充实机状态。
4. 适配H.264数据到硬解，再配置视频尺寸/裁剪/位置/zorder及隐藏恢复；不得先默认复制每帧视频到LVGL软件画布。
5. 建立线程边界：SDK/解码回调传递有所有权约定的消息，UI线程更新LVGL；明确视频缓冲释放、退出后迟到回调和队列背压。
6. 完成进入互联、13退出回仪表、23确认、方向/旋转事件、26长按返回；区分隐藏投屏与停止SDK会话，不能把返回仪表一律实现为断开手机。
7. 实现模式切换和断开恢复；切风格期间处理硬件层位置与显示状态，避免旧视频层压住新仪表。
8. 音频单独确认：已核对Qt音频回调为空，需确认是否由外部服务承担；不能把有画面当成音频完成。OTA空回调不列入可用能力。
9. T113另建适配清单并重新验证架构、库、显示/解码及性能，不复制T507产物宣称适配完成。

**验收**：真实手机分别验证纳入范围的连接模式、连入/断开/重连、进退互联、按键、样式切换、退出后仪表恢复及视频层清理；分别报告视频/音频完成度，记录CPU/内存/FPS与回调生命周期。

### 14.11 步骤6.7：升级状态、界面与平台后端

**边界**：先做契约与UI/服务状态迁移，未经明确批准不执行真实APP替换、MCU烧写、系统更新或重启。普通PC回归只使用隔离临时目录与明确标注的模拟后端，不自动扫描并执行真实设备升级。

**操作步骤**

1. 核对USB检测、挂载点、包类型、目标组件/版本/架构、依赖顺序、校验、磁盘空间、电源条件及失败恢复契约；确认是否保留Qt自动触发行为，不能悄悄新增或修改入口。
2. 先定义状态机：空闲、发现候选包、校验、等待确认（如获批准采用）、执行、分项结果、总体结果、等待重启。UI只显示后端报告，不自行推测成功。
3. 迁移升级进度/结果界面及仪表遮挡；总体成功必须由真实分项结果计算。Qt已读路径计算了`overallSuccess`却发送固定true，此偏差必须明确修正，不能照搬或伪报成功。
4. APP/资源更新确认兼容关系、暂存和恢复方式；仅有car_A/car_B常量不足以证明A/B切换与回滚已实现。
5. MCU升级先确认业务串口停止、升级串口独占、失败释放、业务恢复和通信状态通知；界面不可因耗时传输冻结。Qt独立MCU入口与恢复升级部分被注释，不能按现成闭环处理。
6. 系统组件更新与整机镜像升级分开定义；Qt现有文件复制流程不等同完整OTA。
7. 校验不仅要判断传输完整性，还需确认包的可信来源及目标兼容性；MD5不能单独作为真实性认证依据。认证方案由实际平台契约决定，不臆造已有签名服务。
8. 所有组件结果到齐后再决定是否请求重启；重启策略与用户交互单独确认，不照搬“5秒自动重启”作为默认行为。
9. 板端验收前准备可恢复台架、备份和恢复途径；实际更新、掉电/恢复验证需单独批准，不由普通自动化测试触发。

**验收**：隔离模拟环境验证正常成功、普通文件校验失败、取消、磁盘写入失败、设备断开、串口不可用、分项失败、总体结果及重启条件；真实后端逐组件台架验收，未验证恢复能力时不得写“可回滚”。

### 14.12 步骤6.8：集成与最终验收

1. PC先跑全量已有CTest，再运行新增设置/状态路由/按键顺序/平台适配测试；断言从Qt业务顺序推导，不按当前运行结果改期望值。生产pump和分层接口都覆盖。
2. 逐菜单对照Qt，记录根分类、焦点、选中态、文案、中文缺字、字号、布局、返回和超时；无显示环境则标记视觉未验，不用对象树测试代替截图。
3. 用固定车辆输入覆盖“设置变化但车况不变”“警告显示中进设置/互联”“切样式后警告继续计时”“断连再恢复”。
4. 板端单实例验证真实0x20，完整覆盖13/19/20/21/22/23/26/31与0/1/2状态；同时记录预期和实际画面/动作。
5. 记录设置页打开、切门、切卡、指针运动、互联视频等场景的UI处理、绘制、显示提交耗时，含均值/P95/最大值、CPU、内存与实际FPS。20ms调度配置不作为50fps验收证据。
6. 做100次设置/样式/互联往返，检查预热后对象、定时器、文件描述符、线程及内存是否持续增长；记录测量方式和局限。
7. 按当前T507工具链构建Release，记录程序/必要资源/配置版本及校验值，说明部署和恢复步骤；未实际部署或未在板上执行的项目保持待验证。
8. 回填每个子任务的变更、实际命令/退出码、PC结果、板端证据、偏差与依赖。完成部分功能不等于阶段6整体完成。

### 14.13 执行前需确认与首批建议

需确认：

- 首批是否限定当前Forester Style1，其他车型/赛道/其他风格是否后续独立迁移。
- 是否先关闭E2/E3遗留项；若允许同步推进，应明确保留性能和视觉未验项目。
- 原Qt配置是否需要导入，是否已有可提供的实际配置；配置目录与写入权限如何安排。
- 阶段6中文字体/视觉要求，以及卡片全部关闭时的产品行为。
- T507 SDK/动态库/视频图层/音频后端是否齐备，哪些互联模式本轮必须可用。
- 升级是否包含真实后端，包来源/恢复方式、USB自动触发/确认/重启策略如何确定。

**推荐首批只执行6.0～6.2**：冻结Qt契约 → 设置模型与存储 → 设置栈和完整按键路由。首批验收目标是“普通仪表翻卡不退化、23按真实条件进入设置骨架、设置内可导航、13按Qt返回、26短长按正确、31逐事件按合法风格处理”。没有SDK时不把CarPlay占位视为完成，也不在首批引入真实升级。

本节获用户确认后再进入实现；未确认项只更新文档，不修改功能代码、不部署、不执行升级。

**2026-11-05 用户确认并已实施**：首批范围收敛为“仅设置模型 + ACC关闭统一保存”，不迁移设置菜单、不接管现有UI设置项；ACC来源为中间件cmdid 0x34；配置路径`/opt/car_dashboard/setting.cfg`，不导入Qt旧配置；ACC以收到真实帧为准；3秒窗口内重新开启允许重开且不取消已发起的保存。实现明细与PC验证见14.5“实现状态”，板端端到端保存与恢复仍待验证。

