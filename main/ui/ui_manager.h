#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_screens.h"

void ui_manager_init(void);
void ui_manager_update(void);
void ui_next_screen(void);

#ifdef __cplusplus
}
#endif

#endif