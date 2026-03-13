#include "ship_data_util.h"
#include <cstring>

bool fresh(unsigned long age, unsigned long limit) {
    return esp_timer_get_time() - age < limit && age != 0;
}

bool isSet(char *str) {
    return str != NULL && str[0] != 0;
}

bool starts_with(const char* str, const char* pre) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

const char* step_into_path(const char* path) {
    if (path == NULL) return NULL;
    const char* s = strchr(path, '/');
    return s != NULL ? ++s : NULL;
}

const char* step_into_token(const char* path) {
    if (path == NULL) return NULL;
    const char* s = strchr(path, '.');
    return s != NULL ? ++s : NULL;
}

engine_t *lookup_engine(const char *engineID) {
    int last = -1;
    for (int i = 0; i < MAX_ENGINES; i++) {
        if (strncmp(shipDataModel.propulsion.engines[i].engine_label, engineID, MAX_ENGINE_LBL_LENGTH) == 0) {
        return &shipDataModel.propulsion.engines[i];
        }
        if (shipDataModel.propulsion.engines[i].engine_label[0] != 0) {
        last++;
        }
    }
    if ((last + 1) < MAX_ENGINES) {
        strncpy((char *)(shipDataModel.propulsion.engines[last + 1].engine_label), engineID, MAX_ENGINE_LBL_LENGTH);
        return &shipDataModel.propulsion.engines[last + 1];
    }
    return NULL;
}