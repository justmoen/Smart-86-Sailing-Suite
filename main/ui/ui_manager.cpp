#include "navigation.h"

#include "ui_manager.h"
#include "signalk_path_config.h"
#include "screens/ui_wind.h"
#include "screens/ui_engine.h"
#include "screens/ui_depth.h"
#include "screens/ui_speed.h"
#include "screens/ui_compass.h"
#include "screens/ui_gps.h"
#include "screens/ui_tanks.h"
#include "screens/ui_heel.h"
#include "screens/ui_reboot.h"
#include "ui_manager.h"
#include "screen_config.h"
extern int current_index;  // Make accessible for notify_config_changed

// Maximum screens: 1 (wind) + 8 (engines) + 6 (depth/speed/compass/gps/tanks/heel) + 1 (reboot) = 16
#define MAX_SCREENS 16

static lv_updatable_screen_t* screens[MAX_SCREENS];
static int screen_count = 0;

int current_index = 0;
static lv_updatable_screen_t *current_screen = nullptr;
static int pending_screen_index = -1;  // Deferred screen transition
static int pending_create_screen_index = -1;  // Deferred screen creation
static int pending_reinit_flag = 0;  // 1 = full reinit requested
static int pending_load_screen_index = -1;   // Deferred screen load (separate from creation)
static bool reinit_in_progress = false;

// Initialize screens array with proper number of engine screens
void init_screens_array() {
    const auto& config = get_signalk_path_config();
    int engine_screen_count = config.num_engines;
    if (engine_screen_count < 1) engine_screen_count = 1;
    if (engine_screen_count > 8) engine_screen_count = 8;
    
    int idx = 0;
    
    if (is_screen_enabled("wind")) {
        screens[idx++] = &windScreen;
    }
    
    // Create and add engine screens dynamically
    lv_updatable_screen_t* engine_screens[8];
    create_engine_screens(engine_screens, &engine_screen_count);
    for (int i = 0; i < engine_screen_count; i++) {
        char id[20];
        sprintf(id, "engine_%d", i);

        if (is_screen_enabled(id)) {
            screens[idx++] = engine_screens[i];
        }
    }
    
    // Add remaining screens
    if (is_screen_enabled("depth"))
        screens[idx++] = &depthScreen;

    if (is_screen_enabled("speed"))
        screens[idx++] = &speedScreen;

    if (is_screen_enabled("compass"))
        screens[idx++] = &compassScreen;

    if (is_screen_enabled("gps"))
        screens[idx++] = &gpsScreen;

    if (is_screen_enabled("tanks"))
        screens[idx++] = &tanksScreen;

    if (is_screen_enabled("heel"))
        screens[idx++] = &heelScreen;

    // always include reboot
    screens[idx++] = &rebootScreen;
    
    screen_count = idx;
}

static void create_if_needed(lv_updatable_screen_t *scr)
{
    if (!scr || scr->created) {
        return;
    }

    if (!scr->screen) {
        scr->screen = lv_obj_create(NULL);
        apply_screen_style(scr->screen);
        lv_obj_add_event_cb(scr->screen, gesture_event_cb, LV_EVENT_ALL, NULL);
    }

    // Screen creation happens in main loop task, not LVGL task.
    if (scr->create_cb)
        scr->create_cb(scr);

    scr->created = true;
}

void ui_manager_show(lv_updatable_screen_t *scr)
{
    if (!scr) return;

    if (scr->destroy_cb) {
        scr->destroy_cb(scr);
    }

    create_if_needed(scr);

    current_screen = scr;

    save_last_screen(current_index);
    lv_scr_load(scr->screen);
}

void ui_manager_next()
{
    if (screen_count <= 0) {
        return;
    }

    // Only queue one pending transition at a time - discard intermediate swipes
    // Calculate the target screen
    int next_index = current_index + 1;
    if (next_index >= screen_count)
        next_index = 0;
    
    // Only set if no transition is already pending (prevents accumulation of swipes)
    if (pending_screen_index < 0) {
        pending_screen_index = next_index;
        
        // Mark for creation if needed
        if (screens[next_index] && !screens[next_index]->created) {
            pending_create_screen_index = next_index;
        }
    }
}

void ui_manager_prev()
{
    if (screen_count <= 0) {
        return;
    }

    // Only queue one pending transition at a time - discard intermediate swipes
    // Calculate the target screen
    int prev_index = current_index - 1;
    if (prev_index < 0)
        prev_index = screen_count - 1;
    
    // Only set if no transition is already pending (prevents accumulation of swipes)
    if (pending_screen_index < 0) {
        pending_screen_index = prev_index;
        
        // Mark for creation if needed
        if (screens[prev_index] && !screens[prev_index]->created) {
            pending_create_screen_index = prev_index;
        }
    }
}

