/****************************************************************************
 *
 *   Copyright (c) 2012-2014 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file sensor_params.c
 *
 * Parameters defined by the sensors task.
 *
 * @author Lorenz Meier <lm@inf.ethz.ch>
 * @author Julian Oes <joes@student.ethz.ch>
 * @author Thomas Gubler <thomasgubler@student.ethz.ch>
 */

#include <nuttx/config.h>
#include <systemlib/param/param.h>

/**
 * Gyro X-axis offset
 *
 * @min -10.0
 * @max 10.0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_XOFF, 0.0f);

/**
 * Gyro Y-axis offset
 *
 * @min -10.0
 * @max 10.0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_YOFF, 0.0f);

/**
 * Gyro Z-axis offset
 *
 * @min -5.0
 * @max 5.0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_ZOFF, 0.0f);

/**
 * Gyro X-axis scaling factor
 *
 * @min -1.5
 * @max 1.5
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_XSCALE, 1.0f);

/**
 * Gyro Y-axis scaling factor
 *
 * @min -1.5
 * @max 1.5
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_YSCALE, 1.0f);

/**
 * Gyro Z-axis scaling factor
 *
 * @min -1.5
 * @max 1.5
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_ZSCALE, 1.0f);

/**
 * Date of the last gyro calibration in hours since 1970.01.01
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_GYRO_CDATE, 0);

/**
 * Temperature at the time of the last gyro calibration in degrees Celsius
 * By default set to -5 degrees relative to 0K to trigger recalibration even at absolute zero. :-)
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_GYRO_CTEMP, -278.15f);

/**
 * Magnetometer X-axis offset
 *
 * @min -500.0
 * @max 500.0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_XOFF, 0.0f);

/**
 * Magnetometer Y-axis offset
 *
 * @min -500.0
 * @max 500.0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_YOFF, 0.0f);

/**
 * Magnetometer Z-axis offset
 *
 * @min -500.0
 * @max 500.0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_ZOFF, 0.0f);

/**
 * Magnetometer X-axis scaling factor
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_XSCALE, 1.0f);

/**
 * Magnetometer Y-axis scaling factor
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_YSCALE, 1.0f);

/**
 * Magnetometer Z-axis scaling factor
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_ZSCALE, 1.0f);

/**
 * Date of the last mag calibration in hours since 1970.01.01
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_MAG_CDATE, 0);

/**
 * Temperature at the time of the last mag calibration in degrees Celsius
 * By default set to -5 degrees relative to 0K to trigger recalibration even at absolute zero. :-)
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_MAG_CTEMP, -278.15f);

/**
 * Weight for sonar filter
 *
 * Sonar filter detects spikes on sonar measurements and used to detect new surface level.
 *
 * @min 0.0
 * @max 1.0
 * @group Position Estimator INAV
 */
PARAM_DEFINE_FLOAT(SENS_SON_FILT, 0.8f);

/**
 * Sonar maximal accepted delta for non-spike/new level sonar measurment
 *
 * If sonar measurement delta is larger than this value it skiped (spike) or accepted as new surface level (if offset is stable).
 *
 * @min 0.0
 * @max not limited
 * @unit m
 * @group Position Estimator INAV
 */
PARAM_DEFINE_FLOAT(SENS_SON_ERR, 0.7f);

/**
 * Sonar sensor on/off trigger
 * @on 1
 * @off 0
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_SON_ON, 1);

/**
 * Minimal distance to surface allowed by sonar
 * @min 0.0
 * @max unlimited;    max range of sonar, but there is no real maximal boundary
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_SON_MIN, 5.0f);

/**
 * Sonar coefficient to multiply sonar minimal distance by, resulting in critical distance
 * ==0.5f means critical distance will be 0.5*SENS_SON_MIN and at this altitude sonar will
 * trigger full throttle
 * @min 1.0f
 * @max 0.0f
 * @unit meters
 */
PARAM_DEFINE_FLOAT(SENS_SON_SMOT, 1.0f);

