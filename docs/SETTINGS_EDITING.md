# 设置菜单维护

设置列表不再使用 `.def` 和宏展开。现在是三份独立的普通 C 数据：**菜单数组决定摆哪些功能，绑定表决定功能做什么，语言资源决定显示什么文字。** 修改后重新编译即可。

## 最常用：删除、移动、恢复选项

只打开 `src/ui/settings/ui_settings_menu.c`。例如行程资讯：

```c
static const ui_settings_item_id_t itinerary_menu[] = {
    UI_SETTINGS_IT_TRIP,
    UI_SETTINGS_IT_ASSIST,       // 驾驶员辅助
    UI_SETTINGS_IT_VEHICLE,      // 车辆设置
    UI_SETTINGS_ITEM_ROOT_BACK,
};
```

**删除整个“驾驶员辅助”大项：删掉 `UI_SETTINGS_IT_ASSIST,` 这一行。** 子菜单入口随之不可达；不用改 MCU 解析、绑定表、翻译，也不用修改数量和下标。以后恢复时把这一行放回来即可。

删除驻车辅助中的雷达音量：从 `parking_menu[]` 删除 `UI_SETTINGS_PARK_VOLUME,`。调整顺序直接移动数组中的行。把已经存在的功能放进同类列表，只需把它的功能 ID 加入数组。

| 页面 | 同一个文件中的数组 |
| --- | --- |
| 行程资讯（包括驾驶员辅助、车辆设置入口） | `itinerary_menu[]` |
| 驾驶员辅助下的分类入口 | `assist_menu[]` |
| 电动助力转向 | `eps_menu[]` |
| 驻车辅助 | `parking_menu[]` |
| 底盘控制 | `chassis_menu[]` |
| 驾驶辅助开关 | `driving_assist_menu[]` |
| 行车信息 | `driving_info_menu[]` |
| ECO 设置 | `eco_menu[]` |
| 转向灯 | `turn_menu[]` |
| 雨刷器 | `wiper_menu[]` |

保留每页末尾的返回项。根列表用 `UI_SETTINGS_ITEM_ROOT_BACK`，其余用 `UI_SETTINGS_ITEM_BACK`。行数由 `sizeof` 计算，控件按实际行数分配；新增已有页面内的选项不需要手动扩大控件数组。

以上十个列表已经采用这个结构。系统、我的座驾、车辆设置内部的灯光/车门/保养等旧列表及特殊详情页仍在 `ui_settings_page.c`；新增整张页面仍需接入创建、显示、按键和返回逻辑。

## 三种编号不要混淆

以雷达音量为例：

- `UI_SETTINGS_PARK_VOLUME`：功能 ID，在 `ui_settings_menu.h` 声明，与行顺序无关。
- `parking_index`：当前选中的行位置；移动数组会改变位置。需要查询时用 `ui_settings_menu_index(UI_SETTINGS_MENU_PARKING, UI_SETTINGS_PARK_VOLUME)`，找不到返回 `UI_SETTINGS_MENU_INDEX_NONE`。
- `.command_index = 17`：MCU 协议编号，不能随菜单重排改变。

界面、按键和返回通过“行位置 → 功能 ID → 绑定”查找，删除中间行不会让后面功能误发其他 MCU 命令。菜单索引不写入 MCU 或设置存储。新增功能 ID 放在枚举末尾、`UI_SETTINGS_ITEM_COUNT` 前；删除入口时保留 ID 和绑定方便复用。

## 功能怎么绑定

`src/ui/settings/ui_settings_bindings.inc` 是普通 C 结构体数组。它由页面实现包含一次，以便引用页面已有的 MCU 缓存；无需学习宏生成规则。

例如现有雷达音量：

```c
static const ui_settings_text_id_t radar_volume[] = {
    UI_TEXT_LOW, UI_TEXT_MID, UI_TEXT_HIGH
};

// model_bindings[] 中的一项
[UI_SETTINGS_PARK_VOLUME] = {
    .text_id = UI_TEXT_PARK_VOLUME,
    .kind = MODEL_MCU_CHOICE,
    .number_value = &s_pc_radar_volume,
    .command_index = 17,
    .options = radar_volume,
    .option_count = MODEL_COUNT(radar_volume),
},
```

`options` 的顺序对应 MCU 值 0、1、2；这是协议值顺序，不能为了改菜单顺序而调整它。普通开关用 `MODEL_MCU_SWITCH` 和 `.bool_value = &缓存字段`，不填选项表。`.feature_bit = 9` 表示只有工厂功能 bit9 开启时才显示，上下键也跳过隐藏行；不填或填 0 表示不限制。

本地设置用 `MODEL_LOCAL`，绑定 `car_settings_values_t` 字段偏移、已有 setter 和默认值。当前本地接口处理布尔值（包括两档 ECO 显示）；多于两档的本地数值需要增加相应读写接口。导航入口用 `MODEL_NAV` 和 `.target_view`；禁用展示行用 `MODEL_LOCKED`。

