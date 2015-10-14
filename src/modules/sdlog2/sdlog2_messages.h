/****************************************************************************
 *
 *   Copyright (c) 2013 PX4 Development Team. All rights reserved.
 *   Author: Anton Babushkin <anton.babushkin@me.com>
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
 * @file sdlog2_messages.h
 *
 * Log messages and structures definition.
 *
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#ifndef SDLOG2_MESSAGES_H_
#define SDLOG2_MESSAGES_H_

#include "sdlog2_format.h"

/* define message formats */

#pragma pack(push, 1)
/* --- ATT - ATTITUDE --- */
#define LOG_ATT_MSG 2
struct log_ATT_s {
	float roll;
	float pitch;
	float yaw;
	float roll_rate;
	float pitch_rate;
	float yaw_rate;
	float gx;
	float gy;
	float gz;
	float roll_off;
	float pitch_off;
	float yaw_off;
};

/* --- ATSP - ATTITUDE SET POINT --- */
#define LOG_ATSP_MSG 3
struct log_ATSP_s {
	float roll_sp;
	float pitch_sp;
	float yaw_sp;
	float thrust_sp;
};

/* --- IMU - IMU SENSORS --- */
#define LOG_IMU_MSG 4
#define LOG_IMU1_MSG 22
#define LOG_IMU2_MSG 23
struct log_IMU_s {
	float acc_x;
	float acc_y;
	float acc_z;
	float gyro_x;
	float gyro_y;
	float gyro_z;
	float mag_x;
	float mag_y;
	float mag_z;
};

/* --- SRAW - RAW SENSORS --- */
#define LOG_SRAW_MSG 79
struct log_SRAW_s {
	int16_t acc_x;
	int16_t acc_y;
	int16_t acc_z;
	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
	int16_t mag_x;
	int16_t mag_y;
	int16_t mag_z;
};

/* --- SENS - OTHER SENSORS --- */
#define LOG_SENS_MSG 5
struct log_SENS_s {
	float baro_pres;
	float baro_alt;
	float baro_temp;
	float diff_pres;
	float diff_pres_filtered;
	float accel_temp;
	float gyro_temp;
};

/* --- LPOS - LOCAL POSITION --- */
#define LOG_LPOS_MSG 6
struct log_LPOS_s {
	float x;
	float y;
	float z;
	float ground_dist;
	float ground_dist_rate;
	float vx;
	float vy;
	float vz;
	int32_t ref_lat;
	int32_t ref_lon;
	float ref_alt;
	uint8_t pos_flags;
	uint8_t landed;
	uint8_t ground_dist_flags;
	float eph;
	float epv;
};

/* --- LRAW - Raw local position --- */
#define LOG_LRAW_MSG 80
struct log_LRAW_s {
	float vx;
	float vy;
	float vz;
};

/* --- LPSP - LOCAL POSITION SETPOINT --- */
#define LOG_LPSP_MSG 7
struct log_LPSP_s {
	float x;
	float y;
	float z;
	float yaw;
};

/* --- GPS - GPS POSITION --- */
#define LOG_GPS_MSG 8
struct log_GPS_s {
	uint64_t gps_time;
	uint8_t fix_type;
	float eph;
	float epv;
	int32_t lat;
	int32_t lon;
	float alt;
	float vel_n;
	float vel_e;
	float vel_d;
	float cog;
	uint8_t sats;
	uint16_t snr_mean;
	uint16_t noise_per_ms;
	uint16_t jamming_indicator;
};

/* --- ATTC - ATTITUDE CONTROLS (ACTUATOR_0 CONTROLS)--- */
#define LOG_ATTC_MSG 9
struct log_ATTC_s {
	float roll;
	float pitch;
	float yaw;
	float thrust;
};

/* --- STAT - VEHICLE STATE --- */
#define LOG_STAT_MSG 10
struct log_STAT_s {
	uint8_t main_state;
	uint8_t arming_state;
	uint8_t failsafe_state;
	uint8_t navigation_state;
	float battery_remaining;
	uint8_t battery_warning;
	uint8_t landed;
	float load;
	uint8_t aird_state;
	uint8_t target_valid;
	int free_memory;
	int used_memory;
};

