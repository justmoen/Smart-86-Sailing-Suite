#ifndef SHIP_DATA_UTIL_H
#define SHIP_DATA_UTIL_H

#include "ship_data_model.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TWO_MINUTES 120000
#define LONG_EXPIRE_TO 172800000

#define NM_TO_METERS 1852.0

  bool fresh(unsigned long age, unsigned long limit = 5000);
  bool isSet(char *str);
  bool starts_with(const char* str, const char* pre);
  const char* step_into_path(const char* path);
  const char* step_into_token(const char* path);
  engine_t *lookup_engine(const char *engineID);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
