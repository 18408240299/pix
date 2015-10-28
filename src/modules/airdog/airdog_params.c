/**
 * @file airdog_params.c
 * Airdog specific params.
 *
 * @author Kirils Sivokozs
 */

#include <systemlib/param/param.h>

/**
 * Minimum altitude for manual follow
 *
 * Minimum altitude in manual follow. Recommended to be in range from 3 to 10.
 *
 * @min 0.0
 * @max MAX_FLOAT
 * @group Airdog
 */
PARAM_DEFINE_FLOAT(AIRD_MIN_MF_ALT, 3.0f);

/**
 * Airdog step to adjust position
 *
 * in LOITER mode
 *
 * @unit meters
 * @group AirDog
 */
PARAM_DEFINE_FLOAT(AIRD_LOITER_STEP, 2.0f);

/**
 * Airdog Battery Warning level in percent
 * @group AirDog
 */
PARAM_DEFINE_FLOAT(AIRD_BAT_WARN, 30.0f);

/**
 * Airdog Battery Failsafe level in percent
 * @group AirDog
 */
PARAM_DEFINE_FLOAT(AIRD_BAT_FS, 20.0f);

/*Custom binded device id.*/
PARAM_DEFINE_INT32(AIRD_BINDED_ID, 4);

/*Custom binded device id.*/
PARAM_DEFINE_INT32(AIRD_TRAINER_ID, 10);

/*
 * Leash mode:
 *   0 is target,
 *   1 is trainer,
 *   2 is target with trajectory
 */
PARAM_DEFINE_INT32(AIRD_LEASH_MODE, 
#ifdef CONFIG_ARCH_BOARD_AIRLEASH
		5
#else
		0
#endif
);


/* TODO Description. (It was wrong.) */
PARAM_DEFINE_INT32(AIRD_PITCH_DOWN, 0);

/*Enable if should check for external magnetometer*/
PARAM_DEFINE_INT32(AIRD_CHECK_MAG, 
#ifdef CONFIG_ARCH_BOARD_AIRLEASH
		0
#else
		1
#endif
);


/*Enable automatic magnetic declination setting from coordinates*/
PARAM_DEFINE_INT32(AIRD_AUTO_MAG, 1);

/*Trajectory calculator low pass filter cutoff frequency (Hz) */
PARAM_DEFINE_FLOAT(AIRD_TRAJ_CUT, 0.2f);

/*Trajectory calculator stillness radius (meters) */
PARAM_DEFINE_FLOAT(AIRD_TRAJ_RAD, 5);

/**
 * @descr:  Airdog parameter to turn landing speed correction on or off
 *
 * @author: Max Shvetsov <maxim.shvetsov@airdog.com>
 * @unit:   1 - on, 0 - off
 */
PARAM_DEFINE_INT32(LAND_CORR_ON, 1);

/**
 * @descr:  Airdog param for custom landing with range finders
 *          Safe distance to trigger disarm while landing using
 *          range finder
 *
 * @author: Max Shvetsov <maxim.shvetsov@airdog.com>
 * @unit:   Meters
 * @min:    0.0f <- We can only disarm when sensors show 0.0 which is practically not measurable
 * @max     1.0f <- Note that vehicle should be able to survive safely drop from this distance
 */
PARAM_DEFINE_FLOAT(LAND_SAFE_H, 0.45f);

/**
 * @descr:  Airdog param for custom landing with range finders
 *          Max landing speed which will be held from the start of landing
 *          and then lowered by range finder down to A_LAND_MIN_V
 *
 * @author: Max Shvetsov <maxim.shvetsov@airdog.com>
 * @unit:   Meters/seconds
 * @min:    0.5f <- Practically should be higher. The minimum should be configured in A_LAND_MIN_V
 * @max     4.0f <- Unlimited, but HIGHLY not recommended to increase more than that
 */
PARAM_DEFINE_FLOAT(LAND_MAX_V, 2.2f);

/**
 * @descr:  Airdog param for custom landing with range finders
 *          Minimal landing speed which vehicle will achieve when close to the ground and about to
 *          disarm and land.
 *
 * @author: Max Shvetsov <maxim.shvetsov@airdog.com>
 * @unit:   Meters/seconds
 * @min:    0.1f
 * @max     1.0f
 */
PARAM_DEFINE_FLOAT(LAND_MIN_V, 0.5f);

/**
 * @descr: Parameter overriding max pwm set in rc scripts. If set to 0, script value will be used
 *
 * @min: 0
 */
PARAM_DEFINE_INT32(A_MAX_PWM, 1880);

PARAM_DEFINE_INT32(A_DO_FRAME_BUTT, 1);

/**
 * @descr: Parameter defining if magnetometer built-in scale calibration should be used. Used on values >0.
 *
 * @min: 0
 * @max: 1
 */
PARAM_DEFINE_INT32(A_CALIB_MAG_SCAL, 1);