/* --- RC - RC INPUT CHANNELS --- */
#define LOG_RC_MSG 11
struct log_RC_s {
	float channel[8];
	uint8_t channel_count;
	uint8_t signal_lost;
};

/* --- OUT0 - ACTUATOR_0 OUTPUT --- */
#define LOG_OUT0_MSG 12
struct log_OUT0_s {
	float output[8];
};

/* --- AIRS - AIRSPEED --- */
#define LOG_AIRS_MSG 13
struct log_AIRS_s {
	float indicated_airspeed;
	float true_airspeed;
	float air_temperature_celsius;
};

/* --- ARSP - ATTITUDE RATE SET POINT --- */
#define LOG_ARSP_MSG 14
struct log_ARSP_s {
	float roll_rate_sp;
	float pitch_rate_sp;
	float yaw_rate_sp;
};

/* --- FLOW - OPTICAL FLOW --- */
#define LOG_FLOW_MSG 15
struct log_FLOW_s {
	int16_t flow_raw_x;
	int16_t flow_raw_y;
	float flow_comp_x;
	float flow_comp_y;
	float distance;
	uint8_t	quality;
	uint8_t sensor_id;
};

/* --- GPOS - GLOBAL POSITION ESTIMATE --- */
#define LOG_GPOS_MSG 16
struct log_GPOS_s {
	int32_t lat;
	int32_t lon;
	float alt;
	float vel_n;
	float vel_e;
	float vel_d;
	float eph;
	float epv;
	float terrain_alt;
};

/* --- GPSP - GLOBAL POSITION SETPOINT --- */
#define LOG_GPSP_MSG 17
struct log_GPSP_s {
	uint8_t nav_state;
	int32_t lat;
	int32_t lon;
	float alt;
	float yaw;
	uint8_t type;
	float loiter_radius;
	int8_t loiter_direction;
	float pitch_min;
};

/* --- ESC - ESC STATE --- */
#define LOG_ESC_MSG 18
struct log_ESC_s {
	uint16_t counter;
	uint8_t  esc_count;
	uint8_t  esc_connectiontype;
	uint8_t  esc_num;
	uint16_t esc_address;
	uint16_t esc_version;
	float    esc_voltage;
	float    esc_current;
	int32_t  esc_rpm;
	float    esc_temperature;
	float    esc_setpoint;
	uint16_t esc_setpoint_raw;
};

/* --- GVSP - GLOBAL VELOCITY SETPOINT --- */
#define LOG_GVSP_MSG 19
struct log_GVSP_s {
	float vx;
	float vy;
	float vz;
};

/* --- BATT - BATTERY --- */
#define LOG_BATT_MSG 20
struct log_BATT_s {
	float voltage;
	float voltage_filtered;
	float current;
	float discharged;
};

/* --- DIST - DISTANCE TO SURFACE --- */
#define LOG_DIST_MSG 21
struct log_DIST_s {
	float bottom;
	float bottom_rate;
    float minDist;
	uint8_t flags;
};

/* LOG IMU1 and IMU2 MSGs consume IDs 22 and 23 */


/* --- PWR - ONBOARD POWER SYSTEM --- */
#define LOG_PWR_MSG 24
struct log_PWR_s {
	float peripherals_5v;
	float servo_rail_5v;
	float servo_rssi;
	uint8_t usb_ok;
	uint8_t brick_ok;
	uint8_t servo_ok;
	uint8_t low_power_rail_overcurrent;
	uint8_t high_power_rail_overcurrent;
};

/* --- VICN - VICON POSITION --- */
#define LOG_VICN_MSG 25
struct log_VICN_s {
	float x;
	float y;
	float z;
	float roll;
	float pitch;
	float yaw;
};

/* --- GS0A - GPS SNR #0, SAT GROUP A --- */
#define LOG_GS0A_MSG 26
struct log_GS0A_s {
	uint8_t satellite_snr[16];			/**< dBHz, Signal to noise ratio of satellite C/N0, range 0..99 */
};

/* --- GS0B - GPS SNR #0, SAT GROUP B --- */
#define LOG_GS0B_MSG 27
struct log_GS0B_s {
	uint8_t satellite_snr[16];			/**< dBHz, Signal to noise ratio of satellite C/N0, range 0..99 */
};