/**
 * TEMPORARY!!!
 * Should calibration through mavlink use original or modified calibration routines.
 * Original calibration used on 0.
 * Modified calibration on nonzero values.
 * @min 0
 * @max 1
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(A_CALIB_MODE, 1);

/**
 * Defines minimal difference in hours between current date and last calibration date
 * That triggers an automatic gyro recalibration on arm
 *
 * @min 0
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(A_CALIB_dTIME_H, 5*24);

/**
 * Defines minimal difference in degrees Celsius between current temperature and last calibration temperature
 * That triggers an automatic gyro recalibration on arm
 *
 * @min 0
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(A_CALIB_dTEMP_C, 3.0f);

/**
 * Accelerometer X-axis offset
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_XOFF, 0.0f);

/**
 * Accelerometer Y-axis offset
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_YOFF, 0.0f);

/**
 * Accelerometer Z-axis offset
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_ZOFF, 0.0f);

/**
 * Accelerometer X-axis scaling factor
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_XSCALE, 1.0f);

/**
 * Accelerometer Y-axis scaling factor
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_YSCALE, 1.0f);

/**
 * Accelerometer Z-axis scaling factor
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_ZSCALE, 1.0f);

/**
 * Date of the last acc calibration in hours since 1970.01.01
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_ACC_CDATE, 0);

/**
 * Temperature at the time of the last acc calibration in degrees Celsius
 * By default set to -5 degrees relative to 0K to trigger recalibration even at absolute zero. :-)
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_ACC_CTEMP, -278.15f);

/**
 * Maximum needed acceleration in g
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_ACC_RANGE, 
#ifdef CONFIG_ARCH_BOARD_AIRLEASH
		16
#else
		8
#endif
);



/**
 * Differential pressure sensor offset
 *
 * The offset (zero-reading) in Pascal
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_DPRES_OFF, 0.0f);

/**
 * Differential pressure sensor analog scaling
 *
 * Pick the appropriate scaling from the datasheet.
 * this number defines the (linear) conversion from voltage
 * to Pascal (pa). For the MPXV7002DP this is 1000.
 *
 * NOTE: If the sensor always registers zero, try switching
 * the static and dynamic tubes.
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_DPRES_ANSC, 0);

/**
 * QNH for barometer
 *
 * @min 500
 * @max 1500
 * @group Sensor Calibration
 * @unit hPa
 */
PARAM_DEFINE_FLOAT(SENS_BARO_QNH, 1013.25f);


/**
 * Board rotation
 *
 * This parameter defines the rotation of the FMU board relative to the platform.
 * Possible values are:
 *    0 = No rotation
 *    1 = Yaw 45°
 *    2 = Yaw 90°
 *    3 = Yaw 135°
 *    4 = Yaw 180°
 *    5 = Yaw 225°
 *    6 = Yaw 270°
 *    7 = Yaw 315°
 *    8 = Roll 180°
 *    9 = Roll 180°, Yaw 45°
 *   10 = Roll 180°, Yaw 90°
 *   11 = Roll 180°, Yaw 135°
 *   12 = Pitch 180°
 *   13 = Roll 180°, Yaw 225°
 *   14 = Roll 180°, Yaw 270°
 *   15 = Roll 180°, Yaw 315°
 *   16 = Roll 90°
 *   17 = Roll 90°, Yaw 45°
 *   18 = Roll 90°, Yaw 90°
 *   19 = Roll 90°, Yaw 135°
 *   20 = Roll 270°
 *   21 = Roll 270°, Yaw 45°
 *   22 = Roll 270°, Yaw 90°
 *   23 = Roll 270°, Yaw 135°
 *   24 = Pitch 90°
 *   25 = Pitch 270°
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_BOARD_ROT, 6);

/**
 * Board rotation Y (Pitch) offset
 *
 * This parameter defines a rotational offset in degrees around the Y (Pitch) axis. It allows the user
 * to fine tune the board offset in the event of misalignment.
 *
 * @group Sensor Calibration
 */
 PARAM_DEFINE_FLOAT(SENS_BOARD_Y_OFF, 0.0f);

