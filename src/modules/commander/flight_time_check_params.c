#include <nuttx/config.h>
#include <systemlib/param/param.h>

/**
 * Enable flight time check. 0 = disable, any non-zero value = enable.
 */
PARAM_DEFINE_INT32(A_FTC_DO, 1);

/**
 * Configure, in degrees, the maximum tilt allowed during takeoff. Limited to [FTC_TTILT_DEG_MIN;FTC_TTILT_DEG_MAX].
 */
PARAM_DEFINE_INT32(A_FTC_TTILT_DEG, 55);

/**
 * Configure, in meters, the minimum altitude to be gained in takeoff. Limited to [FTC_TALT_M_MIN;FTC_TALT_M_MAX].
 */
PARAM_DEFINE_FLOAT(A_FTC_TALT_M, 4.0f);

/**
 * Configure, in milliseconds, the time allowed to gain altitude in takeoff. Limited to [FTC_TALT_MS_MIN;FTC_TALT_MS_MAX].
 */
PARAM_DEFINE_INT32(A_FTC_TALT_MS, 5000);

/**
 * Configure, in degrees, the maximum tilt allowed during flight. Limited to [FTC_FTILT_DEG_MIN;FTC_FTILT_DEG_MAX].
 */
PARAM_DEFINE_INT32(A_FTC_FTILT_DEG, 90);

/**
 * Configure, in degrees, the "low" tilt threshold during flight. Limited to [FTC_FLTILT_DEG_MIN;FTC_FLTILT_DEG_MAX].
 */
PARAM_DEFINE_INT32(A_FTC_FLTILT_DEG, 50);

/**
 * Configure, in milliseconds, the "low" tilt timeout during flight. Limited to [FTC_FLTILT_MS_MIN;FTC_FLTILT_MS_MAX].
 */
PARAM_DEFINE_INT32(A_FTC_FLTILT_MS, 3000);

/**
 * Configure, in degrees, the maximum tilt allowed during landing. Limited to [FTC_LTILT_DEG_MIN;FTC_LTILT_DEG_MAX].
 */
PARAM_DEFINE_INT32(A_FTC_LTILT_DEG, 55);