/* --- GS1A - GPS SNR #1, SAT GROUP A --- */
#define LOG_GS1A_MSG 28
struct log_GS1A_s {
	uint8_t satellite_snr[16];			/**< dBHz, Signal to noise ratio of satellite C/N0, range 0..99 */
};

/* --- GS1B - GPS SNR #1, SAT GROUP B --- */
#define LOG_GS1B_MSG 29
struct log_GS1B_s {
	uint8_t satellite_snr[16];			/**< dBHz, Signal to noise ratio of satellite C/N0, range 0..99 */
};

/* --- TECS - TECS STATUS --- */
#define LOG_TECS_MSG 30
struct log_TECS_s {
	float altitudeSp;
	float altitudeFiltered;
	float flightPathAngleSp;
	float flightPathAngle;
	float flightPathAngleFiltered;
	float airspeedSp;
	float airspeedFiltered;
	float airspeedDerivativeSp;
	float airspeedDerivative;

	float totalEnergyRateSp;
	float totalEnergyRate;
	float energyDistributionRateSp;
	float energyDistributionRate;

	uint8_t mode;
};

/* --- WIND - WIND ESTIMATE --- */
#define LOG_WIND_MSG 31
struct log_WIND_s {
	float x;
	float y;
	float cov_x;
	float cov_y;
};

/* --- EST0 - ESTIMATOR STATUS --- */
#define LOG_EST0_MSG 32
struct log_EST0_s {
	float s[12];
	uint8_t n_states;
	uint8_t nan_flags;
	uint8_t health_flags;
	uint8_t timeout_flags;
};

/* --- EST1 - ESTIMATOR STATUS --- */
#define LOG_EST1_MSG 33
struct log_EST1_s {
	float s[16];
};

/* --- TEL0..3 - TELEMETRY STATUS --- */
#define LOG_TEL0_MSG 34
#define LOG_TEL1_MSG 35
#define LOG_TEL2_MSG 36
#define LOG_TEL3_MSG 37
struct log_TEL_s {
	uint8_t rssi;
	uint8_t remote_rssi;
	uint8_t noise;
	uint8_t remote_noise;
	uint16_t rxerrors;
	uint16_t fixed;
	uint8_t txbuf;
	uint64_t heartbeat_time;
};

/* --- VISN - VISION POSITION --- */
#define LOG_VISN_MSG 38
struct log_VISN_s {
	float x;
	float y;
	float z;
	float vx;
	float vy;
	float vz;
	float qx;
	float qy;
	float qz;
	float qw;
};
/* --- TPOS - TARGET GLOBAL POSITION --- */
#define LOG_TPOS_MSG 64
struct log_TPOS_s {
	uint8_t sysid;
	uint64_t time;
	int32_t lat;
	int32_t lon;
	float alt;
	float vel_n;
	float vel_e;
	float vel_d;
	float eph;
	float epv;
};
/* --- EXTJ - EXTERNAL TRAJECTORY --- */
#define LOG_EXTJ_MSG 65
struct log_EXTJ_s {
	uint8_t point_type; /**< Indicates whether movement has crossed the threshold, 0 - still point, 1 - moving */
	uint8_t sysid; 		/**< External system id */
	uint64_t timestamp;	/**< Time of this estimate, in microseconds since system start */
	int32_t lat;			/**< Latitude in degrees */
	int32_t lon;			/**< Longitude in degrees */
	float alt;			/**< Altitude AMSL in meters */
	float relative_alt; /**< Altitude above ground in meters */
	float vel_n; 		/**< Ground north velocity, m/s	*/
	float vel_e;		/**< Ground east velocity, m/s */
	float vel_d;		/**< Ground downside velocity, m/s */
	float heading;   	/**< Compass heading in radians [0..2PI) */
};
/* --- LOTJ - LOCAL TRAJECTORY --- */
#define LOG_LOTJ_MSG 66
struct log_LOTJ_s {
	uint8_t point_type; /**< Indicates whether movement has crossed the threshold, 0 - still point, 1 - moving */
	uint64_t timestamp;	/**< Time of this estimate, in microseconds since system start		*/
	int32_t lat;			/**< Latitude in degrees	*/
	int32_t lon;			/**< Longitude in degrees	*/
	float alt;			/**< Altitude AMSL in meters */
	float relative_alt; /**< Altitude above ground in meters */
	float vel_n; 		/**< Ground north velocity, m/s	*/
	float vel_e;		/**< Ground east velocity, m/s */
	float vel_d;		/**< Ground downside velocity, m/s */
	float heading;   	/**< Compass heading in radians [0..2PI) */
};