/**
 * Board rotation X (Roll) offset
 *
 * This parameter defines a rotational offset in degrees around the X (Roll) axis It allows the user
 * to fine tune the board offset in the event of misalignment.
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_BOARD_X_OFF, 0.0f);

/**
 * Board rotation Z (YAW) offset
 *
 * This parameter defines a rotational offset in degrees around the Z (Yaw) axis. It allows the user
 * to fine tune the board offset in the event of misalignment.
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_FLOAT(SENS_BOARD_Z_OFF, 180.0f);

/**
 * External magnetometer rotation
 *
 * This parameter defines the rotation of the external magnetometer relative
 * to the platform (not relative to the FMU).
 * See SENS_BOARD_ROT for possible values.
 *
 * @group Sensor Calibration
 */
PARAM_DEFINE_INT32(SENS_EXT_MAG_ROT, 2);

/**
* Set usage of external magnetometer
*
*  * Set to 0 (default) to auto-detect (will try to get the external as primary)
*  * Set to 1 to force the external magnetometer as primary
*  * Set to 2 to force the internal magnetometer as primary
*
* @min 0
* @max 2
* @group Sensor Calibration
*/
PARAM_DEFINE_INT32(SENS_EXT_MAG, 0);


/**
 * RC Channel 1 Minimum
 *
 * Minimum value for RC channel 1
 *
 * @min 800.0
 * @max 1500.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC1_MIN, 996.0f);

/**
 * RC Channel 1 Trim
 *
 * Mid point value (same as min for throttle)
 *
 * @min 800.0
 * @max 2200.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC1_TRIM, 996.0f);

/**
 * RC Channel 1 Maximum
 *
 * Maximum value for RC channel 1
 *
 * @min 1500.0
 * @max 2200.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC1_MAX, 2011.0f);

/**
 * RC Channel 1 Reverse
 *
 * Set to -1 to reverse channel.
 *
 * @min -1.0
 * @max 1.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC1_REV, 1.0f);

/**
 * RC Channel 1 dead zone
 *
 * The +- range of this value around the trim value will be considered as zero.
 *
 * @min 0.0
 * @max 100.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC1_DZ, 50.0f);

/**
 * RC Channel 2 Minimum
 *
 * Minimum value for RC channel 2
 *
 * @min 800.0
 * @max 1500.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC2_MIN, 1002.0f);

/**
 * RC Channel 2 Trim
 *
 * Mid point value (same as min for throttle)
 *
 * @min 800.0
 * @max 2200.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC2_TRIM, 1500.0f);

/**
 * RC Channel 2 Maximum
 *
 * Maximum value for RC channel 2
 *
 * @min 1500.0
 * @max 2200.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC2_MAX, 2006.0f);

/**
 * RC Channel 2 Reverse
 *
 * Set to -1 to reverse channel.
 *
 * @min -1.0
 * @max 1.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC2_REV, 1.0f);

/**
 * RC Channel 2 dead zone
 *
 * The +- range of this value around the trim value will be considered as zero.
 *
 * @min 0.0
 * @max 100.0
 * @group Radio Calibration
 */
PARAM_DEFINE_FLOAT(RC2_DZ, 100.0f);

PARAM_DEFINE_FLOAT(RC3_MIN, 1002);
PARAM_DEFINE_FLOAT(RC3_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC3_MAX, 2006);
PARAM_DEFINE_FLOAT(RC3_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC3_DZ, 100.0f);

PARAM_DEFINE_FLOAT(RC4_MIN, 992);
PARAM_DEFINE_FLOAT(RC4_TRIM, 1507);
PARAM_DEFINE_FLOAT(RC4_MAX, 2016);
PARAM_DEFINE_FLOAT(RC4_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC4_DZ, 50.0f);

PARAM_DEFINE_FLOAT(RC5_MIN, 1010);
PARAM_DEFINE_FLOAT(RC5_TRIM, 1501);
PARAM_DEFINE_FLOAT(RC5_MAX, 2011);
PARAM_DEFINE_FLOAT(RC5_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC5_DZ,  10.0f);

PARAM_DEFINE_FLOAT(RC6_MIN, 1000);
PARAM_DEFINE_FLOAT(RC6_TRIM, 1504);
PARAM_DEFINE_FLOAT(RC6_MAX, 2012);
PARAM_DEFINE_FLOAT(RC6_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC6_DZ, 20.0f);

PARAM_DEFINE_FLOAT(RC7_MIN, 992);
PARAM_DEFINE_FLOAT(RC7_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC7_MAX, 2017);
PARAM_DEFINE_FLOAT(RC7_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC7_DZ, 10.0f);

