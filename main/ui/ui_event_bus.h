#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UI_EVENT_NONE = 0,
    UI_EVENT_PAGE_CHANGED,
    UI_EVENT_BRIGHTNESS_CHANGED
} ui_event_t;

typedef void (*ui_event_cb_t)(ui_event_t event, void *data);

void ui_event_subscribe(ui_event_cb_t cb);
void ui_event_publish(ui_event_t event, void *data);

#ifdef __cplusplus
}
#endif