/* --- GPRE - PREVIOUS GLOBAL SETPOINT --- */
#define LOG_GPRE_MSG 67
struct log_GPRE_s {
	uint8_t nav_state;
	int32_t lat;
	int32_t lon;
	float alt;
	uint8_t type;
};

#define LOG_GNEX_MSG 68
struct log_GNEX_s {
	uint8_t nav_state;
	int32_t lat;
	int32_t lon;
	float alt;
	uint8_t type;
};


/* ---  FOLP --- */
#define LOG_FOLP_MSG 69
struct log_FOLP_s {

    float dst_i;
    float dst_p;
    float dst_d;

    float vel;
    float point_count;
    
    float dst_to_gate;
    float dst_to_tunnel_middle;

    float fx;
    float fy;
    float fz;

    float sx;
    float sy;
    float sz;

};

#include <uORB/topics/mavlink_stats.h>
/* --- Mavlink receive stats --- */
#define LOG_MVRX_MSG 70
#define log_MVRX_s mavlink_stats_s
/* --- Mavlink transmit stats --- */
#define LOG_MVTX_MSG 71
#define log_MVTX_s mavlink_stats_s


/* --- Vehicle command --- */
#define LOG_CMD_MSG 72
struct log_CMD_s {
	uint16_t command;
	uint8_t source_sys;
	uint8_t source_comp;
	float param1;
	float param2;
	float param3;
};

/* --- Target raw GPS data --- */
#define LOG_TGPS_MSG 73
struct log_TGPS_s {
	uint64_t gps_time;
	uint8_t fix_type;
	float eph;
	float epv;
	int32_t lat;
	int32_t lon;
	float alt;
	float vel;
	float cog;
	uint8_t sats;
};

#include <uORB/topics/bt21_laird.h>

#define LOG_BTSI_MSG 74
#define log_BTSI_s bt_svc_in_s

#define LOG_BTSO_MSG 75
#define log_BTSO_s bt_svc_out_s

#define LOG_BTEV_MSG 76
#define log_BTEV_s bt_evt_status_s

#define LOG_BTLK_MSG 77
#define log_BTLK_s bt_link_status_s

//#define LOG_BTCH_MSG 78
//struct log_BTCH_s {
//	uint32_t bytes_sent[7];
//	uint32_t bytes_received[7];
//};

/* !!! ID 79 used up by raw sensors */
/* !!! ID 80 used up by raw local position */

#define LOG_BUTT_MSG 81
struct log_BUTT_s {
	uint16_t mask;
};

/********** SYSTEM MESSAGES, ID > 0x80 **********/

/* --- TIME - TIME STAMP --- */
#define LOG_TIME_MSG 129
struct log_TIME_s {
	uint64_t t;
};

/* --- VER - VERSION --- */
#define LOG_VER_MSG 130
struct log_VER_s {
	char arch[16];
	char fw_git[64];
};

/* --- PARM - PARAMETER --- */
#define LOG_PARM_MSG 131
struct log_PARM_s {
	char name[16];
	float value;
};

#pragma pack(pop)

