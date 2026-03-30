#ifndef SUNRISET_H
#define SUNRISET_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Macros ---

#define days_since_2000_Jan_0(y, m, d) \
  (367L * (y) - ((7 * ((y) + (((m) + 9) / 12))) / 4) + ((275 * (m)) / 9) + (d)-730530L)

#ifndef PI
#define PI 3.1415926535897932384
#endif

#define RADEG (180.0 / PI)
#define DEGRAD (PI / 180.0)

#define sind(x) sin((x)*DEGRAD)
#define cosd(x) cos((x)*DEGRAD)
#define tand(x) tan((x)*DEGRAD)

#define atand(x) (RADEG * atan(x))
#define asind(x) (RADEG * asin(x))
#define acosd(x) (RADEG * acos(x))
#define atan2d(y, x) (RADEG * atan2(y, x))

#define INV360 (1.0 / 360.0)

// --- Convenience macros ---

#define day_length(y, m, d, lon, lat) \
  __daylen__(y, m, d, lon, lat, -35.0 / 60.0, 1)

#define day_civil_twilight_length(y, m, d, lon, lat) \
  __daylen__(y, m, d, lon, lat, -6.0, 0)

#define day_nautical_twilight_length(y, m, d, lon, lat) \
  __daylen__(y, m, d, lon, lat, -12.0, 0)

#define day_astronomical_twilight_length(y, m, d, lon, lat) \
  __daylen__(y, m, d, lon, lat, -18.0, 0)

#define sun_rise_set(y, m, d, lon, lat, rise, set) \
  __sunriset__(y, m, d, lon, lat, -35.0 / 60.0, 1, rise, set)

  // --- Twilight macros ---

#define civil_twilight(y, m, d, lon, lat, start, end) \
  __sunriset__(y, m, d, lon, lat, -6.0, 0, start, end)

#define nautical_twilight(y, m, d, lon, lat, start, end) \
  __sunriset__(y, m, d, lon, lat, -12.0, 0, start, end)

#define astronomical_twilight(y, m, d, lon, lat, start, end) \
  __sunriset__(y, m, d, lon, lat, -18.0, 0, start, end)

// --- Prototypes ---

float __daylen__(int year, int month, int day, float lon, float lat,
                 float altit, int upper_limb);

int __sunriset__(int year, int month, int day, float lon, float lat,
                 float altit, int upper_limb, float *rise, float *set);

void sunpos(float d, float *lon, float *r);
void sun_RA_dec(float d, float *RA, float *dec, float *r);

float revolution(float x);
float rev180(float x);
float GMST0(float d);

#ifdef __cplusplus
}
#endif

#endif