/*
 * car_registry.h - 命令字典（表驱动）
 *
 * 一张表同时承担三件事，避免"长度校验"和"解码"两处各写一遍而慢慢漂移：
 *   1. 帧可行性判断（喂给 car_parser 当 policy）
 *   2. 载荷长度边界（min/max）
 *   3. 载荷解码函数 + 变更掩码
 *
 * 新增一条命令 = 在 car_registry.c 的表里加一行 + 写一个解码函数，
 * 其余分支（解析、快照、UI 掩码）都不用动。
 */
#ifndef CARMW_CAR_REGISTRY_H
#define CARMW_CAR_REGISTRY_H

#include "carmw/car_state.h"
#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 未登记命令的最大可接受长度。
 * 太小会丢掉将来新增的命令，太大会让假帧头长时间占用解析窗口；
 * 256 是折中值：既给扩展留了余量，又能在 115200 波特率下 25ms 内判定完。
 */
#define CAR_UNKNOWN_MAX_PAYLOAD 256U

typedef bool (*car_decode_fn)(const uint8_t *data, uint16_t len, car_state_t *state);

typedef struct {
    uint8_t cmd_id;
    uint16_t min_len;
    uint16_t max_len;
    uint64_t mask;         /* 该命令影响哪些状态组；0 表示不改状态 */
    car_decode_fn decode;  /* NULL 表示只做长度放行，不解析 */
    const char *name;
} car_cmd_desc_t;

/* 未登记命令返回 NULL。表按 cmd_id 升序排列，内部二分查找。 */
const car_cmd_desc_t *car_registry_find(uint8_t cmd_id);

const car_cmd_desc_t *car_registry_at(size_t index);

size_t car_registry_count(void);

/* 永远返回可打印名称（未登记时返回 "Unknown"）。 */
const char *car_cmd_name(uint8_t cmd_id);

/*
 * 帧可行性：已登记命令按 min/max 卡；未登记命令只放行 <= 256 字节。
 * 直接可用作 car_parser_policy_fn。
 */
bool car_registry_accept(uint8_t cmd_id, uint16_t len, void *user);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_REGISTRY_H */
