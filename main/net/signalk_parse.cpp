#include "derived_data.h"

#include <math.h>
#include "sunriset.h"
#include "hw_rtc.h"
#include "ship_data_util.h"
#include <TinyGPSPlus.h>
#include <WMM_Tinier.h>

// extern globals (defined elsewhere)
extern ship_data_t shipDataModel;
extern TinyGPSPlus gps;
extern WMM_Tinier myDeclination;

/* -------------------------------------------------- */
/* Helpers                                            */
/* -------------------------------------------------- */

float norm_deg(float deg)
{
    if (deg < 0.0) return deg + 360.0;
    if (deg > 360.0) return deg - 360.0;
    return deg;
}

float norm180_deg(float deg)
{
    if (deg < -180.0) return deg + 360.0;
    if (deg > 180.0) return deg - 360.0;
    return deg;
}

/* -------------------------------------------------- */
/* Internal state                                     */
/* -------------------------------------------------- */

static RTC_DateTime RTCdate;

/* -------------------------------------------------- */
/* Sunrise / Sunset                                   */
/* -------------------------------------------------- */

static void sunrise_sunset()
{
    if ((!fresh(shipDataModel.environment.sunrise.age, TWO_MINUTES) ||
         !fresh(shipDataModel.environment.sunset.age, TWO_MINUTES)) &&
        fresh(shipDataModel.navigation.position.lat.age) &&
        fresh(shipDataModel.navigation.position.lon.age))
    {
        get_rtc_datetime(&RTCdate);

        float lon = shipDataModel.navigation.position.lon.deg;
        float lat = shipDataModel.navigation.position.lat.deg;

        int year  = (RTCdate.year % 100) + 1900;
        int month = RTCdate.month + 1;
        int day   = RTCdate.day;

        float daylen  = day_length(year, month, day, lon, lat);
        float nautlen = day_nautical_twilight_length(year, month, day, lon, lat);
        float twilight = (nautlen - daylen) / 2.0;

        shipDataModel.environment.daylight_duration.hr = daylen;
        shipDataModel.environment.daylight_duration.age = millis();

        shipDataModel.environment.nautical_twilight_duration.hr = twilight;
        shipDataModel.environment.nautical_twilight_duration.age = millis();

        float rise, set, naut_start, naut_end;

        int rs   = sun_rise_set(year, month, day, lon, lat, &rise, &set);
        int naut = nautical_twilight(year, month, day, lon, lat, &naut_start, &naut_end);

        if (rs == 0) {
            shipDataModel.environment.sunrise.hr = rise;
            shipDataModel.environment.sunrise.age = millis();
            shipDataModel.environment.sunset.hr = set;
            shipDataModel.environment.sunset.age = millis();
        }

        shipDataModel.environment.no_sunset_flag = rs;

        if (naut == 0) {
            shipDataModel.environment.nautical_twilight_start.hr = naut_start;
            shipDataModel.environment.nautical_twilight_start.age = millis();
            shipDataModel.environment.nautical_twilight_end.hr = naut_end;
            shipDataModel.environment.nautical_twilight_end.age = millis();
        }

        shipDataModel.environment.no_dark_flag = naut;
    }
}

/* -------------------------------------------------- */
/* Distance                                           */
/* -------------------------------------------------- */

static float distance_m(position_t& pos1, position_t& pos2)
{
    return gps.distanceBetween(
        pos1.lat.deg, pos1.lon.deg,
        pos2.lat.deg, pos2.lon.deg);
}

/* -------------------------------------------------- */
/* Main                                               */
/* -------------------------------------------------- */

void derive_data(void)
{
    /* --- magnetic variation --- */

    if (fresh(shipDataModel.navigation.position.lat.age) &&
        fresh(shipDataModel.navigation.position.lon.age))
    {
        get_rtc_datetime(&RTCdate);

        float lat = shipDataModel.navigation.position.lat.deg;
        float lon = shipDataModel.navigation.position.lon.deg;

        float mag_var =
            myDeclination.magneticDeclination(
                lat, lon,
                RTCdate.year % 100,
                RTCdate.month + 1,
                RTCdate.day);

        if (fabs(mag_var) > 0.00001) {
            shipDataModel.navigation.mag_var.deg = mag_var;
            shipDataModel.navigation.mag_var.age = millis();
        }
    }

    /* --- wind, drift, etc (unchanged logic) --- */
    /* keep the rest of your existing derive_data body exactly as-is */

    sunrise_sunset();
}