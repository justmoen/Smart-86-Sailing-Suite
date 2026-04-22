#include "screen_config.h"
#include "signalk_path_config.h"
#include <string.h>

static const char* REBOOT_ID = "reboot";

bool is_screen_enabled(const char* id)
{
    if (strcmp(id, REBOOT_ID) == 0) return true;

    const auto& cfg = get_signalk_path_config();

    for (int i = 0; i < cfg.screen_config_count; i++) {
        if (strcmp(cfg.screens[i].id, id) == 0) {
            return cfg.screens[i].enabled;
        }
    }

    return true;
}

void set_screen_enabled(const char* id, bool enabled)
{
    if (strcmp(id, REBOOT_ID) == 0) return;

    auto& cfg = get_signalk_path_config();  // MUST be non-const

    for (int i = 0; i < cfg.screen_config_count; i++) {
        if (strcmp(cfg.screens[i].id, id) == 0) {
            cfg.screens[i].enabled = enabled;
            return;
        }
    }

    if (cfg.screen_config_count < MAX_SCREENS_CONFIG) {
        strcpy(cfg.screens[cfg.screen_config_count].id, id);
        cfg.screens[cfg.screen_config_count].enabled = enabled;
        cfg.screen_config_count++;
    }
}