PARAM_DEFINE_FLOAT(RC8_MIN, 991);
PARAM_DEFINE_FLOAT(RC8_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC8_MAX, 2016);
PARAM_DEFINE_FLOAT(RC8_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC8_DZ, 10.0f);

PARAM_DEFINE_FLOAT(RC9_MIN, 1000);
PARAM_DEFINE_FLOAT(RC9_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC9_MAX, 2000);
PARAM_DEFINE_FLOAT(RC9_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC9_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC10_MIN, 1000);
PARAM_DEFINE_FLOAT(RC10_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC10_MAX, 2000);
PARAM_DEFINE_FLOAT(RC10_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC10_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC11_MIN, 1000);
PARAM_DEFINE_FLOAT(RC11_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC11_MAX, 2000);
PARAM_DEFINE_FLOAT(RC11_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC11_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC12_MIN, 1000);
PARAM_DEFINE_FLOAT(RC12_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC12_MAX, 2000);
PARAM_DEFINE_FLOAT(RC12_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC12_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC13_MIN, 1000);
PARAM_DEFINE_FLOAT(RC13_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC13_MAX, 2000);
PARAM_DEFINE_FLOAT(RC13_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC13_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC14_MIN, 1000);
PARAM_DEFINE_FLOAT(RC14_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC14_MAX, 2000);
PARAM_DEFINE_FLOAT(RC14_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC14_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC15_MIN, 1000);
PARAM_DEFINE_FLOAT(RC15_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC15_MAX, 2000);
PARAM_DEFINE_FLOAT(RC15_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC15_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC16_MIN, 1000);
PARAM_DEFINE_FLOAT(RC16_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC16_MAX, 2000);
PARAM_DEFINE_FLOAT(RC16_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC16_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC17_MIN, 1000);
PARAM_DEFINE_FLOAT(RC17_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC17_MAX, 2000);
PARAM_DEFINE_FLOAT(RC17_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC17_DZ, 0.0f);

PARAM_DEFINE_FLOAT(RC18_MIN, 1000);
PARAM_DEFINE_FLOAT(RC18_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC18_MAX, 2000);
PARAM_DEFINE_FLOAT(RC18_REV, 1.0f);
PARAM_DEFINE_FLOAT(RC18_DZ, 0.0f);

#ifdef CONFIG_ARCH_BOARD_PX4FMU_V1
PARAM_DEFINE_INT32(RC_RL1_DSM_VCC, 0); /* Relay 1 controls DSM VCC */
#endif

/**
 * DSM binding trigger.
 *
 * -1 = Idle, 0 = Start DSM2 bind, 1 = Start DSMX bind
 *
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_DSM_BIND, -1);


/**
 * Scaling factor for battery voltage sensor on PX4IO.
 *
 * @group Battery Calibration
 */
PARAM_DEFINE_INT32(BAT_V_SCALE_IO, 10000);

/**
 * Scaling factor for battery voltage sensor on FMU v2.
 *
 * @group Battery Calibration
 */
PARAM_DEFINE_FLOAT(BAT_V_SCALING, 
#ifdef CONFIG_ARCH_BOARD_AIRLEASH
		0.00459341f
#else
		0.00615f
#endif
);

/**
 * Scaling factor for battery current sensor.
 *
 * @group Battery Calibration
 */
PARAM_DEFINE_FLOAT(BAT_C_SCALING, 
#ifdef CONFIG_ARCH_BOARD_AIRLEASH
		0.0124f
#else
		0.020658f
#endif
);


/**
 * Roll control channel mapping.
 *
 * The channel index (starting from 1 for channel 1) indicates
 * which channel should be used for reading roll inputs from.
 * A value of zero indicates the switch is not assigned.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_ROLL, 2);

/**
 * Pitch control channel mapping.
 *
 * The channel index (starting from 1 for channel 1) indicates
 * which channel should be used for reading pitch inputs from.
 * A value of zero indicates the switch is not assigned.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_PITCH, 3);

/**
 * Failsafe channel mapping.
 *
 * The RC mapping index indicates which channel is used for failsafe
 * If 0, whichever channel is mapped to throttle is used
 * otherwise the value indicates the specific rc channel to use
 *
 * @min 0
 * @max 18
 *
 *
 */
