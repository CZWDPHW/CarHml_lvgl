#include "test_util.h"

#include "carmw/car_state.h"

void test_state_suite(void)
{
    /* ---- 初始化 ---- */
    {
        car_state_t st;

        memset(&st, 0xA5, sizeof(st)); /* 脏数据 */
        car_state_init(&st);

        CHECK(st.speed_rpm.speed_kph == 0);
        CHECK(st.coolant.display == CAR_COOLANT_SHOW_VALUE);
        CHECK(st.gear.text[0] == '\0');
        CHECK(!st.speed_rpm.valid);
        CHECK(!st.warnings.valid);
        for(unsigned i = 0; i < CAR_WARNING_SLOTS; ++i) {
            CHECK(st.warnings.levels[i] == CAR_WARN_OFF);
        }
        CHECK(st.tires.sequence == 0);
        CHECK(!st.high_func.lane.valid);
        CHECK(st.high_func.lane.lane_center == 0);
        CHECK(st.high_func.lane.car_state == 0);
        CHECK(st.high_func.lane.show_area == 0);
        CHECK(st.high_func.lane.show_car == 0);
    }

    /* ---- 快照存储 ---- */
    {
        car_state_store_t store;
        car_state_view_t view;
        car_state_t sample;

        car_state_store_init(&store);
        car_state_init(&sample);
        sample.speed_rpm.speed_kph = 88;
        sample.speed_rpm.valid = true;

        car_state_store_apply(&store, &sample, CAR_CH_SPEED_RPM);

        car_state_store_view(&store, &view);
        CHECK(view.has_data);
        CHECK(view.version == 1);
        CHECK(view.changed == CAR_CH_SPEED_RPM);
        CHECK(view.state.speed_rpm.speed_kph == 88);

        /* 取过一次之后变更掩码清零，但版本号不变 */
        car_state_store_view(&store, &view);
        CHECK(view.changed == 0);
        CHECK(view.version == 1);

        /* 再来一组，掩码累加 */
        sample.fuel.percent = 42;
        car_state_store_apply(&store, &sample, CAR_CH_FUEL);
        car_state_store_view(&store, &view);
        CHECK(view.version == 2);
        CHECK(view.changed == CAR_CH_FUEL);
        CHECK(view.state.fuel.percent == 42);
        CHECK(view.state.speed_rpm.speed_kph == 88); /* 旧字段仍在 */

        car_state_store_deinit(&store);
    }

    /* ---- 故障灯取值：只读持久 levels，诊断字段不参与回退 ---- */
    {
        car_state_t st;

        car_state_init(&st);
        st.warnings.levels[3] = (uint8_t)CAR_WARN_BLINK_FAST;
        st.warnings.levels[9] = (uint8_t)CAR_WARN_BLINK_SLOW;
        st.warnings.levels[4] = (uint8_t)CAR_WARN_ON;
        CHECK(car_state_warning_level(&st, 3) == CAR_WARN_BLINK_FAST);
        CHECK(car_state_warning_level(&st, 9) == CAR_WARN_BLINK_SLOW);
        CHECK(car_state_warning_level(&st, 4) == CAR_WARN_ON);

        st.warnings.global_valid = true;
        st.warnings.global_state = (uint8_t)CAR_WARN_ON;
        st.warnings.list_valid = true;
        st.warnings.list_state = (uint8_t)CAR_WARN_BLINK_FAST;
        st.warnings.list_count = 2;
        st.warnings.list[0] = 3;
        st.warnings.list[1] = 9;
        st.warnings.levels[3] = (uint8_t)CAR_WARN_OFF;

        CHECK(car_state_warning_level(&st, 3) == CAR_WARN_OFF);
        CHECK(car_state_warning_level(&st, 9) == CAR_WARN_BLINK_SLOW);
        CHECK(car_state_warning_level(&st, 4) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 5) == CAR_WARN_OFF);
        CHECK(car_state_warning_level(NULL, 3) == CAR_WARN_OFF);
    }

    /* ---- 档位文本解析 ---- */
    {
        CHECK(car_gear_from_text("P", CAR_GEAR_N) == CAR_GEAR_P);
        CHECK(car_gear_from_text("R", CAR_GEAR_N) == CAR_GEAR_R);
        CHECK(car_gear_from_text("D4", CAR_GEAR_N) == CAR_GEAR_D);
        CHECK(car_gear_from_text("", CAR_GEAR_S) == CAR_GEAR_S);
        CHECK(car_gear_from_text(NULL, CAR_GEAR_S) == CAR_GEAR_S);
    }
}