/* construct list of all message formats */
static const struct log_format_s log_formats[] = {
	/* business-level messages, ID < 0x80 */
	LOG_FORMAT(ATT, "ffffffffffff",		"Roll,Pitch,Yaw,RollRate,PitRate,YawRate,GX,GY,GZ,ROff,POff,YOff"),
	LOG_FORMAT(ATSP, "ffff",		"RollSP,PitchSP,YawSP,ThrustSP"),
	LOG_FORMAT_S(IMU, IMU, "fffffffff",		"AccX,AccY,AccZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ"),
	LOG_FORMAT_S(IMU1, IMU, "fffffffff",		"AccX,AccY,AccZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ"),
	LOG_FORMAT_S(IMU2, IMU, "fffffffff",		"AccX,AccY,AccZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ"),
	LOG_FORMAT(SRAW, "hhhhhhhhh", "AccX,AccY,AccZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ"),
	LOG_FORMAT(SENS, "fffffff",		"BrPres,BrAlt,BrTemp,DiffPres,DiffPresFilt,AccTemp,GyroTemp"),
	LOG_FORMAT(LPOS, "ffffffffLLfBBBff",	"X,Y,Z,Dist,DistR,VX,VY,VZ,RLat,RLon,RAlt,PFlg,LFlg,GFlg,EPH,EPV"),
	LOG_FORMAT(LRAW, "fff", "VX,VY,VZ"),
	LOG_FORMAT(LPSP, "ffff",		"X,Y,Z,Yaw"),
	LOG_FORMAT(GPS, "QBffLLfffffBHHH",	"GPSTime,Fix,EPH,EPV,Lat,Lon,Alt,VelN,VelE,VelD,Cog,nSat,SNR,N,J"),
	LOG_FORMAT(ATTC, "ffff",		"Roll,Pitch,Yaw,Thrust"),
	LOG_FORMAT(STAT, "BBBBfBBfBBii",		"MainSt,Arm,FailSt,NavSt,BatRem,BWrn,Land,Load,AdSt,TgVal,Mem,Us"),
	LOG_FORMAT(RC, "ffffffffBB",		"Ch0,Ch1,Ch2,Ch3,Ch4,Ch5,Ch6,Ch7,Count,SignalLost"),
	LOG_FORMAT(OUT0, "ffffffff",		"Out0,Out1,Out2,Out3,Out4,Out5,Out6,Out7"),
	LOG_FORMAT(AIRS, "fff",			"IndSpeed,TrueSpeed,AirTemp"),
	LOG_FORMAT(ARSP, "fff",			"RollRateSP,PitchRateSP,YawRateSP"),
	LOG_FORMAT(FLOW, "hhfffBB",		"RawX,RawY,CompX,CompY,Dist,Q,SensID"),
	LOG_FORMAT(GPOS, "LLfffffff",		"Lat,Lon,Alt,VelN,VelE,VelD,EPH,EPV,TALT"),
	LOG_FORMAT(GPSP, "BLLffBfbf",		"NavState,Lat,Lon,Alt,Yaw,Type,LoitR,LoitDir,PitMin"),
	LOG_FORMAT(ESC, "HBBBHHffiffH",		"count,nESC,Conn,N,Ver,Adr,Volt,Amp,RPM,Temp,SetP,SetPRAW"),
	LOG_FORMAT(GVSP, "fff",			"VX,VY,VZ"),
	LOG_FORMAT(BATT, "ffff",		"V,VFilt,C,Discharged"),
	LOG_FORMAT(DIST, "fffB",			"Bottom,BottomRate,MinDist,Flags"),
	LOG_FORMAT_S(TEL0, TEL, "BBBBHHBQ",		"RSSI,RemRSSI,Noise,RemNoise,RXErr,Fixed,TXBuf,HbTime"),
	LOG_FORMAT_S(TEL1, TEL, "BBBBHHBQ",		"RSSI,RemRSSI,Noise,RemNoise,RXErr,Fixed,TXBuf,HbTime"),
	LOG_FORMAT_S(TEL2, TEL, "BBBBHHBQ",		"RSSI,RemRSSI,Noise,RemNoise,RXErr,Fixed,TXBuf,HbTime"),
	LOG_FORMAT_S(TEL3, TEL, "BBBBHHBQ",		"RSSI,RemRSSI,Noise,RemNoise,RXErr,Fixed,TXBuf,HbTime"),
	LOG_FORMAT(EST0, "ffffffffffffBBBB",	"s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,nStat,fNaN,fHealth,fTOut"),
	LOG_FORMAT(EST1, "ffffffffffffffff",	"s12,s13,s14,s15,s16,s17,s18,s19,s20,s21,s22,s23,s24,s25,s26,s27"),
	LOG_FORMAT(PWR, "fffBBBBB",		"Periph5V,Servo5V,RSSI,UsbOk,BrickOk,ServoOk,PeriphOC,HipwrOC"),
	LOG_FORMAT(VICN, "ffffff",		"X,Y,Z,Roll,Pitch,Yaw"),
	LOG_FORMAT(VISN, "ffffffffff",		"X,Y,Z,VX,VY,VZ,QuatX,QuatY,QuatZ,QuatW"),
	LOG_FORMAT(GS0A, "BBBBBBBBBBBBBBBB",	"s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15"),
	LOG_FORMAT(GS0B, "BBBBBBBBBBBBBBBB",	"s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15"),
	LOG_FORMAT(GS1A, "BBBBBBBBBBBBBBBB",	"s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15"),
	LOG_FORMAT(GS1B, "BBBBBBBBBBBBBBBB",	"s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15"),
	LOG_FORMAT(TECS, "fffffffffffffB",	"ASP,AF,FSP,F,FF,AsSP,AsF,AsDSP,AsD,TERSP,TER,EDRSP,EDR,M"),
	LOG_FORMAT(WIND, "ffff",	"X,Y,CovX,CovY"),
	LOG_FORMAT(TPOS, "BQLLffffff", "SysID,Time,Lat,Lon,Alt,VelN,VelE,VelD,EPH,EPV"),
	LOG_FORMAT(EXTJ, "BBQLLffffff", "Type,SysID,Time,Lat,Lon,Alt,RAlt,VelN,VelE,VelD,Head"),
	LOG_FORMAT(FOLP, "fffffffffffff", "I,P,D,Vel,PointCo,DstToGate,DstToTraj,fx,fy,fz,sx,sy,sz"),
	LOG_FORMAT(LOTJ, "BQLLffffff", "Type,Time,Lat,Lon,Alt,RAlt,VelN,VelE,VelD,Head"),
	LOG_FORMAT(GPRE, "BLLfB",		"NavState,Lat,Lon,Alt,Type"),
	LOG_FORMAT(GNEX, "BLLfB",		"NavState,Lat,Lon,Alt,Type"),
	LOG_FORMAT(MVRX, MAVLINK_STATS_SDLOG_TYPES, MAVLINK_STATS_SDLOG_NAMES),
	LOG_FORMAT(MVTX, MAVLINK_STATS_SDLOG_TYPES, MAVLINK_STATS_SDLOG_NAMES),
	LOG_FORMAT(CMD, "HBBfff", "Cmd,SrcSys,SrcComp,Par1,Par2,Par3"),
	LOG_FORMAT(TGPS, "QBffLLfffB", "Time,Fix,EPH,EPV,Lat,Lon,Alt,Vel,COG,nSat"),
	LOG_FORMAT(BTSI, BT_SVC_IN_SDLOG_TYPES, BT_SVC_IN_SDLOG_NAMES),
	LOG_FORMAT(BTSO, BT_SVC_OUT_SDLOG_TYPES, BT_SVC_OUT_SDLOG_NAMES),
	LOG_FORMAT(BTEV, BT_EVT_STATUS_SDLOG_TYPES, BT_EVT_STATUS_SDLOG_NAMES),
	LOG_FORMAT(BTLK, BT_LINK_STATUS_SDLOG_TYPES, BT_LINK_STATUS_SDLOG_NAMES),
	//LOG_FORMAT(BTCH, "IIIIIIIIIIIIII", "xT1,xT2,xT3,xT4,xT5,xT6,xT7,Rx1,Rx2,Rx3,Rx4,Rx5,Rx6,Rx7"),
	LOG_FORMAT(BUTT, "H", "LastMask"),

	/* system-level messages, ID >= 0x80 */
	/* FMT: don't write format of format message, it's useless */
	LOG_FORMAT(TIME, "Q", "StartTime"),
	LOG_FORMAT(VER, "NZ", "Arch,FwGit"),
	LOG_FORMAT(PARM, "Nf", "Name,Value")
};

static const unsigned log_formats_num = sizeof(log_formats) / sizeof(log_formats[0]);

#endif /* SDLOG2_MESSAGES_H_ */
