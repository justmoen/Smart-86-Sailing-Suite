#include "ui_event_bus.h"

#define MAX_SUBSCRIBERS 8

static ui_event_cb_t subscribers[MAX_SUBSCRIBERS];

void ui_event_subscribe(ui_event_cb_t cb)
{
    for(int i=0;i<MAX_SUBSCRIBERS;i++)
    {
        if(!subscribers[i])
        {
            subscribers[i] = cb;
            return;
        }
    }
}

void ui_event_publish(ui_event_t event, void *data)
{
    for(int i=0;i<MAX_SUBSCRIBERS;i++)
    {
        if(subscribers[i])
            subscribers[i](event, data);
    }
}