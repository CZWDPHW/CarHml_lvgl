/*
 * ui_session.c - 跨样式存活的界面会话状态
 */
#include "ui/ui_session.h"

static ov_tire_event_t s_tire_event;

void ui_session_reset(void)
{
    ov_tire_event_reset(&s_tire_event);
}

void ui_session_apply(const car_state_t *state)
{
    ov_tire_event_apply(&s_tire_event, state);
}

const ov_tire_event_t *ui_session_tire_event(void)
{
    return &s_tire_event;
}