PARAM_DEFINE_INT32(RC_MAP_FAILSAFE, 5);  //Default to throttle function

/**
 * Throttle control channel mapping.
 *
 * The channel index (starting from 1 for channel 1) indicates
 * which channel should be used for reading throttle inputs from.
 * A value of zero indicates the switch is not assigned.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_THROTTLE, 1);

/**
 * Yaw control channel mapping.
 *
 * The channel index (starting from 1 for channel 1) indicates
 * which channel should be used for reading yaw inputs from.
 * A value of zero indicates the switch is not assigned.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_YAW, 4);

/**
 * Mode switch channel mapping.
 *
 * This is the main flight mode selector.
 * The channel index (starting from 1 for channel 1) indicates
 * which channel should be used for deciding about the main mode.
 * A value of zero indicates the switch is not assigned.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_MODE_SW, 6);

/**
 * Return switch channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_RETURN_SW, 0);

/**
 * Posctl switch channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_POSCTL_SW, 5);

/**
 * Loiter switch channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_LOITER_SW, 0);

/**
 * Acro switch channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_ACRO_SW, 0);

/**
 * Offboard switch channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_OFFB_SW, 0);

/**
 * Follow switch channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_FOLLOW_SW, 5);

/**
 * Flaps channel mapping.
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_FLAPS, 0);

/**
 * Auxiliary switch 1 channel mapping.
 *
 * Default function: Camera pitch
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_AUX1, 7);

/**
 * Auxiliary switch 2 channel mapping.
 *
 * Default function: Camera roll
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_AUX2, 8);	/**< default function: camera roll */

/**
 * Auxiliary switch 3 channel mapping.
 *
 * Default function: Camera azimuth / yaw
 *
 * @min 0
 * @max 18
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_MAP_AUX3, 0);


/**
 * Failsafe channel PWM threshold.
 *
 * @min 800
 * @max 2200
 * @group Radio Calibration
 */
PARAM_DEFINE_INT32(RC_FAILS_THR, 1000);

/**
 * Threshold for selecting assist mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_ASSIST_TH, 0.3f);

/**
 * Threshold for selecting auto mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_AUTO_TH, 0.75f);

/**
 * Threshold for selecting posctl mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_POSCTL_TH, 0.3f);

/**
 * Threshold for selecting return to launch mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_RETURN_TH, 0.5f);

/**
 * Threshold for selecting loiter mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_LOITER_TH, 0.5f);

/**
 * Threshold for selecting acro mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_ACRO_TH, 0.5f);


/**
 * Threshold for selecting offboard mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_OFFB_TH, 0.5f);

/**
 * Threshold for selecting follow mode
 *
 * min:-1
 * max:+1
 *
 * 0-1 indicate where in the full channel range the threshold sits
 * 		0 : min
 * 		1 : max
 * sign indicates polarity of comparison
 * 		positive : true when channel>th
 * 		negative : true when channel<th
 *
 */
PARAM_DEFINE_FLOAT(RC_FOLLOW_TH, 0.5f);

/**
 * Specifies if MPU6000 sensor should not be started and lsm303d and l3gd20 used instead
 *
 * min: 0
 * max: 1
 *
 * 1 - l3gd20, lsm303d
 * 0 - MPU6000
 */
PARAM_DEFINE_INT32(SENS_DISABLE_MPU, 0);

/**
 * Specifies expected value returned by excited magnetometer on X axis
 */
PARAM_DEFINE_FLOAT(SENS_MAG_XPECT_X, 1.08);

/**
 * Specifies expected value returned by excited magnetometer on Y axis
 */
PARAM_DEFINE_FLOAT(SENS_MAG_XPECT_Y, 1.16);

/**
 * Specifies expected value returned by excited magnetometer on Z axis
 */
PARAM_DEFINE_FLOAT(SENS_MAG_XPECT_Z, 1.16);

/**
 * Controls whether we calculate both offsets
 * and scales during the built-in calibration or only scales
 */
PARAM_DEFINE_INT32(SENS_MAG_XCT_OFF, 0);
