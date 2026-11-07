#include "test_util.h"

#include "carmw/car_registry.h"
#include "carmw/car_state.h"

void test_registry_suite(void)
{
    size_t count = car_registry_count();

    CHECK(count > 40);
    CHECK(count < 256);

    for(size_t i = 0; i < count; ++i) {
        const car_cmd_desc_t *desc = car_registry_at(i);

        CHECK(desc != NULL);
        if(desc == NULL) {
            continue;
        }

        CHECK(desc->name != NULL);
        CHECK(desc->min_len <= desc->max_len);
        CHECK(desc->max_len <= CAR_MAX_PAYLOAD);

        /* 不变式：会改状态的命令必须挂解码器 */
        if(desc->mask != 0) {
            CHECK(desc->decode != NULL);
        }

        /* 表必须严格升序，二分查找才成立 */
        if(i > 0) {
            const car_cmd_desc_t *prev = car_registry_at(i - 1);
            CHECK(prev->cmd_id < desc->cmd_id);
        }

        CHECK(car_registry_find(desc->cmd_id) == desc);
    }

    CHECK(car_registry_at(count) == NULL);
    CHECK(car_registry_find(0x99) == NULL);
    CHECK(strcmp(car_cmd_name(0x99), "Unknown") == 0);
    CHECK(strcmp(car_cmd_name(0x04), "Speed and RPM") == 0);

    /* 定长命令的长度边界 */
    CHECK(car_registry_accept(0x04, 4, NULL));
    CHECK(!car_registry_accept(0x04, 3, NULL));
    CHECK(!car_registry_accept(0x04, 5, NULL));

    CHECK(car_registry_accept(0x22, 11, NULL));
    CHECK(!car_registry_accept(0x22, 10, NULL));
    CHECK(!car_registry_accept(0x22, 12, NULL));

    CHECK(car_registry_accept(0x15, 3, NULL));
    CHECK(!car_registry_accept(0x15, 4, NULL));

    /* 变长命令 */
    CHECK(car_registry_accept(0x02, 1, NULL));
    CHECK(car_registry_accept(0x02, 200, NULL));
    CHECK(!car_registry_accept(0x02, 0, NULL));

    /* 未登记命令：<= 256 放行，给协议扩展留余地 */
    CHECK(car_registry_accept(0x99, 0, NULL));
    CHECK(car_registry_accept(0x99, CAR_UNKNOWN_MAX_PAYLOAD, NULL));
    CHECK(!car_registry_accept(0x99, CAR_UNKNOWN_MAX_PAYLOAD + 1U, NULL));

    /* 烟雾测试：所有解码器在最大长度全零载荷下都不能越界/崩溃 */
    {
        static uint8_t zeros[CAR_MAX_PAYLOAD];
        car_state_t state;

        car_state_init(&state);

        for(size_t i = 0; i < count; ++i) {
            const car_cmd_desc_t *desc = car_registry_at(i);

            if(desc == NULL || desc->decode == NULL) {
                continue;
            }

            uint16_t len = desc->max_len;

            if(len > CAR_MAX_PAYLOAD) {
                len = (uint16_t)CAR_MAX_PAYLOAD;
            }

            (void)desc->decode(zeros, len, &state);
        }

        CHECK(1);
    }
}