普通增删行不改通用引擎 `ui_settings_model.inc`。

## 新增一个与 MCU 交互的功能

如果 MCU 回读字段和菜单缓存已经存在，只需以下 UI 改动：

1. `ui_settings_menu.h`：在功能 ID 枚举末尾新增一个有意义的 ID。
2. `ui_settings_text.h` 与 `translations/`：新增文案 ID，并给各语言填译文；已存在的文案可直接复用。
3. `ui_settings_bindings.inc`：增加一个结构体绑定缓存字段、命令编号，以及可选的档位文本表和工厂功能位。
4. `ui_settings_menu.c`：把新功能 ID 放进对应页面的数组。

如果是一个从未接入的 MCU 功能，还要接回读。沿用 `0x16/0x86` 驻车辅助协议时：

| 文件 | 修改内容 |
| --- | --- |
| `include/carmw/car_state.h` | 给对应车况结构增加回读字段 |
| `src/car_decode.c` | 在 `car_decode_high_func` 解析 index/status，写入该字段 |
| `src/ui/settings/ui_settings_page.c` | 声明缓存；在 `ui_settings_page_set_parking_chassis` 等对应回调中加入变化比较和赋值 |
| 上述菜单、绑定、文案文件 | 增加 UI 功能和入口 |

现有传递整个结构体的接口通常不用改 `ui_app.c` 和页面头文件；新增独立数据通道时才扩展这些接口。新字段启动值非零时检查 `src/car_state.c`。新命令格式还需修改 `src/car_command.c/.h`，必要时在 `car_registry.c` 和 `car_decode_internal.h` 登记解析函数。

当前 `MODEL_MCU_SWITCH/CHOICE` 共用 `car_cmd_set_vehicle_setting`：只适用于它支持的 index 1～31、status 0～3。不同协议要新增对应动作处理，不能仅填入超范围编号。补充协议解析、命令编码、菜单按键和回读测试。

数据流如下：

```text
按 OK → 根据当前 MCU 缓存计算下一值 → command_index 发命令
MCU 回包 → car_decode.c → car_state → ui_app → 页面缓存 → 更新显示
```

MCU 设置按 OK 后等待回读，界面不会先自行翻转。删除入口不停止解析回包，也不影响仪表其他地方使用相同的 MCU 数据。

## 多语言怎么增加

菜单和绑定里只有文案 ID，不写中英文二选一。

```c
// translations/en.h
[UI_TEXT_PARK_VOLUME] = "Radar Vol",

// translations/zh_cn.h
[UI_TEXT_PARK_VOLUME] = "雷达音量",
```

修改文案只改相应语言资源；新增一条文案，先在 `ui_settings_text.h` 的 `UI_TEXT_COUNT` 前声明 ID，再填资源表。

增加法语的步骤（示例）：

1. 新建 `translations/fr.h`，定义普通资源数组 `static const char *const text_fr[UI_TEXT_COUNT] = {...};`，按相同 ID 填译文。
2. 在 `ui_settings_text.c` 包含该文件，向 `catalogs[]` 增加 `[8] = text_fr,`。8 是现有 Qt 配置里的法语编号。
3. 编译并检查实际文字宽度和字体字符覆盖。

原有语言编号不变：0 简中、1 英文、2 繁中、3 俄语、4 西语、5 德语、6 土耳其语、7 韩语、8 法语、9 意大利语、10 葡语、11 保加利亚语、12 波兰语、13 罗马尼亚语、14 荷兰语、15 捷克语、16 日语、17 希腊语、18 阿拉伯语。现已提供简中、英文；未提供的语言、缺失或空译文逐项回退英文。

旧页面的拼接文本仍由 `ui_settings_language.c` 中的英文片段到 ID 的兼容映射识别，译文已统一放到同一批语言资源中。新菜单直接使用文案 ID，不依赖英文匹配。全仪表其他场景的 `ui_i18n.c` 仍是独立模块，本次没有把它们一并改成这些设置资源。

新增字符后检查字体覆盖：`tools/qt_fonts.py` 会扫描 `translations/*.h`，生成方法见 `docs/FONT_PORT.md`。第三语言还需要能覆盖相应字符的字体；当前字库缺字会回退英文。阿拉伯语等还需额外处理文字塑形和从右到左布局，加入资源表不等于已经完成这些渲染支持。

## 验证

Windows 工作区已有的临时 MinGW 验证工程：

```powershell
cmake --build .cache/font-check/build --parallel 4
ctest --test-dir .cache/font-check/build --output-on-failure
```

Linux/板端继续使用项目原有 CMake 构建。增删或修改功能后同步调整该功能的行为断言；菜单测试通过 ID 查当前位置，不把功能 ID 当行下标。检查选项的显示顺序、返回、功能隐藏、发包内容和 MCU 回读。
