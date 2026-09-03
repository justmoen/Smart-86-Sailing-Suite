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
#include "screen_config.h"
#include "esp_lv_adapter.h"

extern int current_index;

#define MAX_SCREENS 16

static lv_updatable_screen_t* screens[MAX_SCREENS];

static int screen_count = 0;

int current_index = 0;

static lv_updatable_screen_t *current_screen = nullptr;

static int pending_screen_index = -1;
static int pending_create_screen_index = -1;
static int pending_reinit_flag = 0;
static int pending_load_screen_index = -1;

static bool reinit_in_progress = false;


/* --------------------------------------------------------------------------
 * LVGL locking
 * -------------------------------------------------------------------------- */

static bool ui_lock()
{
    return esp_lv_adapter_lock(-1) == ESP_OK;
}


static void ui_unlock()
{
    esp_lv_adapter_unlock();
}


/* --------------------------------------------------------------------------
 * Initialize screens array
 * -------------------------------------------------------------------------- */

void init_screens_array()
{
    const auto& config =
        get_signalk_path_config();

    int engine_screen_count =
        config.num_engines;

    if(engine_screen_count < 1)
        engine_screen_count = 1;

    if(engine_screen_count > 8)
        engine_screen_count = 8;

    int idx = 0;


    if(is_screen_enabled("wind"))
    {
        screens[idx++] =
            &windScreen;
    }


    lv_updatable_screen_t* engine_screens[8];

    create_engine_screens(
        engine_screens,
        &engine_screen_count
    );


    for(int i = 0;
        i < engine_screen_count;
        i++)
    {
        char id[20];

        sprintf(
            id,
            "engine_%d",
            i
        );

        if(is_screen_enabled(id))
        {
            screens[idx++] =
                engine_screens[i];
        }
    }


    if(is_screen_enabled("depth"))
        screens[idx++] = &depthScreen;

    if(is_screen_enabled("speed"))
        screens[idx++] = &speedScreen;

    if(is_screen_enabled("compass"))
        screens[idx++] = &compassScreen;

    if(is_screen_enabled("gps"))
        screens[idx++] = &gpsScreen;

    if(is_screen_enabled("tanks"))
        screens[idx++] = &tanksScreen;

    if(is_screen_enabled("heel"))
        screens[idx++] = &heelScreen;


    /*
     * Always include reboot.
     */
    screens[idx++] =
        &rebootScreen;


    screen_count =
        idx;
}


/* --------------------------------------------------------------------------
 * Create screen if necessary
 * -------------------------------------------------------------------------- */

static void create_if_needed(
    lv_updatable_screen_t *scr
)
{
    if(!scr || scr->created)
    {
        return;
    }


    if(!scr->screen)
    {
        scr->screen =
            lv_obj_create(NULL);


        apply_screen_style(
            scr->screen
        );


        lv_obj_add_event_cb(
            scr->screen,
            gesture_event_cb,
            LV_EVENT_ALL,
            NULL
        );
    }


    if(scr->create_cb)
    {
        scr->create_cb(scr);
    }


    scr->created =
        true;
}


/* --------------------------------------------------------------------------
 * Show screen
 * -------------------------------------------------------------------------- */

void ui_manager_show(
    lv_updatable_screen_t *scr
)
{
    if(!scr)
        return;


    if(!ui_lock())
        return;


    if(scr->destroy_cb)
    {
        scr->destroy_cb(scr);
    }


    create_if_needed(scr);

    current_screen =
        scr;


    save_last_screen(
        current_index
    );


    lv_screen_load(
        scr->screen
    );


    ui_unlock();
}


/* --------------------------------------------------------------------------
 * Next screen
 * -------------------------------------------------------------------------- */

void ui_manager_next()
{
    if(screen_count <= 0)
        return;


    int next_index =
        current_index + 1;


    if(next_index >= screen_count)
        next_index = 0;

    if(pending_screen_index < 0)
    {
        pending_screen_index =
            next_index;

        if(
            screens[next_index] &&
            !screens[next_index]->created
        )
        {
            pending_create_screen_index =
                next_index;
        }
    }
}


/* --------------------------------------------------------------------------
 * Previous screen
 * -------------------------------------------------------------------------- */

void ui_manager_prev()
{
    if(screen_count <= 0)
        return;


    int prev_index =
        current_index - 1;


    if(prev_index < 0)
        prev_index =
            screen_count - 1;

    if(pending_screen_index < 0)
    {
        pending_screen_index =
            prev_index;

        if(
            screens[prev_index] &&
            !screens[prev_index]->created
        )
        {
            pending_create_screen_index =
                prev_index;
        }
    }
}


/* --------------------------------------------------------------------------
 * Initialize UI manager
 * -------------------------------------------------------------------------- */

