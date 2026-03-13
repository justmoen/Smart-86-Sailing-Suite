#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize UI system
void ui_manager_init(void);

// Switch to a specific screen
void ui_manager_load_screen(lv_updatable_screen_t* screen);

// Periodic update, call from main loop or timer
void ui_manager_update(void);

#ifdef __cplusplus
}
#endif

#endif