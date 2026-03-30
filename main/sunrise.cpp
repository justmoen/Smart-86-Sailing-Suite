#include "sunriset.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- __sunriset__ ---
int __sunriset__(int year, int month, int day, float lon, float lat,
                 float altit, int upper_limb, float *trise, float *tset)
{
    float d = days_since_2000_Jan_0(year, month, day) + 0.5 - lon / 360.0;

    float sidtime = revolution(GMST0(d) + 180.0 + lon);

    float sr, sRA, sdec;
    sun_RA_dec(d, &sRA, &sdec, &sr);

    float tsouth = 12.0 - rev180(sidtime - sRA) / 15.0;
    float sradius = 0.2666 / sr;

    if (upper_limb)
        altit -= sradius;

    float cost = (sind(altit) - sind(lat) * sind(sdec)) /
                 (cosd(lat) * cosd(sdec));

    float t;
    int rc = 0;

    if (cost >= 1.0) {
        rc = -1;
        t = 0.0;
    } else if (cost <= -1.0) {
        rc = +1;
        t = 12.0;
    } else {
        t = acosd(cost) / 15.0;
    }

    *trise = tsouth - t;
    *tset  = tsouth + t;

    return rc;
}

// --- __daylen__ ---
float __daylen__(int year, int month, int day, float lon, float lat,
                 float altit, int upper_limb)
{
    float d = days_since_2000_Jan_0(year, month, day) + 0.5 - lon / 360.0;

    float obl_ecl = 23.4393 - 3.563E-7 * d;

    float slon, sr;
    sunpos(d, &slon, &sr);

    float sin_sdecl = sind(obl_ecl) * sind(slon);
    float cos_sdecl = sqrt(1.0 - sin_sdecl * sin_sdecl);

    float sradius = 0.2666 / sr;

    if (upper_limb)
        altit -= sradius;

    float cost = (sind(altit) - sind(lat) * sin_sdecl) /
                 (cosd(lat) * cos_sdecl);

    float t;

    if (cost >= 1.0)
        t = 0.0;
    else if (cost <= -1.0)
        t = 24.0;
    else
        t = (2.0 / 15.0) * acosd(cost);

    return t;
}

// --- sunpos ---
void sunpos(float d, float *lon, float *r)
{
    float M = revolution(356.0470 + 0.9856002585 * d);
    float w = 282.9404 + 4.70935E-5 * d;
    float e = 0.016709 - 1.151E-9 * d;

    float E = M + e * RADEG * sind(M) * (1.0 + e * cosd(M));

    float x = cosd(E) - e;
    float y = sqrt(1.0 - e * e) * sind(E);

    *r = sqrt(x * x + y * y);

    float v = atan2d(y, x);
    *lon = v + w;

    if (*lon >= 360.0)
        *lon -= 360.0;
}

// --- sun_RA_dec ---
void sun_RA_dec(float d, float *RA, float *dec, float *r)
{
    float lon;
    sunpos(d, &lon, r);

    float x = *r * cosd(lon);
    float y = *r * sind(lon);

    float obl_ecl = 23.4393 - 3.563E-7 * d;

    float z = y * sind(obl_ecl);
    y = y * cosd(obl_ecl);

    *RA  = atan2d(y, x);
    *dec = atan2d(z, sqrt(x * x + y * y));
}

// --- revolution ---
float revolution(float x)
{
    return (x - 360.0 * floor(x * INV360));
}

// --- rev180 ---
float rev180(float x)
{
    return (x - 360.0 * floor(x * INV360 + 0.5));
}

// --- GMST0 ---
float GMST0(float d)
{
    return revolution(
        (180.0 + 356.0470 + 282.9404) +
        (0.9856002585 + 4.70935E-5) * d
    );
}

  #ifdef __cplusplus
} /*extern "C"*/
#endif