void ui_manager_init()
{
    if(!ui_lock())
        return;


    /*
     * Register the persistent input-device callbacks.
     *
     * This must happen before the first screen is loaded.
     */
    navigation_init();


    init_screens_array();


    for(int i = 0;
        i < screen_count;
        i++)
    {
        screens[i]->created =
            false;

        screens[i]->screen =
            nullptr;
    }


    current_index =
        load_last_screen();


    if(
        current_index < 0 ||
        current_index >= screen_count
    )
    {
        current_index = 0;
    }


    default_settings();


    create_if_needed(
        screens[current_index]
    );


    current_screen =
        screens[current_index];


    save_last_screen(
        current_index
    );


    lv_screen_load(
        screens[current_index]->screen
    );


    ui_unlock();
}


/* --------------------------------------------------------------------------
 * Update current screen
 * -------------------------------------------------------------------------- */

void ui_manager_update()
{
    if(!current_screen)
        return;


    if(!ui_lock())
        return;


    if(current_screen->update_cb)
    {
        current_screen->update_cb(
            current_screen
        );
    }


    ui_unlock();
}


/* --------------------------------------------------------------------------
 * Process deferred screen creation
 * -------------------------------------------------------------------------- */

void ui_manager_process_deferred_screen_creation()
{
    if(!ui_lock())
        return;


    if(pending_reinit_flag)
    {
        pending_reinit_flag = 0;


        /*
         * Reinit assumes the LVGL lock is already held.
         */
        ui_manager_reinit_screens();


        ui_unlock();

        return;
    }


    if(
        pending_create_screen_index >= 0 &&
        pending_create_screen_index < screen_count
    )
    {
        create_if_needed(
            screens[
                pending_create_screen_index
            ]
        );


        pending_create_screen_index =
            -1;
    }


    if(
        pending_screen_index >= 0 &&
        pending_load_screen_index < 0
    )
    {
        pending_load_screen_index =
            pending_screen_index;


        pending_screen_index =
            -1;
    }


    ui_unlock();
}


/* --------------------------------------------------------------------------
 * Process deferred screen load
 * -------------------------------------------------------------------------- */

void ui_manager_process_deferred_screen_load()
{
    if(!ui_lock())
        return;


    if(
        pending_load_screen_index >= 0 &&
        pending_load_screen_index < screen_count
    )
    {
        lv_updatable_screen_t *next_screen =
            screens[
                pending_load_screen_index
            ];


        current_index =
            pending_load_screen_index;


        pending_load_screen_index =
            -1;


        current_screen =
            next_screen;


        save_last_screen(
            current_index
        );


        lv_screen_load_anim(
            next_screen->screen,
            LV_SCR_LOAD_ANIM_NONE,
            0,
            0,
            false
        );
    }


    ui_unlock();
}


/* --------------------------------------------------------------------------
 * Reinitialize screens
 *
 * Called while the LVGL lock is already held.
 * -------------------------------------------------------------------------- */

void ui_manager_reinit_screens()
{
    if(reinit_in_progress)
    {
        return;
    }


    reinit_in_progress =
        true;


    lv_updatable_screen_t *old_screens[
        MAX_SCREENS
    ] = {nullptr};


    int old_screen_count =
        screen_count;


    for(
        int i = 0;
        i < old_screen_count;
        i++
    )
    {
        old_screens[i] =
            screens[i];

        screens[i] =
            nullptr;
    }


    for(
        int i = 0;
        i < old_screen_count;
        i++
    )
    {
        lv_updatable_screen_t *scr =
            old_screens[i];


        if(!scr)
            continue;


        if(scr->destroy_cb)
        {
            scr->destroy_cb(scr);
        }


        if(scr->screen)
        {
            lv_obj_delete(
                scr->screen
            );

            scr->screen =
                nullptr;
        }


        scr->created =
            false;
    }


    screen_count = 0;

    pending_screen_index = -1;

    pending_create_screen_index = -1;

    pending_load_screen_index = -1;

    pending_reinit_flag = 0;


    init_screens_array();


    for(
        int i = 0;
        i < screen_count;
        i++
    )
    {
        if(!screens[i])
            continue;


        screens[i]->created =
            false;

        screens[i]->screen =
            nullptr;
    }


    current_screen =
        nullptr;


    if(
        current_index >= 0 &&
        current_index < screen_count
    )
    {
        /* Keep current index. */
    }
    else
    {
        current_index = 0;
    }


    if(screen_count > 0)
    {
        create_if_needed(
            screens[current_index]
        );


        current_screen =
            screens[current_index];


        save_last_screen(
            current_index
        );


        if(
            screens[current_index] &&
            screens[current_index]->screen
        )
        {
            lv_screen_load(
                screens[current_index]->screen
            );
        }
    }
    else
    {
        current_index = 0;
    }


    reinit_in_progress =
        false;
}


/* --------------------------------------------------------------------------
 * Notify configuration changed
 * -------------------------------------------------------------------------- */

void notify_config_changed()
{
    if(reinit_in_progress)
    {
        return;
    }


    pending_reinit_flag =
        1;
}