#ifndef HYDROBRICKS_PREVAH_SNOW_ALBEDO_H
#define HYDROBRICKS_PREVAH_SNOW_ALBEDO_H

#include <cmath>

/**
 * Snow surface albedo following PREVAH's snow-age relation:
 *   albedo = 0.4 + 0.45 * exp(-0.15 * snow_age)
 * i.e. ~0.85 for fresh snow decaying toward 0.4 for old snow. Meaningful only when the
 * snowpack holds snow (callers weight it by the snow-covered fraction).
 *
 * @param snowAgeDays age of the snow surface since the last snowfall [d].
 * @return the snow albedo [0.4, 0.85].
 */
inline double PrevahSnowAlbedo(double snowAgeDays) {
    return 0.4 + 0.45 * std::exp(-0.15 * snowAgeDays);
}

#endif  // HYDROBRICKS_PREVAH_SNOW_ALBEDO_H