void ui_manager_init()
{
    // Initialize screens array with dynamic engine screens based on config
    init_screens_array();
    
    for (int i = 0; i < screen_count; i++) {
        screens[i]->created = false;
        screens[i]->screen = nullptr;
    }

    current_index = load_last_screen();

    if (current_index < 0 || current_index >= screen_count) {
        current_index = 0;
    }
    default_settings();
    
    // Create only the current screen on init
    create_if_needed(screens[current_index]);
    current_screen = screens[current_index];
    save_last_screen(current_index);
    lv_scr_load(screens[current_index]->screen);
}

void ui_manager_update()
{
    if (!current_screen) return;

    // Only update the current screen, don't do transitions here
    // Transitions happen in ui_manager_process_deferred_screen_load()
    if (current_screen->update_cb)
        current_screen->update_cb(current_screen);
}

// Process deferred screen creation (call from main loop OUTSIDE display lock)
void ui_manager_process_deferred_screen_creation()
{
    if (pending_reinit_flag) {
        pending_reinit_flag = 0;
        ui_manager_reinit_screens();
        return;
    }
    
    if (pending_create_screen_index >= 0 && pending_create_screen_index < screen_count) {
        create_if_needed(screens[pending_create_screen_index]);
        pending_create_screen_index = -1;
    }
    
    // Always queue the load if there's a pending screen transition
    // IMPORTANT: Clear pending_screen_index immediately when queueing the load to prevent
    // race conditions where a new gesture could queue another screen before we finish loading this one
    if (pending_screen_index >= 0 && pending_load_screen_index < 0) {
        pending_load_screen_index = pending_screen_index;
        pending_screen_index = -1;  // Clear immediately to prevent double-advance race condition
    }
}

// Process deferred screen load (call from main loop OUTSIDE display lock)
void ui_manager_process_deferred_screen_load()
{
    if (pending_load_screen_index >= 0 && pending_load_screen_index < screen_count) {
        lv_updatable_screen_t *next_screen = screens[pending_load_screen_index];
        
        current_index = pending_load_screen_index;
        pending_load_screen_index = -1;
        // Note: pending_screen_index is already cleared in ui_manager_process_deferred_screen_creation()
        current_screen = next_screen;
        save_last_screen(current_index);
        
        // Load screen without animation, outside of render lock
        lv_scr_load_anim(next_screen->screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    }
}

void ui_manager_reinit_screens() {
    if (reinit_in_progress) {
        return;
    }
    reinit_in_progress = true;

    lv_updatable_screen_t *old_screens[MAX_SCREENS] = {nullptr};
    int old_screen_count = screen_count;
    for (int i = 0; i < old_screen_count; i++) {
        old_screens[i] = screens[i];
        screens[i] = nullptr;
    }

    // Destroy all existing screens before the array is rebuilt.
    for (int i = 0; i < old_screen_count; i++) {
        lv_updatable_screen_t *scr = old_screens[i];
        if (!scr) continue;
        if (scr->destroy_cb) {
            scr->destroy_cb(scr);
        }
        if (scr->screen) {
            lv_obj_del(scr->screen);
            scr->screen = nullptr;
        }
        scr->created = false;
    }

    // Reset state
    screen_count = 0;
    pending_screen_index = -1;
    pending_create_screen_index = -1;
    pending_load_screen_index = -1;
    pending_reinit_flag = 0;

    // Reinitialize screens array with current config.
    init_screens_array();

    for (int i = 0; i < screen_count; i++) {
        if (!screens[i]) continue;
        screens[i]->created = false;
        screens[i]->screen = nullptr;
    }

    // Restore current_index (clamped) and create the screen safely.
    current_screen = nullptr;
    current_index = (current_index >= 0 && current_index < screen_count) ? current_index : 0;
    if (screen_count > 0) {
        create_if_needed(screens[current_index]);
        current_screen = screens[current_index];
        save_last_screen(current_index);
        if (screens[current_index] && screens[current_index]->screen) {
            lv_scr_load(screens[current_index]->screen);
        }
    } else {
        current_index = 0;
    }

    reinit_in_progress = false;
}

void notify_config_changed() {
    if (reinit_in_progress) {
        return;
    }
    pending_reinit_flag = 1;
}
