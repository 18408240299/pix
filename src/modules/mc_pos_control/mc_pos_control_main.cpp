/****************************************************************************
 *
 *   Copyright (c) 2013, 2014 PX4 Development Team. All rights reserved.
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THr
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
 * @file mc_pos_control_main.cpp
 * Multicopter position controller.
 *
 * The controller has two loops: P loop for position error and PID loop for velocity error.
 * Output of velocity controller is thrust vector that splitted to thrust direction
 * (i.e. rotation matrix for multicopter orientation) and thrust module (i.e. multicopter thrust itself).
 * Controller doesn't use Euler angles for work, they generated only for more human-friendly control and logging.
 *
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <drivers/drv_hrt.h>
#include <arch/board/board.h>
#include <uORB/uORB.h>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/follow_offset.h>
#include <uORB/topics/home_position.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/position_restriction.h>
#include <uORB/topics/position_setpoint_triplet.h>
#include <uORB/topics/target_global_position.h>
#include <uORB/topics/user_camera_offsets.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_attitude_setpoint.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/vehicle_global_velocity_setpoint.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_local_position_setpoint.h>
#include <uORB/topics/vehicle_rates_setpoint.h>
#include <uORB/topics/vehicle_status.h>
#include <systemlib/param/param.h>
#include <systemlib/err.h>
#include <systemlib/systemlib.h>
#include <mathlib/mathlib.h>
#include <mathlib/math/filter/LowPassFilter.hpp>
#include <lib/geo/geo.h>
#include <lib/geo/position_predictor.h>
#include <mavlink/mavlink_log.h>
#include <systemlib/perf_counter.h>

#define TILT_COS_MAX	0.7f
#define SIGMA			0.000001f
#define MIN_DIST		0.01f
#define FOLLOW_OFFS_XY_MIN	2.0f

/**
 * Multicopter position control app start / stop handling function
 *
 * @ingroup apps
 */
extern "C" __EXPORT int mc_pos_control_main(int argc, char *argv[]);

// Sonar factory maximal distance, needed in distance corretion function

class MulticopterPositionControl
{
public:
	/**
	 * Constructor
	 */
	MulticopterPositionControl();

	/**
	 * Destructor, also kills task.
	 */
	~MulticopterPositionControl();

	/**
	 * Start task.
	 *
	 * @return		OK on success.
	 */
	int		start();

private:

	const float alt_ctl_dz = 0.2f;

	bool	_task_should_exit;		/**< if true, task should exit */
	int		_control_task;			/**< task handle for task */
	int		_mavlink_fd;			/**< mavlink fd */

    int     _cam_offset_sub;         /**< defined from leash camera offset subscription */
	int		_att_sub;				/**< vehicle attitude subscription */
	int		_att_sp_sub;			/**< vehicle attitude setpoint */
	int		_control_mode_sub;		/**< vehicle control mode subscription */
	int		_params_sub;			/**< notification of parameter updates */
	int		_manual_sub;			/**< notification of manual control updates */
	int		_arming_sub;			/**< arming status of outputs */
	int		_local_pos_sub;			/**< vehicle local position */
	int		_pos_sp_triplet_sub;	/**< position setpoint triplet */
	int		_local_pos_sp_sub;		/**< offboard local position setpoint */
	int		_global_vel_sp_sub;		/**< offboard global velocity setpoint */
	int		_target_pos_sub;		/**< target global position subscription */
    int     _vehicle_status_sub;    /**< vehicle status subscription */
    int 	_pos_restrict_sub;		/**< position restriction subscribtion */
    int 	_follow_offset_sub;		/**< offset for follow offset modes*/
    int		_home_pos_sub;			/**< home position */

	orb_advert_t	_att_sp_pub;			/**< attitude setpoint publication */
	orb_advert_t	_local_pos_sp_pub;		/**< vehicle local position setpoint publication */
	orb_advert_t	_global_vel_sp_pub;		/**< vehicle global velocity setpoint publication */
	orb_advert_t	_cam_control_pub;		/**< camera control publication */

	struct vehicle_attitude_s			_att;			/**< vehicle attitude */
	struct vehicle_attitude_setpoint_s		_att_sp;		/**< vehicle attitude setpoint */
	struct manual_control_setpoint_s		_manual;		/**< r/c channel data */
	struct vehicle_control_mode_s			_control_mode;	/**< vehicle control mode */
	struct actuator_armed_s				_arming;		/**< actuator arming status */
	struct vehicle_local_position_s			_local_pos;		/**< vehicle local position */
	struct position_setpoint_triplet_s		_pos_sp_triplet;	/**< vehicle global position setpoint triplet */
	struct vehicle_local_position_setpoint_s	_local_pos_sp;		/**< vehicle local position setpoint */
	struct vehicle_global_velocity_setpoint_s	_global_vel_sp;	/**< vehicle global velocity setpoint */
	struct target_global_position_s		_target_pos;	/**< target global position */
	struct actuator_controls_s			_cam_control;	/**< camera control */
	struct position_restriction_s		_pos_restrict;	/**< position restriction*/
    struct camera_user_offsets_s        _cam_offset;    /**< user defined camera offset */
    struct follow_offset_s              _orb_follow_offset; /** < follow offset for offset follow modes > */
    struct home_position_s 				_home_pos; /**< Home position in both global and local coordinates */
    bool 								_home_valid; /**< We can't relay on home_timestamp for the time being */

	struct {
        param_t max_sp_distance_regular;
        param_t max_sp_distance_max;
        param_t cbp_max_init_speed;
        param_t cam_pitch_step;
        param_t cam_yaw_step;
		param_t thr_min;
		param_t thr_max;
		param_t z_p;
		param_t z_vel_p;
		param_t z_vel_i;
		param_t z_vel_d;
		param_t z_vel_max_up;
		param_t z_vel_max_down;
		param_t z_ff;
		param_t xy_p;
		param_t xy_vel_p;
		param_t xy_vel_i;
		param_t xy_vel_d;
		param_t xy_vel_max;
		param_t xy_ff;
		param_t tilt_max_air;
		param_t land_speed_max;
        param_t land_speed_min;
        param_t land_sensor_validation_dist;
        param_t safe_land_h;
        param_t regular_land_speed;
        param_t land_correction_on;
		param_t takeoff_speed;
		param_t tilt_max_land;
        param_t max_land_xy_speed;
		param_t follow_vel_ff_z;
		param_t follow_vel_ff_xy;
		param_t follow_talt_offs;
		param_t follow_yaw_off_max;
		param_t follow_use_alt;
		param_t follow_rpt_alt;
		param_t follow_lpf_xy;
		param_t follow_lpf_z;
        param_t loi_step_len;
		param_t cam_pitch_max;
        param_t sonar_correction_on;
        param_t sonar_min_dist;
        param_t sonar_smooth_coef;
        param_t pafol_mode;
        param_t accept_radius;
        param_t pitch_lpf_cut;

        param_t follow_grad_ff_start;
        param_t follow_grad_ff_end;
        param_t follow_grad_ff;

        param_t vel_control_z_p_down;
        param_t vel_control_z_p_up;

		param_t yaw_rate_max;

        param_t yaw_dead_zone_r;
        param_t yaw_gradient_zone_r;

        param_t rtl_alt;

        param_t takeoff_warmup_time;
        param_t takeoff_gradient_time;

	}		_params_handles;		/**< handles for interesting parameters */

	struct {
        float max_sp_distance_regular;
        float max_sp_distance_max;
        float cbp_max_init_speed;
        float cam_pitch_step;
        float cam_yaw_step;
		float thr_min;
		float thr_max;
		float tilt_max_air;
		float land_speed_max;
        float land_speed_min;
        float land_sensor_validation_dist;
        float safe_land_h;
        float regular_land_speed;
		float takeoff_speed;
		float tilt_max_land;
        float max_land_xy_speed;
		float follow_vel_ff_z;
		float follow_vel_ff_xy;
		float follow_talt_offs;
		float follow_yaw_off_max;

        float follow_grad_ff_start;
        float follow_grad_ff_end;
        float follow_grad_ff;

        bool land_correction_on;
		bool follow_use_alt;
		bool follow_rpt_alt;
		float follow_lpf_xy;
		float follow_lpf_z;
        float loi_step_len;
		float cam_pitch_max;
        bool sonar_correction_on;
        float sonar_min_dist;
        float sonar_smooth_coef;
        int pafol_mode;
        float accept_radius;

        float vel_control_z_p_down;
        float vel_control_z_p_up;

        float yaw_dead_zone_r;
        float yaw_gradient_zone_r;

		float yaw_rate_max;					/**< max yaw rate */

		math::Vector<3> pos_p;
		math::Vector<3> vel_p;
		math::Vector<3> vel_ff;
		math::Vector<3> vel_i;
		math::Vector<3> vel_d;
		math::Vector<3> vel_max;
		math::Vector<3> sp_offs_max;

        float xy_vel_max;
        float z_vel_max_up;
        float z_vel_max_down;

		float pitch_lpf_cut;

		float rtl_alt;

		uint32_t takeoff_warmup_time;
		uint32_t takeoff_gradient_time;
	}		_params;

	struct map_projection_reference_s _ref_pos;
	float _ref_alt;
	hrt_abstime _ref_timestamp;
	float _alt_start;			/**< start altitude, i.e. when vehicle was armed */
	float _target_alt_start;	/**< target start altitude, i.e. when vehicle was armed or target was found first time (if vehicle was already armed) */

	bool _target_alt_start_valid;	/**< target start altitude valid flag */

    float _target_alt_on_follow_start;
    float _drone_alt_on_follow_start;
	bool _reset_pos_sp;
	bool _reset_alt_sp;
	bool _mode_auto;
    bool _mode_follow;
    bool _mode_attitude_hold;
	bool _reset_follow_offset;
    hrt_abstime landed_time;

    math::Vector<4> first_ground_correction;
	math::Vector<3> _pos;
	math::Vector<3> _pos_sp;
	math::Vector<3> _vel;
	math::Vector<3> _vel_sp;
	math::Vector<3> _vel_prev;	/**< velocity on previous step */

    math::Vector<3> _vel_ff;

    // Velocity feed foward componenet derived from sp_move_rate
    math::Vector<3> _vel_ff_sp_mv_r;
    // Velocity feed foward componenet derived from target movements
    math::Vector<3> _vel_ff_t;
    // Velocity feed foward component read from position setpoint vx, vy, vz
    math::Vector<3> _vel_ff_sp_v;

	math::Vector<3> _sp_move_rate;

	math::Vector<3> _tpos;

	math::Vector<3> _tvel;

    math::LowPassFilter<float> _tvel_lpf_x;
    math::LowPassFilter<float> _tvel_lpf_y;
    math::LowPassFilter<float> _tvel_lpf_z;


	math::Vector<3> _att_rates_ff;


	math::Vector<3> _follow_offset;		/**< offset from target for FOLLOW mode, vector in NED frame */

    math::Vector<2> _first_cbpark_point;  /**< cable park mode first point {lat, lon, alt} */
    math::Vector<2> _last_cbpark_point;  /**< cable park mode last point {lat, lon, alt} */
    math::Vector<2> _ref_vector; 		/**< cable park mode path normalized vector {x, y} */
    float _first_cbpark_point_alt;     /**< cable park mode first point altitude */
    float _last_cbpark_point_alt;     /**< cable park mode first point altitude */
    float _ref_vector_module = 1.0f;	/**< cable park vector module (before normalization) */
    float _current_allowed_velocity;    /**< cable park maximum speed (taking last and first point into account) */
    bool _valid_vel_correction = false; /**< cable park thing to use velocity correcion */
    bool _cbp_flight_to_possition = true;
    bool was_corrected_till_end = false;

	LocalPositionPredictor	_tpos_predictor;

	bool _ground_setpoint_corrected = false;
	bool _ground_position_invalid = false;
	float _ground_position_available_drop = 0;

    struct vehicle_status_s _vstatus;


	perf_counter_t _loop_perf;

	math::LowPassFilter<float> _pitchLPF;

	/**
	 * Update our local parameter cache.
	 */
	int			parameters_update(bool force);

	/**
	 * Update control outputs
	 */
	void		control_update();

	/**
	 * Check for changes in subscribed topics.
	 */
	void		poll_subscriptions();

	static float	scale_control(float ctl, float end, float dz);

	/**
	 * Update reference for local position projection
	 */
	void		update_ref();
	/**
	 * Reset position setpoint to current position
	 */
	void		reset_pos_sp();

	/**
	 * Reset altitude setpoint to current altitude
	 */
	void		reset_alt_sp();

	/**
	 * Check if position setpoint is too far from current position and adjust it if needed.
	 */
	void		limit_pos_sp_offset();

	/**
	 * Set position setpoint using manual control
	 */
	void		control_manual(float dt);

	/**
	 * Set position setpoint using offboard control
	 */
	void		control_offboard(float dt);

	bool		cross_sphere_line(const math::Vector<3>& sphere_c, float sphere_r,
					const math::Vector<3> line_a, const math::Vector<3> line_b, math::Vector<3>& res);

    /**
     * Controll cable park mode
     */
    void        control_cablepark();

	/**
     * Calculate velocity sp from pos_sp_triplet
	 */
	void		control_auto_vel(float dt);

	/**
	 * Set position setpoint for AUTO
	 */
	void		control_auto(float dt);

	/**
	 * Update target position and velocity (prediction and filtering)
	 */
	void		update_target_pos();

	/**
	 * Select between barometric and global (AMSL) altitudes
	 */
	void		select_alt(bool global);

	/**
	 * Reset follow offset to current offset.
	 */
	void		reset_follow_offset();

	/**
	 * Control setpoint if "follow target" mode
	 */
	void		control_follow(float dt);

	/**
	 * Calulate how much of full feed foward is necessary based on distance.
     * returned values are in range from 0.0 to 1.0 - zero to hundred percent of full feed foward
	 */
	float		follow_grad_ff(float distance);


	/**
	 * Control camera and copter yaw depending on mode
	 */
	void		point_to_target();

	/**
	 * Set camera pitch (smooth speed)
	 */
	void		set_camera_pitch(float pitch);

	/**
	 * Set camera yaw (smooth speed)
	 */
    void        set_camera_yaw();

	/**
	 * Shim for calling task_main from task_create.
	 */
	static void	task_main_trampoline(int argc, char *argv[]);

	/**
	 * Main sensor collection task.
	 */
	void		task_main();

    /* author:       Max <max@airdog.com>
     * description:  Reject any setpoint further than value defined in parameters
     * return:       True if no reset needed, False if reset needed
     */
    bool        validate_setpoint_distance();

	/**
     * Change setpoint Z coordinate according to sonar measurements
     */
    bool       ground_dist_correction();

    /**
     * Calculate landing coefficient if needed
     */
    float      landing_speed_correction();

    /*
     * Check vehicle_status topic and update _vstatus if it's updated
     */
    bool       update_vehicle_status();

};

namespace pos_control
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

MulticopterPositionControl	*g_control;
}

MulticopterPositionControl::MulticopterPositionControl() :

	_task_should_exit(false),
	_control_task(-1),
	_mavlink_fd(-1),

/* subscriptions */
    _cam_offset_sub(-1),
	_att_sub(-1),
	_att_sp_sub(-1),
	_control_mode_sub(-1),
	_params_sub(-1),
	_manual_sub(-1),
	_arming_sub(-1),
	_local_pos_sub(-1),
	_pos_sp_triplet_sub(-1),
	_global_vel_sp_sub(-1),
	_follow_offset_sub(-1),
	_home_pos_sub(-1),

/* publications */
	_att_sp_pub(-1),
	_local_pos_sp_pub(-1),
	_global_vel_sp_pub(-1),
	_cam_control_pub(-1),

	_home_valid(false),

	_ref_alt(0.0f),
	_ref_timestamp(0),
	_alt_start(0.0f),
	_target_alt_start(0.0f),
	_target_alt_start_valid(false),
    _target_alt_on_follow_start(0.0f),
    _drone_alt_on_follow_start(0.0f),

	_reset_pos_sp(true),
	_reset_alt_sp(true),
	_mode_auto(false),
    _mode_follow(false),
	_reset_follow_offset(true),
    landed_time(0),
	_loop_perf(perf_alloc(PC_ELAPSED, "mc_pos_control")),
	_pitchLPF ()
{
    memset(&_cam_offset, 0, sizeof(_cam_offset));
	memset(&_att, 0, sizeof(_att));
	memset(&_att_sp, 0, sizeof(_att_sp));
	memset(&_manual, 0, sizeof(_manual));
	memset(&_control_mode, 0, sizeof(_control_mode));
	memset(&_arming, 0, sizeof(_arming));
	memset(&_local_pos, 0, sizeof(_local_pos));
	memset(&_pos_sp_triplet, 0, sizeof(_pos_sp_triplet));
	memset(&_local_pos_sp, 0, sizeof(_local_pos_sp));
	memset(&_global_vel_sp, 0, sizeof(_global_vel_sp));
	memset(&_target_pos, 0, sizeof(_target_pos));
	memset(&_cam_control, 0, sizeof(_cam_control));

	memset(&_ref_pos, 0, sizeof(_ref_pos));
	memset(&_home_pos, 0, sizeof(_home_pos));

	_params.pos_p.zero();
	_params.vel_p.zero();
	_params.vel_i.zero();
	_params.vel_d.zero();
	_params.vel_max.zero();
	_params.vel_ff.zero();
	_params.sp_offs_max.zero();

	_pos.zero();
	_pos_sp.zero();
	_vel.zero();
	_vel_sp.zero();
	_vel_prev.zero();
	_vel_ff.zero();
	_vel_ff_sp_v.zero();
	_vel_ff_sp_mv_r.zero();
	_vel_ff_t.zero();
	_sp_move_rate.zero();

	_tpos.zero();
	_tvel.zero();

	_sp_move_rate.zero();
	_att_rates_ff.zero();

	/* initialize to safe value to avoid flying into target */
	_follow_offset.zero();
	_follow_offset(2) = -20.0f;

	_tpos_predictor.set_min_latency(20000);
	_tpos_predictor.set_max_latency(1000000);

    _params_handles.max_sp_distance_regular = param_find("A_M_SPDIST_RANGE");
    _params_handles.max_sp_distance_max = param_find("A_M_SPDIST_MAX");
    _params_handles.cbp_max_init_speed = param_find("CBP_MAX_INIT_SPD");
    _params_handles.cam_pitch_step = param_find("CAM_PITCH_STEP");
    _params_handles.cam_yaw_step = param_find("CAM_YAW_STEP");
	_params_handles.thr_min		= param_find("MPC_THR_MIN");
	_params_handles.thr_max		= param_find("MPC_THR_MAX");
	_params_handles.z_p			= param_find("MPC_Z_P");
	_params_handles.z_vel_p		= param_find("MPC_Z_VEL_P");
	_params_handles.z_vel_i		= param_find("MPC_Z_VEL_I");
	_params_handles.z_vel_d		= param_find("MPC_Z_VEL_D");

	_params_handles.z_vel_max_up	= param_find("MPC_Z_VEL_MAX_U");
	_params_handles.z_vel_max_down	= param_find("MPC_Z_VEL_MAX_D");

	_params_handles.z_ff		= param_find("MPC_Z_FF");
	_params_handles.xy_p		= param_find("MPC_XY_P");
	_params_handles.xy_vel_p	= param_find("MPC_XY_VEL_P");
	_params_handles.xy_vel_i	= param_find("MPC_XY_VEL_I");
	_params_handles.xy_vel_d	= param_find("MPC_XY_VEL_D");
	_params_handles.xy_vel_max	= param_find("MPC_XY_VEL_MAX");
	_params_handles.xy_ff		= param_find("MPC_XY_FF");
	_params_handles.tilt_max_air	= param_find("MPC_TILTMAX_AIR");
	_params_handles.land_speed_max	= param_find("LAND_MAX_V");
    _params_handles.land_speed_min  = param_find("LAND_MIN_V");
    _params_handles.land_sensor_validation_dist  = param_find("LAND_SENS_VALID");
    _params_handles.safe_land_h     = param_find("LAND_SAFE_H");
    _params_handles.regular_land_speed = param_find("LAND_REG_V");
    _params_handles.land_correction_on = param_find("LAND_CORR_ON");
    _params_handles.max_land_xy_speed = param_find("LAND_MAX_XY_V");
	_params_handles.takeoff_speed	= param_find("MPC_TAKEOFF_SPD");
    _params_handles.takeoff_warmup_time = param_find("MPC_TAKEOFF_WARM");
    _params_handles.takeoff_gradient_time = param_find("MPC_TAKEOFF_GRAD");

    _params_handles.yaw_dead_zone_r = param_find("A_YAW_DEAD_Z_R");
    _params_handles.yaw_gradient_zone_r = param_find("A_YAW_GRAD_Z_R");

	_params_handles.yaw_rate_max	= 	param_find("MC_YAWRATE_MAX");

	_params_handles.tilt_max_land	= param_find("MPC_TILTMAX_LND");
	_params_handles.follow_vel_ff_xy	= param_find("FOL_VEL_FF_XY");
	_params_handles.follow_vel_ff_z	= param_find("FOL_VEL_FF_Z");
	_params_handles.follow_talt_offs	= param_find("FOL_TALT_OFF");
	_params_handles.follow_yaw_off_max	= param_find("FOL_YAW_OFF_MAX");
	_params_handles.follow_use_alt	= param_find("FOL_USE_ALT");
	_params_handles.follow_rpt_alt	= param_find("FOL_RPT_ALT");

    _params_handles.follow_grad_ff_start = param_find("FOL_FF_GRAD_STRT");
    _params_handles.follow_grad_ff_end = param_find("FOL_FF_GRAD_END");
    _params_handles.follow_grad_ff = param_find("FOL_FF_GRAD_USE");

	_params_handles.follow_lpf_xy	= param_find("FOL_LPF_XY");
	_params_handles.follow_lpf_z	= param_find("FOL_LPF_Z");
	_params_handles.cam_pitch_max	= param_find("CAM_P_MAX");

    _params_handles.loi_step_len = param_find("LOI_STEP_LEN");

    _params_handles.sonar_correction_on     = param_find("SENS_SON_ON");
    _params_handles.sonar_min_dist          = param_find("SENS_SON_MIN");
    _params_handles.sonar_smooth_coef       = param_find("SENS_SON_SMOT");
    _params_handles.pafol_mode				= param_find("PAFOL_MODE");

    _params_handles.accept_radius = param_find("NAV_ACC_RAD");

    _params_handles.pitch_lpf_cut = param_find("MPC_PITCH_LPF");

    _params_handles.vel_control_z_p_up = param_find("MPC_CVEL_ZP_UP");
    _params_handles.vel_control_z_p_down = param_find("MPC_CVEL_ZP_DWN");

    _params_handles.rtl_alt = param_find("RTL_RET_ALT");

	/* fetch initial parameter values */
	parameters_update(true);
}

MulticopterPositionControl::~MulticopterPositionControl()
{
	if (_control_task != -1) {
		/* task wakes up every 100ms or so at the longest */
		_task_should_exit = true;

		/* wait for a second for the task to quit at our request */
		unsigned i = 0;

		do {
			/* wait 20ms */
			usleep(20000);

			/* if we have given up, kill it */
			if (++i > 50) {
				task_delete(_control_task);
				break;
			}
		} while (_control_task != -1);
	}

	pos_control::g_control = nullptr;
}

int
MulticopterPositionControl::parameters_update(bool force)
{
	bool updated;
	struct parameter_update_s param_upd;

	orb_check(_params_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(parameter_update), _params_sub, &param_upd);
	}

	if (updated || force) {
        param_get(_params_handles.max_sp_distance_regular, &_params.max_sp_distance_regular);
        param_get(_params_handles.max_sp_distance_max, &_params.max_sp_distance_max);
		param_get(_params_handles.cbp_max_init_speed, &_params.cbp_max_init_speed);
		param_get(_params_handles.cam_pitch_step, &_params.cam_pitch_step);
		param_get(_params_handles.cam_yaw_step, &_params.cam_yaw_step);
		param_get(_params_handles.thr_min, &_params.thr_min);
		param_get(_params_handles.thr_max, &_params.thr_max);
		param_get(_params_handles.tilt_max_air, &_params.tilt_max_air);
		_params.tilt_max_air = math::radians(_params.tilt_max_air);
		param_get(_params_handles.land_speed_max, &_params.land_speed_max);
        param_get(_params_handles.land_speed_min, &_params.land_speed_min);
        param_get(_params_handles.land_sensor_validation_dist, &_params.land_sensor_validation_dist);
        param_get(_params_handles.safe_land_h, &_params.safe_land_h);
        param_get(_params_handles.regular_land_speed, &_params.regular_land_speed);
        param_get(_params_handles.land_correction_on, &_params.land_correction_on);
		param_get(_params_handles.takeoff_speed, &_params.takeoff_speed);
		param_get(_params_handles.takeoff_warmup_time, &_params.takeoff_warmup_time);
		param_get(_params_handles.takeoff_gradient_time, &_params.takeoff_gradient_time);
		param_get(_params_handles.tilt_max_land, &_params.tilt_max_land);
		_params.tilt_max_land = math::radians(_params.tilt_max_land);
        param_get(_params_handles.max_land_xy_speed, &_params.max_land_xy_speed);
		param_get(_params_handles.follow_vel_ff_xy, &_params.follow_vel_ff_xy);
		param_get(_params_handles.follow_vel_ff_z, &_params.follow_vel_ff_z);

		param_get(_params_handles.yaw_dead_zone_r, &_params.yaw_dead_zone_r);
		param_get(_params_handles.yaw_gradient_zone_r, &_params.yaw_gradient_zone_r);

        param_get(_params_handles.yaw_rate_max, &_params.yaw_rate_max);
        _params.yaw_rate_max = math::radians(_params.yaw_rate_max);

		param_get(_params_handles.follow_talt_offs, &_params.follow_talt_offs);
		param_get(_params_handles.follow_yaw_off_max, &_params.follow_yaw_off_max);
		_params.follow_yaw_off_max = math::radians(_params.follow_yaw_off_max);
		param_get(_params_handles.follow_lpf_xy, &_params.follow_lpf_xy);
		param_get(_params_handles.follow_lpf_z, &_params.follow_lpf_z);

		param_get(_params_handles.follow_grad_ff, &_params.follow_grad_ff);
		param_get(_params_handles.follow_grad_ff_start, &_params.follow_grad_ff_start);
		param_get(_params_handles.follow_grad_ff_end, &_params.follow_grad_ff_end);

		_tvel_lpf_x.set_cutoff_frequency(_params.follow_lpf_xy);
		_tvel_lpf_y.set_cutoff_frequency(_params.follow_lpf_xy);
		_tvel_lpf_z.set_cutoff_frequency(_params.follow_lpf_z);

		param_get(_params_handles.loi_step_len, &_params.loi_step_len);
		param_get(_params_handles.cam_pitch_max, &_params.cam_pitch_max);
		_params.cam_pitch_max = math::radians(_params.cam_pitch_max);

		int32_t i;
		param_get(_params_handles.pafol_mode, &i);
		        _params.pafol_mode = i;

		param_get(_params_handles.follow_use_alt, &i);
		_params.follow_use_alt = (i != 0);
		param_get(_params_handles.follow_rpt_alt, &i);
		_params.follow_rpt_alt = (i != 0);

        param_get(_params_handles.sonar_correction_on, &i);
        _params.sonar_correction_on = i;
		float v;
		param_get(_params_handles.xy_p, &v);
		_params.pos_p(0) = v;
		_params.pos_p(1) = v;
		param_get(_params_handles.z_p, &v);
		_params.pos_p(2) = v;
		param_get(_params_handles.xy_vel_p, &v);
		_params.vel_p(0) = v;
		_params.vel_p(1) = v;
		param_get(_params_handles.z_vel_p, &v);
		_params.vel_p(2) = v;
		param_get(_params_handles.xy_vel_i, &v);
		_params.vel_i(0) = v;
		_params.vel_i(1) = v;
		param_get(_params_handles.z_vel_i, &v);
		_params.vel_i(2) = v;
		param_get(_params_handles.xy_vel_d, &v);
		_params.vel_d(0) = v;
		_params.vel_d(1) = v;
		param_get(_params_handles.z_vel_d, &v);
		_params.vel_d(2) = v;
		param_get(_params_handles.xy_vel_max, &v);
		_params.vel_max(0) = v;
		_params.vel_max(1) = v;
        _params.xy_vel_max = v;
		param_get(_params_handles.z_vel_max_up, &v);
		_params.vel_max(2) = v;
        _params.z_vel_max_up = v;
		param_get(_params_handles.z_vel_max_down, &v);
        _params.z_vel_max_down = v;
		param_get(_params_handles.xy_ff, &v);
		v = math::constrain(v, 0.0f, 1.0f);
		_params.vel_ff(0) = v;
		_params.vel_ff(1) = v;
		param_get(_params_handles.z_ff, &v);
		v = math::constrain(v, 0.0f, 1.0f);
		_params.vel_ff(2) = v;
        param_get(_params_handles.sonar_min_dist, &v);
        _params.sonar_min_dist = v;
        param_get(_params_handles.sonar_smooth_coef, &v);
        _params.sonar_smooth_coef = v;

		_params.sp_offs_max = _params.vel_max.edivide(_params.pos_p) * 2.0f;

		param_get(_params_handles.accept_radius, &_params.accept_radius);

		param_get(_params_handles.pitch_lpf_cut, &_params.pitch_lpf_cut);
		if (_params.pitch_lpf_cut < 0.0f) {
			_pitchLPF.set_cutoff_frequency(-_params.pitch_lpf_cut);
		}
		else {
			_pitchLPF.set_cutoff_frequency(_params.pitch_lpf_cut);
		}

        param_get(_params_handles.vel_control_z_p_up, &_params.vel_control_z_p_up);
        param_get(_params_handles.vel_control_z_p_down, &_params.vel_control_z_p_down);

        param_get(_params_handles.rtl_alt, &_params.rtl_alt);

	}

	return OK;
}

void
MulticopterPositionControl::poll_subscriptions()
{
	bool updated;

    orb_check(_cam_offset_sub, &updated);

    if (updated) {
        orb_copy(ORB_ID(camera_user_offsets), _cam_offset_sub, &_cam_offset);
    }

	orb_check(_att_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(vehicle_attitude), _att_sub, &_att);
	}

	orb_check(_att_sp_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(vehicle_attitude_setpoint), _att_sp_sub, &_att_sp);
	}

	orb_check(_control_mode_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(vehicle_control_mode), _control_mode_sub, &_control_mode);
	}

	orb_check(_manual_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(manual_control_setpoint), _manual_sub, &_manual);
	}

	orb_check(_arming_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(actuator_armed), _arming_sub, &_arming);
	}

	orb_check(_local_pos_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(vehicle_local_position), _local_pos_sub, &_local_pos);
	}

	orb_check(_target_pos_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(target_global_position), _target_pos_sub, &_target_pos);
	}

    orb_check(_vehicle_status_sub, &updated);

    if (updated) {
        orb_copy(ORB_ID(vehicle_status), _vehicle_status_sub, &_vstatus);
    }

    orb_check(_follow_offset_sub, &updated);

    if (updated) {
        orb_copy(ORB_ID(follow_offset), _follow_offset_sub, &_orb_follow_offset);
    }

    orb_check(_home_pos_sub, &updated);
	if (updated) {
		orb_copy(ORB_ID(home_position), _home_pos_sub, &_home_pos);
		_home_valid = true;
		if (_ref_timestamp != 0) {
			/* Reproject home to be on the safe side. Not all the publications set x, y, z */
			map_projection_project(&_ref_pos, _home_pos.lat, _home_pos.lon, &_home_pos.x, &_home_pos.y);
			_home_pos.z = -(_home_pos.alt - _ref_alt);
		}
		/*
		 * If ref is invalid and home publication didn't have x, y, z...
		 * We still won't fly to home without ref and on ref update home will be projected
		 */
	}

}

float
MulticopterPositionControl::scale_control(float ctl, float end, float dz)
{
	if (ctl > dz) {
		return (ctl - dz) / (end - dz);

	} else if (ctl < -dz) {
		return (ctl + dz) / (end - dz);

	} else {
		return 0.0f;
	}
}

void
MulticopterPositionControl::task_main_trampoline(int argc, char *argv[])
{
	pos_control::g_control->task_main();
}

void
MulticopterPositionControl::update_ref()
{
	if (_local_pos.ref_timestamp != _ref_timestamp) {
		double lat_sp, lon_sp;
		float alt_sp = 0.0f;

		if (_ref_timestamp != 0) {
			/* calculate current position setpoint in global frame */
			map_projection_reproject(&_ref_pos, _pos_sp(0), _pos_sp(1), &lat_sp, &lon_sp);
			alt_sp = _ref_alt - _pos_sp(2);
		}

		/* update local projection reference */
		map_projection_init(&_ref_pos, _local_pos.ref_lat, _local_pos.ref_lon);
		_ref_alt = _local_pos.ref_alt;

		if (_ref_timestamp != 0) {
			/* reproject position setpoint to new reference */
			map_projection_project(&_ref_pos, lat_sp, lon_sp, &_pos_sp.data[0], &_pos_sp.data[1]);
			_pos_sp(2) = -(alt_sp - _ref_alt);

			if (_home_valid) {
				/* Reproject home in case ref was changed */
				map_projection_project(&_ref_pos, _home_pos.lat, _home_pos.lon, &_home_pos.x, &_home_pos.y);
				_home_pos.z = -(_home_pos.alt - _ref_alt);
			}
		}

		_ref_timestamp = _local_pos.ref_timestamp;
	}
}

void
MulticopterPositionControl::reset_pos_sp()
{
	if (_reset_pos_sp) {
		_reset_pos_sp = false;
		/* shift position setpoint to make attitude setpoint continuous */
		_pos_sp(0) = _pos(0) + (_vel(0) - _att_sp.R_body[0][2] * _att_sp.thrust / _params.vel_p(0)
				- _params.vel_ff(0) * _sp_move_rate(0)) / _params.pos_p(0);
		_pos_sp(1) = _pos(1) + (_vel(1) - _att_sp.R_body[1][2] * _att_sp.thrust / _params.vel_p(1)
				- _params.vel_ff(1) * _sp_move_rate(1)) / _params.pos_p(1);
		// Can happen, for example, when att_sp is 0 and velocity P is 0 too
		if (!isfinite(_pos_sp(0)) || !isfinite(_pos_sp(1))) {
			_pos_sp(0) = _pos(0);
			_pos_sp(1) = _pos(1);
		}
		mavlink_log_info(_mavlink_fd, "[mpc] reset pos sp: %.2f, %.2f", (double)_pos_sp(0), (double)_pos_sp(1));
	}
}

void
MulticopterPositionControl::reset_follow_offset()
{
	if (_reset_follow_offset) {
		_reset_follow_offset = false;

		/* use current position or position setpoint */
		math::Vector<3> pos;
		if (_reset_pos_sp) {
			pos(0) = _pos(0);
			pos(1) = _pos(1);

		} else {
			pos(0) = _pos_sp(0);
			pos(1) = _pos_sp(1);
		}

		pos(2) = _reset_alt_sp ? _pos(2) : _pos_sp(2);

		_follow_offset = pos - _tpos;

		mavlink_log_info(_mavlink_fd, "[mpc] reset follow offs: %.2f, %.2f, %.2f",
				(double)_follow_offset(0), (double)_follow_offset(1), (double)_follow_offset(2));
	}
}

bool
MulticopterPositionControl::update_vehicle_status()
{
    bool vehicle_status_updated = false;
    orb_check(_vehicle_status_sub, &vehicle_status_updated);

    if (vehicle_status_updated) {
        if (orb_copy(ORB_ID(vehicle_status), _vehicle_status_sub, &_vstatus) == OK)
            return true;
        else
            return false;
    }
    return false;
}

void
MulticopterPositionControl::reset_alt_sp()
{
	if (_reset_alt_sp) {
		_reset_alt_sp = false;
		_pos_sp(2) = _pos(2) + (_vel(2) - _params.vel_ff(2) * _sp_move_rate(2)) / _params.pos_p(2);
		// Can happen if position P is 0
		if (!isfinite(_pos_sp(2))) {
			_pos_sp(2) = _pos(2);
		}
		mavlink_log_info(_mavlink_fd, "[mpc] reset alt sp: %.2f", -(double)_pos_sp(2));
	}
}

void
MulticopterPositionControl::limit_pos_sp_offset()
{
	math::Vector<3> pos_sp_offs;
	pos_sp_offs.zero();

	if (_control_mode.flag_control_position_enabled) {
		pos_sp_offs(0) = (_pos_sp(0) - _pos(0)) / _params.sp_offs_max(0);
		pos_sp_offs(1) = (_pos_sp(1) - _pos(1)) / _params.sp_offs_max(1);
	}

	if (_control_mode.flag_control_altitude_enabled) {
		pos_sp_offs(2) = (_pos_sp(2) - _pos(2)) / _params.sp_offs_max(2);
	}

	float pos_sp_offs_norm = pos_sp_offs.length();

	if (pos_sp_offs_norm > 1.0f) {
		pos_sp_offs /= pos_sp_offs_norm;
		_pos_sp = _pos + pos_sp_offs.emult(_params.sp_offs_max);
	}
}

void
MulticopterPositionControl::control_manual(float dt)
{
	_sp_move_rate.zero();

	if (_control_mode.flag_control_altitude_enabled) {
		/* move altitude setpoint with throttle stick */
		_sp_move_rate(2) = -scale_control(_manual.z - 0.5f, 0.5f, alt_ctl_dz);
	}

	if (_control_mode.flag_control_position_enabled) {
		/* move position setpoint with roll/pitch stick */
		_sp_move_rate(0) = _manual.x;
		_sp_move_rate(1) = _manual.y;
	}

	/* limit setpoint move rate */
	float sp_move_norm = _sp_move_rate.length();

	if (sp_move_norm > 1.0f) {
		_sp_move_rate /= sp_move_norm;
	}

	/* _sp_move_rate scaled to 0..1, scale it to max speed and rotate around yaw */
	math::Matrix<3, 3> R_yaw_sp;
	R_yaw_sp.from_euler(0.0f, 0.0f, _att_sp.yaw_body);
	_sp_move_rate = R_yaw_sp * _sp_move_rate.emult(_params.vel_max);

	if (_control_mode.flag_control_altitude_enabled) {
		/* reset alt setpoint to current altitude if needed */
		reset_alt_sp();
	}

	if (_control_mode.flag_control_position_enabled) {
		/* reset position setpoint to current position if needed */
		reset_pos_sp();
	}

	/* feed forward setpoint move rate with weight vel_ff */
	_vel_ff_sp_mv_r = _sp_move_rate.emult(_params.vel_ff);

	/* move position setpoint */
	_pos_sp += _sp_move_rate * dt;

	/* check if position setpoint is too far from actual position */
	math::Vector<3> pos_sp_offs;
	pos_sp_offs.zero();

	if (_control_mode.flag_control_position_enabled) {
		pos_sp_offs(0) = (_pos_sp(0) - _pos(0)) / _params.sp_offs_max(0);
		pos_sp_offs(1) = (_pos_sp(1) - _pos(1)) / _params.sp_offs_max(1);
	}

	if (_control_mode.flag_control_altitude_enabled) {
		pos_sp_offs(2) = (_pos_sp(2) - _pos(2)) / _params.sp_offs_max(2);
	}

	float pos_sp_offs_norm = pos_sp_offs.length();

	if (pos_sp_offs_norm > 1.0f) {
		pos_sp_offs /= pos_sp_offs_norm;
		_pos_sp = _pos + pos_sp_offs.emult(_params.sp_offs_max);
	}
}

void
MulticopterPositionControl::control_offboard(float dt)
{
	bool updated;
	orb_check(_pos_sp_triplet_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(position_setpoint_triplet), _pos_sp_triplet_sub, &_pos_sp_triplet);
	}

	if (_pos_sp_triplet.current.valid) {
		if (_control_mode.flag_control_position_enabled && _pos_sp_triplet.current.position_valid) {
			/* control position */
			_pos_sp(0) = _pos_sp_triplet.current.x;
			_pos_sp(1) = _pos_sp_triplet.current.y;
			_pos_sp(2) = _pos_sp_triplet.current.z;

		} else if (_control_mode.flag_control_velocity_enabled && _pos_sp_triplet.current.velocity_valid) {
			/* control velocity */
			/* reset position setpoint to current position if needed */
			reset_pos_sp();

			/* set position setpoint move rate */
			_sp_move_rate(0) = _pos_sp_triplet.current.vx;
			_sp_move_rate(1) = _pos_sp_triplet.current.vy;
		}

		if (_pos_sp_triplet.current.yaw_valid) {
			_att_sp.yaw_body = _pos_sp_triplet.current.yaw;
		} else if (_pos_sp_triplet.current.yawspeed_valid) {
			_att_sp.yaw_body = _att_sp.yaw_body + _pos_sp_triplet.current.yawspeed * dt;
		}

		if (_control_mode.flag_control_altitude_enabled) {
			/* reset alt setpoint to current altitude if needed */
			reset_alt_sp();

			/* set altitude setpoint move rate */
			_sp_move_rate(2) = _pos_sp_triplet.current.vz;
		}

		/* feed forward setpoint move rate with weight vel_ff */
		_vel_ff_sp_mv_r = _sp_move_rate.emult(_params.vel_ff);

		/* move position setpoint */
		_pos_sp += _sp_move_rate * dt;

	} else {
		reset_pos_sp();
		reset_alt_sp();
	}
}

bool
MulticopterPositionControl::cross_sphere_line(const math::Vector<3>& sphere_c, float sphere_r,
		const math::Vector<3> line_a, const math::Vector<3> line_b, math::Vector<3>& res)
{
	/* project center of sphere on line */
	/* normalized AB */
	math::Vector<3> ab_norm = line_b - line_a;
	ab_norm.normalize();
	math::Vector<3> d = line_a + ab_norm * ((sphere_c - line_a) * ab_norm);
	float cd_len = (sphere_c - d).length();

	/* we have triangle CDX with known CD and CX = R, find DX */
	if (sphere_r > cd_len) {
		/* have two roots, select one in A->B direction from D */
		float dx_len = sqrtf(sphere_r * sphere_r - cd_len * cd_len);
		res = d + ab_norm * dx_len;
		return true;

	} else {
		/* have no roots, return D */
		res = d;
		return false;
	}
}

void
MulticopterPositionControl::control_auto_vel(float dt) {

	bool updated;
	orb_check(_pos_sp_triplet_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(position_setpoint_triplet), _pos_sp_triplet_sub, &_pos_sp_triplet);
	}

	if (_pos_sp_triplet.current.valid) {

		// Mark next call to reset as valid
		_reset_pos_sp = true;
		_reset_alt_sp = true;

		// Project setpoint position to local position to use by default
		map_projection_project(&_ref_pos,
				_pos_sp_triplet.current.lat, _pos_sp_triplet.current.lon,
				&_pos_sp.data[0], &_pos_sp.data[1]);

        _pos_sp(2) = -(_pos_sp_triplet.current.alt - _ref_alt);

        math::Vector<3> pos_delta = _pos_sp - _pos;

		if (_pos_sp_triplet.current.abs_velocity_valid && _pos_sp != _pos) {

            math::Vector<2> xy_move_direction;
            math::Vector<2> xy_pos_delta(pos_delta(0), pos_delta(1));

            xy_move_direction = xy_pos_delta.normalized();

            if (_pos_sp_triplet.next.valid) {
                // TODO:  L1 implementation using next/ prev setpoints
            }
            else
            {

                float z_delta_len = pos_delta(2);
                float xy_speed = _pos_sp_triplet.current.abs_velocity;

                float z_speed = z_delta_len * _params.pos_p(2);

                _vel_sp(0) = xy_move_direction(0) * xy_speed;
                _vel_sp(1) = xy_move_direction(1) * xy_speed;
                _vel_sp(2) = z_speed;

            }

        } else {
            _vel_sp.zero();
        }

	} else {
		// Resets position only once, if reset_pos is true
		reset_pos_sp();
		reset_alt_sp();
        _vel_sp.zero();
	}

}

/**
 * @author: Max Shvetsov <max@airdog.com>
 * @description: this mode should controll leashed_follow mode
 *      NOTE: All vactors are calculated from the first point
 */
void
MulticopterPositionControl::control_cablepark()
{
    // This thing is already in local coordinates

	bool updated = false;
	orb_check(_pos_restrict_sub, &updated);

    if (updated) {
        orb_copy(ORB_ID(position_restriction), _pos_restrict_sub, &_pos_restrict);

        // get first and last points in local coordinates
    	map_projection_project(
                &_ref_pos
                ,_pos_restrict.line.first[0]
                ,_pos_restrict.line.first[1]
                ,&_first_cbpark_point(0)
                ,&_first_cbpark_point(1)
                );
        _first_cbpark_point_alt = -((float)_pos_restrict.line.first[2] - _ref_alt);

        map_projection_project(
                &_ref_pos
                ,_pos_restrict.line.last[0]
                ,_pos_restrict.line.last[1]
                ,&_last_cbpark_point(0)
                ,&_last_cbpark_point(1)
                );
        _last_cbpark_point_alt = -((float)_pos_restrict.line.last[2] - _ref_alt);

        _ref_vector = _last_cbpark_point - _first_cbpark_point;
        // We need this vector module for the future use
		_ref_vector_module = _ref_vector.length();
        // Normalize reference vector now
        _ref_vector /= _ref_vector_module;
    }

    math::Vector<2> final_vector;
    math::Vector<2> vehicle_pos( _local_pos.x - _first_cbpark_point(0), _local_pos.y - _first_cbpark_point(1) );
    math::Vector<2> target_pos( _tpos(0) - _first_cbpark_point(0), _tpos(1) - _first_cbpark_point(1) );

    // Calculating dot product of vehicle vector and path vector
    float vehicle_dot_product = _ref_vector * vehicle_pos;

    // Limiting product not to be great than module of path vector
    float v_v_length = vehicle_pos.length();
    float from_vehicle_to_path = (v_v_length * v_v_length) - (vehicle_dot_product * vehicle_dot_product);
    // Use correction only if on path
    _valid_vel_correction = false;

    /* --- if we are outside of path - return to it first -- */
	if ( (from_vehicle_to_path > _params.accept_radius * _params.accept_radius
         || vehicle_dot_product >= _ref_vector_module + _params.accept_radius
         || vehicle_dot_product < -_params.accept_radius)
         && _cbp_flight_to_possition 
       ) {

        // Changing projection if vehicle outside of last/first points
        if (vehicle_dot_product >= _ref_vector_module) {
            vehicle_dot_product = _ref_vector_module;
        } else if (vehicle_dot_product < 0.0f) {
            vehicle_dot_product = 0.0f;
        }
        // Calculating vector from path start to desired point on path
        final_vector = _ref_vector * vehicle_dot_product;

        // ===== Resulting vector =====
        //final_vector -= vehicle_pos;
        _cbp_flight_to_possition = false;

    } else {
    /* -- We are on path and could follow target now -- */
        //Calculating dot product of target vector and path vector
        float target_dot_product = _ref_vector * target_pos;
        if (target_dot_product >= _ref_vector_module) {
            target_dot_product = _ref_vector_module;
        } else if (target_dot_product < 0.0f) {
            target_dot_product = 0.0f;
        } else {
            // Calculating velocity
            math::Vector<2> target_velocity(_tvel(0), _tvel(1));
            float required_velocity = target_velocity * _ref_vector * _params.follow_vel_ff_xy;

            math::Vector<2> resulting_velocity = _ref_vector * required_velocity;

            // Correcting velocity if near first or last point
            //float current_allowed_velocity;
            if (fabsf(target_dot_product) > fabsf(vehicle_dot_product)) {
                // If we are comming to last point
                _current_allowed_velocity = fabsf(_ref_vector_module - vehicle_dot_product) * _params.pos_p(0);
            } else {
                // Comming to first point
                _current_allowed_velocity = fabsf(vehicle_dot_product) * _params.pos_p(0);
            }
            _valid_vel_correction = true;
            //if (fabsf(required_velocity) > _current_allowed_velocity) {
            //    resulting_velocity *= _current_allowed_velocity/fabsf(required_velocity);
            //}

            _vel_ff_t(0) = resulting_velocity(0);
            _vel_ff_t(1) = resulting_velocity(1);
        }

        // Calculating vector from path start to desired point on path
        final_vector = _ref_vector * target_dot_product;

        // ===== Resulting vector =====
        //final_vector -= vehicle_pos;
    }
    // Returning to local pos of mc_pos_contoll (not starting from the first cable park point)
    _pos_sp(0) = final_vector(0) + _first_cbpark_point(0);
    _pos_sp(1) = final_vector(1) + _first_cbpark_point(1);
    //_pos_sp(2) = _first_cbpark_point_alt; // TODO [Max]: This should be fixed, now working wrong cause of ref_alt changes..
}

void
MulticopterPositionControl::control_auto(float dt)
{
	if (!_mode_auto) {
		_mode_auto = true;
		/* reset position setpoint on AUTO mode activation */
		reset_pos_sp();
		reset_alt_sp();
	}

	bool updated;
	orb_check(_pos_sp_triplet_sub, &updated);

	if (updated) {
		orb_copy(ORB_ID(position_setpoint_triplet), _pos_sp_triplet_sub, &_pos_sp_triplet);
	}

	// TODO! AK: Raw fix.
	// Prevent collapse of the speed-scaled space on low speeds
	if (_control_mode.flag_control_setpoint_velocity && _pos_sp_triplet.current.abs_velocity_valid
			&& _pos_sp_triplet.current.abs_velocity <= 0.1f)
	{ // TODO! Reset all the time? Or just once per case?
		reset_pos_sp();
		reset_alt_sp();
	}
	else if (_pos_sp_triplet.current.valid) {
		/* in case of interrupted mission don't go to waypoint but stay at current position */
		_reset_pos_sp = true;
		_reset_alt_sp = true;

        /* use speeds defined in navigator if valid */
        if (_pos_sp_triplet.current.velocity_valid) {
           _vel_ff_sp_v(0) = _pos_sp_triplet.current.vx;
           _vel_ff_sp_v(1) = _pos_sp_triplet.current.vy;
           _vel_ff_sp_v(2) = _pos_sp_triplet.current.vz;
        }

		/* project setpoint to local frame */
		math::Vector<3> curr_sp;
		map_projection_project(&_ref_pos,
				       _pos_sp_triplet.current.lat, _pos_sp_triplet.current.lon,
				       &curr_sp.data[0], &curr_sp.data[1]);
		curr_sp(2) = -(_pos_sp_triplet.current.alt - _ref_alt);

		/* scaled space: 1 == position error resulting max allowed speed, L1 = 1 in this space */
		math::Vector<3> scale;
		if (_control_mode.flag_control_setpoint_velocity && _pos_sp_triplet.current.abs_velocity_valid) {
			scale(0) = _params.pos_p(0) / _pos_sp_triplet.current.abs_velocity;
			scale(1) = _params.pos_p(1) / _pos_sp_triplet.current.abs_velocity;
			scale(2) = _params.pos_p(2) / _params.vel_max(2);
		}
		else {
			scale = _params.pos_p.edivide(_params.vel_max);	// TODO add mult param here
		}

		/* convert current setpoint to scaled space */
		math::Vector<3> curr_sp_s = curr_sp.emult(scale);

		/* by default use current setpoint as is */
		math::Vector<3> pos_sp_s = curr_sp_s;

		if (_pos_sp_triplet.current.type == SETPOINT_TYPE_POSITION && _pos_sp_triplet.previous.valid) {
			/* follow "previous - current" line */
			math::Vector<3> prev_sp;
			map_projection_project(&_ref_pos,
						   _pos_sp_triplet.previous.lat, _pos_sp_triplet.previous.lon,
						   &prev_sp.data[0], &prev_sp.data[1]);
			prev_sp(2) = -(_pos_sp_triplet.previous.alt - _ref_alt);

			if ((curr_sp - prev_sp).length() > MIN_DIST) {

				/* find X - cross point of L1 sphere and trajectory */
				math::Vector<3> pos_s = _pos.emult(scale);
				math::Vector<3> prev_sp_s = prev_sp.emult(scale);
				math::Vector<3> prev_curr_s = curr_sp_s - prev_sp_s;
				math::Vector<3> curr_pos_s = pos_s - curr_sp_s;
				float curr_pos_s_len = curr_pos_s.length();
				if (curr_pos_s_len < 1.0f) {
					/* copter is closer to waypoint than L1 radius */
					/* check next waypoint and use it to avoid slowing down when passing via waypoint */
					if (_pos_sp_triplet.next.valid) {
						math::Vector<3> next_sp;
						map_projection_project(&_ref_pos,
									   _pos_sp_triplet.next.lat, _pos_sp_triplet.next.lon,
									   &next_sp.data[0], &next_sp.data[1]);
						next_sp(2) = -(_pos_sp_triplet.next.alt - _ref_alt);

						if ((next_sp - curr_sp).length() > MIN_DIST) {
							math::Vector<3> next_sp_s = next_sp.emult(scale);

							/* calculate angle prev - curr - next */
							math::Vector<3> curr_next_s = next_sp_s - curr_sp_s;
							math::Vector<3> prev_curr_s_norm = prev_curr_s.normalized();

							/* cos(a) * curr_next, a = angle between current and next trajectory segments */
							float cos_a_curr_next = prev_curr_s_norm * curr_next_s;

							/* cos(b), b = angle pos - curr_sp - prev_sp */
							float cos_b = -curr_pos_s * prev_curr_s_norm / curr_pos_s_len;

							if (cos_a_curr_next > 0.0f && cos_b > 0.0f) {
								float curr_next_s_len = curr_next_s.length();
								/* if curr - next distance is larger than L1 radius, limit it */
								if (curr_next_s_len > 1.0f) {
									cos_a_curr_next /= curr_next_s_len;
								}

								/* feed forward position setpoint offset */
								math::Vector<3> pos_ff = prev_curr_s_norm *
										cos_a_curr_next * cos_b * cos_b * (1.0f - curr_pos_s_len) *
										(1.0f - expf(-curr_pos_s_len * curr_pos_s_len * 20.0f));
								pos_sp_s += pos_ff;
							}
						}
					}

				} else {
					bool near = cross_sphere_line(pos_s, 1.0f, prev_sp_s, curr_sp_s, pos_sp_s);
					if (near) {
						/* L1 sphere crosses trajectory */

					} else {
						/* copter is too far from trajectory */
						/* if copter is behind prev waypoint, go directly to prev waypoint */
						if ((pos_sp_s - prev_sp_s) * prev_curr_s < 0.0f) {
							pos_sp_s = prev_sp_s;
						}

						/* if copter is in front of curr waypoint, go directly to curr waypoint */
						if ((pos_sp_s - curr_sp_s) * prev_curr_s > 0.0f) {
							pos_sp_s = curr_sp_s;
						}

						pos_sp_s = pos_s + (pos_sp_s - pos_s).normalized();
					}
				}
			}
		}

		/* move setpoint not faster than max allowed speed */
		math::Vector<3> pos_sp_old_s = _pos_sp.emult(scale);

		/* difference between current and desired position setpoints, 1 = max speed */
		math::Vector<3> d_pos_m = (pos_sp_s - pos_sp_old_s).edivide(_params.pos_p);
		float d_pos_m_len = d_pos_m.length();
		if (d_pos_m_len > dt) {
			pos_sp_s = pos_sp_old_s + (d_pos_m / d_pos_m_len * dt).emult(_params.pos_p);
		}

		/* scale result back to normal space */
		_pos_sp = pos_sp_s.edivide(scale);


		/* update yaw setpoint if needed */
		if (isfinite(_pos_sp_triplet.current.yaw)) {
			_att_sp.yaw_body = _pos_sp_triplet.current.yaw;
		}
	}
	else {
		// Reset setpoint? Or is it ok? Reset only once?
	}
}

void
MulticopterPositionControl::update_target_pos()
{
    // If alt is not used target altitude is contsant
    if (!_params.follow_use_alt){
        _tpos(2) = -(_alt_start - _ref_alt + _params.follow_talt_offs);
        _tvel(2) = 0.0f;
    }

	if (_ref_timestamp != 0) {
		/* check if target position updated */
		if (_target_pos.timestamp != _tpos_predictor.get_time_recv_last()) {
			if (!_target_alt_start_valid && _control_mode.flag_armed) {
				/* initialize target start altitude in flight if target was not available on arming */
				_target_alt_start = _target_pos.alt;
				_target_alt_start_valid = true;
			}

			/* project target position to local frame */
			math::Vector<3> tpos;
			map_projection_project(&_ref_pos, _target_pos.lat, _target_pos.lon, &tpos.data[0], &tpos.data[1]);

			math::Vector<3> tvel_current;
			tvel_current(0) = _target_pos.vel_n;
			tvel_current(1) = _target_pos.vel_e;

			if (_params.follow_use_alt) {
				/* use real target altitude */
				tpos(2) = -(_target_pos.alt - _target_alt_start + _alt_start - _ref_alt + _params.follow_talt_offs);
				tvel_current(2) = _target_pos.vel_d;

			} else {
				/* assume that target is always on start altitude */
				tpos(2) = -(_alt_start - _ref_alt + _params.follow_talt_offs);
				tvel_current(2) = 0.0f;
			}

			/* low pass filter for target velocity */
			tvel_current(0) = _tvel_lpf_x.apply(_target_pos.timestamp, tvel_current(0));
			tvel_current(1) = _tvel_lpf_y.apply(_target_pos.timestamp, tvel_current(1));
			tvel_current(2) = _tvel_lpf_z.apply(_target_pos.timestamp, tvel_current(2));

			/* NaN protection */
			if (isfinite(tvel_current(0)) && isfinite(tvel_current(1)) && isfinite(tvel_current(2))) {
				_tvel = tvel_current;

			} else {
				/* NaN on output, use previous value if possible and reset LPF */
				if (!(isfinite(_tvel(0)) && isfinite(_tvel(1)) && isfinite(_tvel(2)))) {

					_tvel.zero();
				}

				_tvel_lpf_x.reset(_target_pos.timestamp, _tvel(0));
				_tvel_lpf_y.reset(_target_pos.timestamp, _tvel(1));
				_tvel_lpf_z.reset(_target_pos.timestamp, _tvel(2));
			}

			/* update target position predictor */
			_tpos_predictor.update(_target_pos.timestamp, _target_pos.remote_timestamp, tpos.data, _tvel.data);
		}

		/* target position prediction */
		if (_tpos_predictor.get_time_recv_last() != 0 && hrt_absolute_time() < _tpos_predictor.get_time_recv_last() + 1000000) {
			_tpos_predictor.predict_position(_local_pos.timestamp, _tpos.data);
		}
	}
}

void
MulticopterPositionControl::control_follow(float dt)
{

    if (!_mode_follow){
        _target_alt_on_follow_start = _tpos(2);
        _drone_alt_on_follow_start = _pos(2);
    }

	/* follow target, change offset from target instead of moving setpoint directly */
	reset_follow_offset();


	/* new value for _follow_offset vector */
	math::Vector<3> follow_offset_new(_follow_offset);

	if (_control_mode.flag_control_manual_enabled) {
		/* move follow offset using polar coordinates */
		_sp_move_rate(0) = _manual.x;
		_sp_move_rate(1) = _manual.y;
		_sp_move_rate(2) = -scale_control(_manual.z - 0.5f, 0.5f, alt_ctl_dz);

		/* limit setpoint move rate */
		float sp_move_norm = _sp_move_rate.length();

		if (sp_move_norm > 1.0f) {
			_sp_move_rate /= sp_move_norm;
		}

		/* _sp_move_rate scaled to 0..1, scale it to max speed */
		_sp_move_rate = _sp_move_rate.emult(_params.vel_max);
	}
	else {
		_sp_move_rate.zero();
	}
	math::Vector<2> follow_offset_xy(_follow_offset(0), _follow_offset(1));
	math::Vector<2> sp_move_rate_xy(_sp_move_rate(0), _sp_move_rate(1));
	float follow_offset_xy_len = follow_offset_xy.length();

	if (sp_move_rate_xy.length_squared() > 0.0f) {
		if (_control_mode.flag_control_point_to_target && follow_offset_xy_len > _params.yaw_dead_zone_r) {
			/* calculate change rate in polar coordinates phi, d */
			float rate_phi = -sp_move_rate_xy(1) / follow_offset_xy_len;
			float rate_d = -sp_move_rate_xy(0);

			/* current direction of offset vector */
			float phi = atan2f(_follow_offset(1), _follow_offset(0));

			/* change length of horizontal component of _follow_offset vector with rate_d */
			follow_offset_new(0) += rate_d * cosf(phi) * dt;
			follow_offset_new(1) += rate_d * sinf(phi) * dt;

			/* rotate _follow_offset around vertical axis with rate_phi */
			math::Matrix<3, 3> R_phi;
			R_phi.from_euler(0.0f, 0.0f, rate_phi * dt);
			follow_offset_new = R_phi * follow_offset_new;

			/* update horizontal components of _sp_move_rate */
			_sp_move_rate(0) = rate_d * cosf(phi) - rate_phi * sinf(phi) * follow_offset_xy_len;
			_sp_move_rate(1) = rate_d * sinf(phi) + rate_phi * cosf(phi) * follow_offset_xy_len;

		} else {
			/* 'point_to_target' disabled or copter is too close to target */
			math::Matrix<3, 3> R_yaw_sp;
			R_yaw_sp.from_euler(0.0f, 0.0f, _att_sp.yaw_body);
			_sp_move_rate = R_yaw_sp * _sp_move_rate;
			follow_offset_new += _sp_move_rate * dt;
		}
	}

	/* change altitude */
	follow_offset_new(2) += _sp_move_rate(2) * dt;

	_follow_offset = follow_offset_new;

    if (_control_mode.flag_control_offset_follow) {

        if (_params.follow_rpt_alt) {

            float target_alt_delta = _tpos(2) - _target_alt_on_follow_start;

            _pos_sp(0) = _tpos(0) + _follow_offset(0);
            _pos_sp(1) = _tpos(1) + _follow_offset(1);
            _pos_sp(2) = _drone_alt_on_follow_start + _follow_offset(2) + target_alt_delta;

        } else {

            _pos_sp(0) = _tpos(0) + _follow_offset(0);
            _pos_sp(1) = _tpos(1) + _follow_offset(1);
            _pos_sp(2) = _drone_alt_on_follow_start + _follow_offset(2);
 
        }

    } else {
        // Applies to manual follow
        _pos_sp = _tpos + _follow_offset;
    
    }

	/* feed forward manual setpoint move rate with weight vel_ff */
	_vel_ff_sp_mv_r = _sp_move_rate.emult(_params.vel_ff);

	/* add target velocity to setpoint move rate */
	_sp_move_rate += _tvel;

    /* feed forward target velocity with weight follow_vel_ff */
    _vel_ff_t(0) = _tvel(0) * _params.follow_vel_ff_xy;
    _vel_ff_t(1) = _tvel(1) * _params.follow_vel_ff_xy;
    _vel_ff_t(2) = _tvel(2) * _params.follow_vel_ff_z;

	/* update position setpoint and feed-forward velocity if not repeating target altitude */
	if (!_params.follow_rpt_alt) {

        _vel_ff_t(2) = 0.0f;
        _sp_move_rate(2) -= _tvel(2);

	}
}

float
MulticopterPositionControl::follow_grad_ff(float distance){

    float grad_start = _params.follow_grad_ff_start;
    float grad_end = _params.follow_grad_ff_end;

    float grad_dst = distance - grad_start;
    float grad_interval = grad_end - grad_start;

    if (grad_dst < 0.0f) grad_dst = 0.0f;
    if (grad_dst > grad_interval) grad_dst = grad_interval;

    if (grad_interval <= 0.0f) {

        if (distance > grad_end) return 0.0f;
        else return 1.0f;

    }

    return 1.0f - (grad_dst / grad_interval);
}

void MulticopterPositionControl::set_camera_yaw(){
    /* Adjust yaw by user offset from leash */
    _att_sp.yaw_body += _params.cam_yaw_step * _cam_offset.yaw_offset;
}

static float last_pitch = 0.0f;
static float pitch_change_speed = 0.005f;
void MulticopterPositionControl::set_camera_pitch(float pitch){
    /* Adjust pitch by user offset from leash */
	float pitch_delta = pitch - last_pitch + _params.cam_pitch_step * _cam_offset.pitch_offset;
	float pitch_delta_20th = pitch_delta/12.f;
	if (fabsf(pitch_delta) > pitch_change_speed){
		if (pitch_delta > 0.0f) {
			last_pitch += (pitch_delta_20th > pitch_change_speed ? pitch_change_speed : pitch_delta_20th);
		}
		else {
			last_pitch -= (-pitch_delta_20th > pitch_change_speed ? pitch_change_speed : -pitch_delta_20th);
		}
	}
	else {
		last_pitch = pitch;
	}
	if (_params.pitch_lpf_cut < -FLT_EPSILON) {
		last_pitch = _pitchLPF.apply(hrt_absolute_time(), pitch);
	}
	else if (_params.pitch_lpf_cut > FLT_EPSILON) {
		last_pitch = _pitchLPF.apply(hrt_absolute_time(), last_pitch);
	}
	_cam_control.control[1] = last_pitch;
}

void
MulticopterPositionControl::point_to_target()
{

	/* change yaw to keep direction to target */
	/* calculate current offset (not offset setpoint) */
	math::Vector<3> current_offset = _pos - _tpos;
	math::Vector<2> current_offset_xy(current_offset(0), current_offset(1));
	/* don't try to rotate near singularity */
	float current_offset_xy_len = current_offset_xy.length();

    if (current_offset_xy_len >= _params.yaw_gradient_zone_r)
        _att_sp.yawrate_limit = 0.0f; // no limit

	if (current_offset_xy_len > _params.yaw_dead_zone_r) {
		/* calculate yaw setpoint from current positions and control offset with yaw stick */
		_att_sp.yaw_body = _wrap_pi(atan2f(-current_offset_xy(1), -current_offset_xy(0)) + _manual.r * _params.follow_yaw_off_max);

		/* feed forward attitude rates */
		math::Vector<2> offs_vel_xy(_vel(0) - _tvel(0), _vel(1) - _tvel(1));
		_att_rates_ff(2) = (current_offset_xy % offs_vel_xy) / current_offset_xy_len / current_offset_xy_len;

        if (current_offset_xy_len < _params.yaw_gradient_zone_r && _params.yaw_dead_zone_r + 1e-6f < _params.yaw_gradient_zone_r) {

            float fraction = (current_offset_xy_len - _params.yaw_dead_zone_r) / (_params.yaw_gradient_zone_r - _params.yaw_dead_zone_r);
            _att_sp.yawrate_limit = _params.yaw_rate_max * fraction;

        }


	}

	/* control camera pitch in global frame (for BL camera gimbal) */
	set_camera_pitch(atan2f(current_offset(2), current_offset_xy_len) / _params.cam_pitch_max + _manual.aux2);
}

void
MulticopterPositionControl::task_main()
{
	warnx("started");

	_mavlink_fd = open(MAVLINK_LOG_DEVICE, 0);
	mavlink_log_info(_mavlink_fd, "[mpc] started");

	/*
	 * do subscriptions
	 */
    _cam_offset_sub = orb_subscribe(ORB_ID(camera_user_offsets));
	_att_sub = orb_subscribe(ORB_ID(vehicle_attitude));
	_att_sp_sub = orb_subscribe(ORB_ID(vehicle_attitude_setpoint));
	_control_mode_sub = orb_subscribe(ORB_ID(vehicle_control_mode));
	_params_sub = orb_subscribe(ORB_ID(parameter_update));
	_manual_sub = orb_subscribe(ORB_ID(manual_control_setpoint));
	_arming_sub = orb_subscribe(ORB_ID(actuator_armed));
	_local_pos_sub = orb_subscribe(ORB_ID(vehicle_local_position));
	_pos_sp_triplet_sub = orb_subscribe(ORB_ID(position_setpoint_triplet));
	_local_pos_sp_sub = orb_subscribe(ORB_ID(vehicle_local_position_setpoint));
	_global_vel_sp_sub = orb_subscribe(ORB_ID(vehicle_global_velocity_setpoint));
	_target_pos_sub = orb_subscribe(ORB_ID(target_global_position));
    _vehicle_status_sub = orb_subscribe(ORB_ID(vehicle_status));
    _pos_restrict_sub = orb_subscribe(ORB_ID(position_restriction));
    _follow_offset_sub = orb_subscribe(ORB_ID(follow_offset));
    _home_pos_sub = orb_subscribe(ORB_ID(home_position));

	parameters_update(true);

	/* initialize values of critical structs until first regular update */
	_arming.armed = false;

	/* get an initial update for all sensor and status data */
	poll_subscriptions();

	bool reset_int_z = true;
	bool reset_int_z_manual = false;
	bool reset_int_xy = true;
	bool was_armed = false;

	hrt_abstime t_prev = 0;
	hrt_abstime takeoff_start_time = 0;

	math::Vector<3> thrust_int;
	thrust_int.zero();
	math::Matrix<3, 3> R;
	R.identity();

	/* wakeup source */
	struct pollfd fds[1];

	fds[0].fd = _local_pos_sub;
	fds[0].events = POLLIN;

	while (!_task_should_exit) {
		_ground_position_invalid = false;
		_ground_setpoint_corrected = false;
		_ground_position_available_drop = 0;

		/* wait for up to 500ms for data */
		int pret = poll(&fds[0], (sizeof(fds) / sizeof(fds[0])), 500);

		/* timed out - periodic check for _task_should_exit */
		if (pret == 0) {
			continue;
		}

		/* this is undesirable but not much we can do */
		if (pret < 0) {
			warn("poll error %d, %d", pret, errno);
			continue;
		}

		perf_begin(_loop_perf);

		poll_subscriptions();
		parameters_update(false);

		hrt_abstime t = hrt_absolute_time();
		float dt = t_prev != 0 ? (t - t_prev) * 0.000001f : 0.0f;
		t_prev = t;

		if (_control_mode.flag_armed && !was_armed) {
			/* reset setpoints and integrals on arming */
			_reset_pos_sp = true;
			_reset_alt_sp = true;
			_reset_follow_offset = true;
			reset_int_z = true;
			reset_int_xy = true;

			/* init start altitude */
			if (_local_pos.timestamp < hrt_absolute_time() + 100000 && _local_pos.ref_timestamp > 0) {
				_alt_start = _local_pos.ref_alt - _local_pos.z;

			} else {
				_alt_start = 0.0;
			}

			/* init target altitude offset */
			if (_target_pos.timestamp < hrt_absolute_time() + TARGET_POSITION_TIMEOUT) {
				_target_alt_start = _target_pos.alt;
				_target_alt_start_valid = true;

			} else {
				_target_alt_start_valid = false;
			}
		}
		else if (!_control_mode.flag_armed) {
			was_armed = false;
			_mode_auto = false;
			takeoff_start_time = 0;
		}

		was_armed = _control_mode.flag_armed;

		update_ref();

		/* manual camera pitch control, overridden later if needed, 0 on init
		 * Check for manual follow state prevents pitch reset on target signal loss */
		if (_control_mode.flag_control_manual_enabled && !_vstatus.rc_signal_lost
				&& _vstatus.nav_state != NAVIGATION_STATE_FOLLOW) {
			_cam_control.control[1] = _manual.aux2;
		}

		if (_control_mode.flag_control_altitude_enabled ||
		    _control_mode.flag_control_position_enabled ||
		    _control_mode.flag_control_climb_rate_enabled ||
		    _control_mode.flag_control_velocity_enabled) {

			_pos(0) = _local_pos.x;
			_pos(1) = _local_pos.y;
			_pos(2) = _local_pos.z;

			_vel(0) = _local_pos.vx;
			_vel(1) = _local_pos.vy;
			_vel(2) = _local_pos.vz;

			update_target_pos();

			_vel_ff_t.zero();
			_vel_ff_sp_mv_r.zero();
			_vel_ff_sp_v.zero();

			_sp_move_rate.zero();
			_att_rates_ff.zero();

            if (_control_mode.flag_control_offset_follow) {

                _reset_follow_offset = false;
                
                _follow_offset(0) = _orb_follow_offset.x;
                _follow_offset(1) = _orb_follow_offset.y;
                _follow_offset(2) = _orb_follow_offset.z;

            }

            bool  control_follow_run = false;

			/* select control source */
			if (_control_mode.flag_control_manual_enabled) {
				if (_control_mode.flag_control_follow_target) {
					/* follow */
                    control_follow_run = true;
					control_follow(dt);

				} else {
					/* manual control */
					control_manual(dt);
				}
				_mode_auto = false;

			} else if (_control_mode.flag_control_offboard_enabled) {
				/* offboard control */
				control_offboard(dt);
				_mode_auto = false;

			} else if (_control_mode.flag_control_auto_enabled) {
				/* AUTO modes*/
                if (_pos_sp_triplet.current.type != SETPOINT_TYPE_VELOCITY) { // control_auto_vel is used where vel_sp is set

                    if (_control_mode.flag_control_follow_target) {
                        if (_control_mode.flag_control_follow_restricted) {
							//Cable park mode
							control_cablepark();
						}
						else {
                        	// For auto ABS Follow
                            control_follow_run = true;
                          	control_follow(dt);
                    	}
                    } else {
                        /* AUTO */
                        control_auto(dt);
                    }
                }
			}

            if (control_follow_run) {
                _mode_follow = true;
            } else {
                _mode_follow = false;
            }

			if (_vstatus.nav_state == NAVIGATION_STATE_ATTITUDE_HOLD && !_mode_attitude_hold) {
				_mode_attitude_hold = true;
				_mode_auto = false;

				// Choose highest of the altitudes, local altitude comparison is inverted
				// TODO! [AK] Check if _pos_sp was valid before we've entered the mode
				if (_home_valid && (_home_pos.z - _params.rtl_alt < _pos_sp(2))) {
					_pos_sp(2) = _home_pos.z - _params.rtl_alt;
				}
				if (_pos(2) < _pos_sp(2)) {
					_pos_sp(2) = _pos(2);
				}

				// Advice the next mode to reset the setpoints on activation
				_reset_pos_sp = true;
				_reset_alt_sp = true;
			}
			else if (_mode_attitude_hold && _vstatus.nav_state != NAVIGATION_STATE_ATTITUDE_HOLD) {
				_mode_attitude_hold = false;
			}

			if (_control_mode.flag_control_point_to_target) {
				point_to_target();
			}

			/* reset follow offset after non-follow modes */
			if (!(_control_mode.flag_control_follow_target)) {
				_reset_follow_offset = true;
			}

            set_camera_yaw();

			/* fill local position setpoint */
			_local_pos_sp.timestamp = hrt_absolute_time();
			_local_pos_sp.x = _pos_sp(0);
			_local_pos_sp.y = _pos_sp(1);
			_local_pos_sp.z = _pos_sp(2);
			_local_pos_sp.yaw = _att_sp.yaw_body;

			/* publish local position setpoint */
			if (_local_pos_sp_pub > 0) {
				orb_publish(ORB_ID(vehicle_local_position_setpoint), _local_pos_sp_pub, &_local_pos_sp);

			} else {
				_local_pos_sp_pub = orb_advertise(ORB_ID(vehicle_local_position_setpoint), &_local_pos_sp);
			}

			// Paranoid NaN protection
			for (int i = 0; i < 3; ++i) {
				if (!isfinite(_pos_sp(i))) {
					// Reset to current local position. If it still contains NaNs it has to be fiexd in inav
					_pos_sp(i) = _pos(i);
				}
			}

			if (!_control_mode.flag_control_manual_enabled && _pos_sp_triplet.current.valid && _pos_sp_triplet.current.type == SETPOINT_TYPE_IDLE) {
				/* idle state, don't run controller and set zero thrust */
				R.identity();
				memcpy(&_att_sp.R_body[0][0], R.data, sizeof(_att_sp.R_body));
				_att_sp.R_valid = true;

				_att_sp.roll_body = 0.0f;
				_att_sp.pitch_body = 0.0f;
				_att_sp.yaw_body = _att.yaw;
				_att_sp.thrust = 0.0f;
				_att_sp.rollrate_ff = 0.0f;
				_att_sp.pitchrate_ff = 0.0f;
				_att_sp.yawrate_ff = 0.0f;

				_att_sp.timestamp = hrt_absolute_time();

				/* publish attitude setpoint */
				if (_att_sp_pub > 0) {
					orb_publish(ORB_ID(vehicle_attitude_setpoint), _att_sp_pub, &_att_sp);

				} else {
					_att_sp_pub = orb_advertise(ORB_ID(vehicle_attitude_setpoint), &_att_sp);
				}

				takeoff_start_time = 0; // Reset takeoff timer in case we were disarmed before we've finished

			} else {

				/* run position & altitude controllers, calculate velocity setpoint */

                if (_pos_sp_triplet.current.type == SETPOINT_TYPE_VELOCITY && _control_mode.flag_control_auto_enabled){

                    control_auto_vel(dt); // calculate vel_sp

                } else {

                    // use gradual target speed feed forward based on drone to target distance
                    if (_params.follow_grad_ff) {

                        math::Vector<3> trgt_drone_delta_pos = _tpos - _pos;

                        _vel_ff_t *= follow_grad_ff( trgt_drone_delta_pos.length() );
                    }

                    _vel_ff = _vel_ff_t + _vel_ff_sp_mv_r + _vel_ff_sp_v;

                    math::Vector<3> pos_err = _pos_sp - _pos;

                    _vel_sp = pos_err.emult(_params.pos_p) + _vel_ff;

                    math::Vector<2> vel_sp_xy(_vel_sp(0), _vel_sp(1));
                    float vel_sp_z = _vel_sp(2);

                    if (vel_sp_xy.length() > _params.xy_vel_max) {
                        vel_sp_xy.normalize();
                        vel_sp_xy *= _params.xy_vel_max;
                    }

                    if (vel_sp_z > _params.z_vel_max_down) {
                        vel_sp_z = _params.z_vel_max_down;
                    }

                    if (vel_sp_z < -_params.z_vel_max_up){
                        vel_sp_z = -_params.z_vel_max_up;
                    }

                    _vel_sp(0) = vel_sp_xy(0);
                    _vel_sp(1) = vel_sp_xy(1);
                    _vel_sp(2) = vel_sp_z;

                    if (_control_mode.flag_control_follow_restricted) {
                        if (_valid_vel_correction)
                        {
                            // Limit speed if we are coming to first/last points in cable park mode
                            float cur_vel_module = fabsf(_vel_sp.length()) ;
                            float allowed_vel_mod = fabsf(_current_allowed_velocity);
                            if ( cur_vel_module > allowed_vel_mod ) {
                                _vel_sp *= allowed_vel_mod/cur_vel_module;
                            }
                        }
                        else  // If we are flying to points in cable park
                        {
                            for (int i=0;i<2;i++)
                            {
                                if ( fabsf(_vel_sp(i)) > _params.cbp_max_init_speed )
                                {
                                    if (_vel_sp(i) > 0.0f)
                                    {
                                        _vel_sp(i) = _params.cbp_max_init_speed;
                                    }
                                    else
                                    {
                                        _vel_sp(i) = - _params.cbp_max_init_speed;
                                    }
                                }
                            }
                        }
                    }

                }

                // reset setpoints to current position and nullify speed if setpoint is too far away
                if (!validate_setpoint_distance())
                {
                    _pos_sp(0) = _pos(0);
                    _pos_sp(1) = _pos(1);
                    _pos_sp(2) = _pos(2);
                    _vel_sp.zero();
                }

                // TODO! [AK] Consider applying distance sensor correction in other cases (ie ATTITUDE_HOLD)
                if ((_control_mode.flag_control_position_enabled || _control_mode.flag_control_follow_target)&&
                        (_vstatus.airdog_state == AIRD_STATE_IN_AIR)    ) {
                    /*
                     * Try to correct this altitude with sonar
                     * Only if we are flying or landing
                     */
                        ground_dist_correction();
                        if (_ground_setpoint_corrected) {
                            //correct altitude velocity

                            _vel_ff_sp_v(2) = 0.0f;
                            _vel_ff_sp_mv_r(2) = 0.0f;
                            _vel_ff_t(2) = 0.0f;

                            //and altitude move rate
                            _sp_move_rate(2)= 0.0f;

                            if (_control_mode.flag_control_follow_target && _control_mode.flag_control_manual_enabled) {
                                //stop moving offset in manual follow mode
                                _follow_offset(2) = _pos_sp(2) - _tpos(2);
                            }
                        }
                }
                else {
                    _ground_setpoint_corrected = false;
                }

				if (!_control_mode.flag_control_altitude_enabled) {
					_reset_alt_sp = true;
					_vel_sp(2) = 0.0f;
				}

				if (!_control_mode.flag_control_position_enabled) {
					_reset_pos_sp = true;
					_vel_sp(0) = 0.0f;
					_vel_sp(1) = 0.0f;
				}

				// TODO! AK: Check what values can be for uninitialized camera_pitch!
                // It makes sense to change yaw and pich trough setpoint when point_to_target is not used
                if (!_control_mode.flag_control_point_to_target && _pos_sp_triplet.current.valid && _pos_sp_triplet.current.camera_pitch_valid) {
                	//_cam_control.control[1] = _pos_sp_triplet.current.camera_pitch;
					set_camera_pitch(_pos_sp_triplet.current.camera_pitch);
                }

				/* use constant descend rate when landing, ignore altitude setpoint */
				if (!_control_mode.flag_control_manual_enabled
						&& _control_mode.flag_control_position_enabled
						&& _pos_sp_triplet.current.valid
						&& _pos_sp_triplet.current.type == SETPOINT_TYPE_LAND) {

                    math::Vector<2> xy_speed(_vel(0),_vel(1));

                    if (xy_speed.length() > _params.max_land_xy_speed) {

                        _vel_sp(2) = 0.0f;

                    } else  {

                        /* In case we have sonar correction - use it */
                        if(_params.land_correction_on) {
                            float speed_corretion =  landing_speed_correction();
                            _vel_sp(2) = _params.land_speed_min * speed_corretion;
                        }
                        else {
                            /* No range finder correction applied */
                            _vel_sp(2) = _params.regular_land_speed;
                        }
                        //_pos_sp(2) = _pos(2);

                    }

				}

				/* use constant ascend rate during take off */
				if (!_control_mode.flag_control_manual_enabled
						&& _control_mode.flag_control_position_enabled
						&& _pos_sp_triplet.current.valid
						&& _pos_sp_triplet.current.type == SETPOINT_TYPE_TAKEOFF) {
					if (_pos(2) - _pos_sp(2) > 0) {
						if (_vel_sp.data[2] < -_params.takeoff_speed){
							_vel_sp.data[2] = -_params.takeoff_speed;
						}
						if (takeoff_start_time == 0) {
							takeoff_start_time = t;
						}
						// Allow the motors to "warm up" at minimal thrust
						else if (t - takeoff_start_time < _params.takeoff_warmup_time) {
							_vel_sp.data[2] = 42; // random constant going _down_
						}
						// Ensure that we go for the desired velocity for some time, but increase velocity gradually
						else if (t - takeoff_start_time < _params.takeoff_warmup_time + _params.takeoff_gradient_time) {
							_vel_sp.data[2] = _vel_sp.data[2] * (t - takeoff_start_time - _params.takeoff_warmup_time) / _params.takeoff_gradient_time;
						}
					}
				}

                //Ground distance correction
                if (_params.sonar_correction_on) {
                    if (_ground_position_invalid) {
                            float drop = _pos(2) - _pos_sp(2);
                            if (drop >= 0) {
                                if (drop < _params.sonar_min_dist) {

                                    float coef = 1 - (_local_pos.dist_bottom/_params.sonar_min_dist);
                                    coef = pow(coef, _params.sonar_smooth_coef);

                                    float max_vel_z = - _params.vel_max(2) * coef;

                                    _vel_sp(2) = max_vel_z;
                                    _sp_move_rate(2)= 0.0f;
                                    //fprintf(stderr, "max_vel_z %.3f coef %.3f, drop %.3f\n"
                                    //        ,(double) max_vel_z, (double) coef, (double) drop);
                                } else {
                                    // Max throttle in case we are "uderground"
                                    // Just a safety limit
                                    _vel_sp(2) = - _params.vel_max(2);
                                    _sp_move_rate(2)= 0.0f;
                                }
                            }
                    }
                    else if (_ground_setpoint_corrected && (_vel(2) > _params.vel_max(2) || _vel_sp(2) > _params.vel_max(2))) {

                        _vel_sp(2) = - 2 * _params.vel_max(2);
                        _sp_move_rate(2)= 0.0f;
                    }
                    else if (_local_pos.dist_bottom_valid) {
                        if (_ground_position_available_drop > 0.0f && _vel_sp(2) > 0){
                        float range = _local_pos.dist_bottom_max - _params.sonar_min_dist;
                        // Used when we are above allowed limit
                        float max_vel_z = _params.vel_max(2) * (float)pow(_ground_position_available_drop/range, 2.0);

                        // If resulted max speed is higher than allowed by parameters - limit it with parameter defined
                        // TODO [Max]: There should be universal way for all range finders, not limited to Ultrasound sonar maximal distance
                        max_vel_z = max_vel_z > _params.vel_max(2) ? _params.vel_max(2) : max_vel_z;

                            //limit down speed
                            if (_vel_sp(2) > max_vel_z) {
                                _vel_sp(2) = max_vel_z;
                            }
                            else {
                            }
                            _sp_move_rate(2)= 0.0f;

                        }
                    }
                }

				_global_vel_sp.vx = _vel_sp(0);
				_global_vel_sp.vy = _vel_sp(1);
				_global_vel_sp.vz = _vel_sp(2);

				/* publish velocity setpoint */
				if (_global_vel_sp_pub > 0) {
					orb_publish(ORB_ID(vehicle_global_velocity_setpoint), _global_vel_sp_pub, &_global_vel_sp);

				} else {
					_global_vel_sp_pub = orb_advertise(ORB_ID(vehicle_global_velocity_setpoint), &_global_vel_sp);
				}

				// Paranoid NaN protection
				for (int i = 0; i < 3; ++i) {
					if (!isfinite(_vel_sp(i))) {
						// By default try to stop
						_vel_sp(i) = 0;
					}
				}

				if (_control_mode.flag_control_climb_rate_enabled || _control_mode.flag_control_velocity_enabled) {
					/* reset integrals if needed */
					if (_control_mode.flag_control_climb_rate_enabled) {
						if (reset_int_z) {
							reset_int_z = false;
							float i = _params.thr_min;

							if (reset_int_z_manual) {
								i = _manual.z;

								if (i < _params.thr_min) {
									i = _params.thr_min;

								} else if (i > _params.thr_max) {
									i = _params.thr_max;
								}
							}

							thrust_int(2) = -i;
						}

					} else {
						reset_int_z = true;
					}

					if (_control_mode.flag_control_velocity_enabled) {
						if (reset_int_xy) {
							reset_int_xy = false;
							thrust_int(0) = 0.0f;
							thrust_int(1) = 0.0f;
						}

					} else {
						reset_int_xy = true;
					}

					/* velocity error */
					math::Vector<3> vel_err = _vel_sp - _vel;

					/* derivative of velocity error, not includes setpoint acceleration */
					math::Vector<3> vel_err_d = (_sp_move_rate - _vel).emult(_params.pos_p) - (_vel - _vel_prev) / dt;
					_vel_prev = _vel;

					/* thrust vector in NED frame */
					math::Vector<3> thrust_sp = vel_err.emult(_params.vel_p) + vel_err_d.emult(_params.vel_d) + thrust_int;

					if (!_control_mode.flag_control_velocity_enabled) {
						thrust_sp(0) = 0.0f;
						thrust_sp(1) = 0.0f;
					}

					if (!_control_mode.flag_control_climb_rate_enabled) {
						thrust_sp(2) = 0.0f;
					}

					/* limit thrust vector and check for saturation */
					bool saturation_xy = false;
					bool saturation_z = false;

					/* limit min lift */
					float thr_min = _params.thr_min;

					if (!_control_mode.flag_control_velocity_enabled && thr_min < 0.0f) {
						/* don't allow downside thrust direction in manual attitude mode */
						thr_min = 0.0f;
					}

					float tilt_max = _params.tilt_max_air;

					/* adjust limits for landing mode */
					if (!_control_mode.flag_control_manual_enabled
							&& _control_mode.flag_control_position_enabled
							&& _pos_sp_triplet.current.valid
							&& _pos_sp_triplet.current.type == SETPOINT_TYPE_LAND) {
						/* limit max tilt and min lift when landing */
						tilt_max = _params.tilt_max_land;

						if (thr_min < 0.0f) {
							thr_min = 0.0f;
						}
					}

					/* adjust limits for takeoff mode */
					if (!_control_mode.flag_control_manual_enabled
							&& _control_mode.flag_control_position_enabled
							&& _pos_sp_triplet.current.valid
							&& _pos_sp_triplet.current.type == SETPOINT_TYPE_TAKEOFF) {
						/* limit max tilt and min lift when landing */
						tilt_max = _params.tilt_max_land;

						if (thr_min < 0.0f) {
							thr_min = 0.0f;
						}
					}

					/* limit min lift */
					if (-thrust_sp(2) < thr_min) {
						thrust_sp(2) = -thr_min;
						saturation_z = true;
					}

					if (_control_mode.flag_control_velocity_enabled) {
						/* limit max tilt */
						if (thr_min >= 0.0f && tilt_max < M_PI_F / 2 - 0.05f) {
							/* absolute horizontal thrust */
							float thrust_sp_xy_len = math::Vector<2>(thrust_sp(0), thrust_sp(1)).length();

							if (thrust_sp_xy_len > 0.01f) {
								/* max horizontal thrust for given vertical thrust*/
								float thrust_xy_max = -thrust_sp(2) * tanf(tilt_max);

								if (thrust_sp_xy_len > thrust_xy_max) {
									float k = thrust_xy_max / thrust_sp_xy_len;
									thrust_sp(0) *= k;
									thrust_sp(1) *= k;
									saturation_xy = true;
								}
							}
						}

					} else {
						/* thrust compensation for altitude only control mode */
						float att_comp;

						if (_att.R[2][2] > TILT_COS_MAX) {
							att_comp = 1.0f / _att.R[2][2];

						} else if (_att.R[2][2] > 0.0f) {
							att_comp = ((1.0f / TILT_COS_MAX - 1.0f) / TILT_COS_MAX) * _att.R[2][2] + 1.0f;
							saturation_z = true;

						} else {
							att_comp = 1.0f;
							saturation_z = true;
						}

						thrust_sp(2) *= att_comp;
					}

					/* limit max thrust */
					float thrust_abs = thrust_sp.length();

					if (thrust_abs > _params.thr_max) {
						if (thrust_sp(2) < 0.0f) {
							if (-thrust_sp(2) > _params.thr_max) {
								/* thrust Z component is too large, limit it */
								thrust_sp(0) = 0.0f;
								thrust_sp(1) = 0.0f;
								thrust_sp(2) = -_params.thr_max;
								saturation_xy = true;
								saturation_z = true;

							} else {
								/* preserve thrust Z component and lower XY, keeping altitude is more important than position */
								float thrust_xy_abs = math::Vector<2>(thrust_sp(0), thrust_sp(1)).length();
								if (thrust_xy_abs > 0.0001f) {
									float thrust_xy_max = sqrtf(_params.thr_max * _params.thr_max - thrust_sp(2) * thrust_sp(2));
									float k = thrust_xy_max / thrust_xy_abs;
									thrust_sp(0) *= k;
									thrust_sp(1) *= k;
									saturation_xy = true;
								}
								// XY component is minimal and Z is approximately max thrust
								else {
									thrust_sp(0) = 0.0f;
									thrust_sp(1) = 0.0f;
								}
							}

						} else {
							/* Z component is negative, going down, simply limit thrust vector */
							float k = _params.thr_max / thrust_abs;
							thrust_sp *= k;
							saturation_xy = true;
							saturation_z = true;
						}

						thrust_abs = _params.thr_max;
					}

					/* update integrals */
					if (_control_mode.flag_control_velocity_enabled && !saturation_xy) {
						thrust_int(0) += vel_err(0) * _params.vel_i(0) * dt;
						thrust_int(1) += vel_err(1) * _params.vel_i(1) * dt;
					}

					if (_control_mode.flag_control_climb_rate_enabled && !saturation_z) {
						thrust_int(2) += vel_err(2) * _params.vel_i(2) * dt;

						/* protection against flipping on ground when landing */
						if (thrust_int(2) > 0.0f) {
							thrust_int(2) = 0.0f;
						}
					}

					/* calculate attitude setpoint from thrust vector */
					if (_control_mode.flag_control_velocity_enabled) {
						/* desired body_z axis = -normalize(thrust_vector) */
						math::Vector<3> body_x;
						math::Vector<3> body_y;
						math::Vector<3> body_z;

						if (thrust_abs > SIGMA) {
							body_z = -thrust_sp / thrust_abs;

						} else {
							/* no thrust, set Z axis to safe value */
							body_z.zero();
							body_z(2) = 1.0f;
						}

						/* vector of desired yaw direction in XY plane, rotated by PI/2 */
						math::Vector<3> y_C(-sinf(_att_sp.yaw_body), cosf(_att_sp.yaw_body), 0.0f);

						if (fabsf(body_z(2)) > SIGMA) {
							/* desired body_x axis, orthogonal to body_z */
							body_x = y_C % body_z;

							/* keep nose to front while inverted upside down */
							if (body_z(2) < 0.0f) {
								body_x = -body_x;
							}

							body_x.normalize();

						} else {
							/* desired thrust is in XY plane, set X downside to construct correct matrix,
							 * but yaw component will not be used actually */
							body_x.zero();
							body_x(2) = 1.0f;
						}

						/* desired body_y axis */
						body_y = body_z % body_x;

						/* fill rotation matrix */
						for (int i = 0; i < 3; i++) {
							R(i, 0) = body_x(i);
							R(i, 1) = body_y(i);
							R(i, 2) = body_z(i);
						}

						/* copy rotation matrix to attitude setpoint topic */
						memcpy(&_att_sp.R_body[0][0], R.data, sizeof(_att_sp.R_body));
						_att_sp.R_valid = true;

						/* calculate euler angles, for logging only, must not be used for control */
						math::Vector<3> euler = R.to_euler();
						_att_sp.roll_body = euler(0);
						_att_sp.pitch_body = euler(1);
						/* yaw already used to construct rot matrix, but actual rotation matrix can have different yaw near singularity */

					} else if (!_control_mode.flag_control_manual_enabled && _control_mode.flag_control_attitude_enabled) {
						/* autonomous altitude control without position control (failsafe landing, gps loss failsafe),
						 * force level attitude, don't change yaw */
						R.from_euler(0.0f, 0.0f, _att_sp.yaw_body);

						/* copy rotation matrix to attitude setpoint topic */
						memcpy(&_att_sp.R_body[0][0], R.data, sizeof(_att_sp.R_body));
						_att_sp.R_valid = true;

						_att_sp.roll_body = 0.0f;
						_att_sp.pitch_body = 0.0f;
					}

					/* convert attitude rates from NED to body frame */
					_att_rates_ff = R.transposed() * _att_rates_ff;

					_att_sp.rollrate_ff = _att_rates_ff(0);
					_att_sp.pitchrate_ff = _att_rates_ff(1);
					_att_sp.yawrate_ff = _att_rates_ff(2);
					_att_sp.thrust = thrust_abs;

					_att_sp.timestamp = hrt_absolute_time();

					/* publish attitude setpoint */
					if (_att_sp_pub > 0) {
						orb_publish(ORB_ID(vehicle_attitude_setpoint), _att_sp_pub, &_att_sp);

					} else {
						_att_sp_pub = orb_advertise(ORB_ID(vehicle_attitude_setpoint), &_att_sp);
					}

				} else {
					reset_int_z = true;
				}
			}

		} else {
			/* position controller disabled, reset setpoints */
			_reset_alt_sp = true;
			_reset_pos_sp = true;
			_reset_follow_offset = true;
			reset_int_z = true;
			reset_int_xy = true;
		}

		/* reset altitude controller integral (hovering throttle) to manual throttle after manual throttle control */
		reset_int_z_manual = _control_mode.flag_armed && _control_mode.flag_control_manual_enabled && !_control_mode.flag_control_climb_rate_enabled;

		/* publish camera control in all modes */
		if (_cam_control_pub < 0) {
			_cam_control_pub = orb_advertise(ORB_ID(actuator_controls_2), &_cam_control);

		} else {
			orb_publish(ORB_ID(actuator_controls_2), _cam_control_pub, &_cam_control);
		}
		perf_end(_loop_perf);
	}

	warnx("stopped");
	mavlink_log_info(_mavlink_fd, "[mpc] stopped");

	_control_task = -1;
	_exit(0);
}

bool
MulticopterPositionControl::validate_setpoint_distance()
{
    bool result = false;
    math::Vector<3> distance = _pos - _pos_sp;

    if(_vstatus.nav_state == NAVIGATION_STATE_RTL)
    {
        if (distance.length() < _params.max_sp_distance_max)
        {
            result = true;
        }
    }
    else if (distance.length() < _params.max_sp_distance_regular)
    {
        result = true;
    }

    return result;
}

int
MulticopterPositionControl::start()
{
	ASSERT(_control_task == -1);

	/* start the task */
	_control_task = task_spawn_cmd("mc_pos_control",
				       SCHED_DEFAULT,
				       SCHED_PRIORITY_MAX - 5,
				       2500,
				       (main_t)&MulticopterPositionControl::task_main_trampoline,
				       nullptr);

	if (_control_task < 0) {
		warn("task start failed");
		return -errno;
	}

	return OK;
}

/* @Author: Max Shvetsov
 * @Name:   landing_speed_correction
 *
 * @descr:  This function calculates coefficient for landing speed based on range finder data
 *          It is assumed that the resulted coefficient is applied to
 *          minimal allowed landing speed
 *          It is assumed that correction by range finder is on
 *
 * @out:    (float) multiplying coefficient
 */
float MulticopterPositionControl::landing_speed_correction() {

    float landing_coeff = _params.land_speed_max/_params.land_speed_min;
    if(_local_pos.dist_bottom_valid) {
        /* -- MATH MAGIC --
         * We use linear function for speed correction
         * represented by
         *      f(x) = A*x + B
         * math knows that A is tangent of angle between oX and function line
         * and -B is offset
         * Function constructed returns 1.0 when dist_bottom == safe_land_h
         * and max/min when dist_bottom == 6.0
         * DO NOT modify this thing unless you are sure what you are doing.
         *
         * To change start height of speed correction - modify 6.0f value (hard-coded)
         * To change end height of speed correction - modify safe_land_h in A_SAFE_LAND_H
         */
        // A
        float tan_of_angle = ((_params.land_speed_max/_params.land_speed_min)-1)/(6.0f - _params.safe_land_h);
        // f(dist_bottom)
        landing_coeff = tan_of_angle * _local_pos.dist_bottom + (1 - tan_of_angle * _params.safe_land_h);
        /* -- END OF MATH MAGIC -- */

        // Don't increase speed more than land_speed_max
        if (landing_coeff > _params.land_speed_max/_params.land_speed_min) {
            landing_coeff = _params.land_speed_max/_params.land_speed_min;
        }
        // Don't decrease speed more than land_speed_min
        else if(landing_coeff < 1.1f) {
            landing_coeff = 0.999f;
        }

        if (landing_coeff < 1.1f) {
            /*
             * This section waits 1 second after sonar lowered speed to minimal
             * and then triggers max landing speed back to stop motors faster
             */
            was_corrected_till_end = true;
            if (landed_time == 0) {
                landed_time = hrt_absolute_time();
            }
            else if (hrt_absolute_time() - landed_time > 1500000) {
                landing_coeff = _params.land_speed_max/_params.land_speed_min;
                // reseting landing time
                landed_time = 0;
            }
        }
        else
        {
            was_corrected_till_end = false;
        }
        DOG_PRINT("[MC_POS] lid valid, current land speed %0.4f\n",
                (double) (landing_coeff * _params.land_speed_min));;
    }
    else {
        // Distance between home and current position = sqrt( (x_1 - x_2)^2 + (y_1 - y_2)^2 )
        float dist_between_points = sqrtf(pow((_pos(0) - _home_pos.x),2) + pow((_pos(1) - _home_pos.y), 2));
        float till_ground = _home_pos.z - _pos(2);
        if (was_corrected_till_end &&
                hrt_absolute_time() - landed_time > 1500000)
        {
            landing_coeff = _params.land_speed_max/_params.land_speed_min;
        }
        else if (dist_between_points < 5.0f) // then use home altitude to validate lidar
        {
            if (till_ground < _params.land_sensor_validation_dist) // then use regular speed
            {
                landing_coeff = _params.regular_land_speed/_params.land_speed_min;
            }
        }
        else 
        {
            landing_coeff = _params.regular_land_speed/_params.land_speed_min;
        }
        DOG_PRINT("[MC_POS] current land speed %0.4f between points %.4f till ground %.4f _pos(2) %.4f\n",
                (double) (landing_coeff * _params.land_speed_min),
                (double) dist_between_points,
                (double) till_ground,
                (double) _pos(2));
    }
    return landing_coeff;
}


/* @Author: Max Shvetsov, Ilja Nevdahs
 * @Name:   ground_dist_correction
 *
 * @descr:
 *
 * @out:    position correction variable
 */
bool MulticopterPositionControl::ground_dist_correction(){
    parameters_update(false);
    // No correction if not valid
    if (!_local_pos.dist_bottom_valid || !_params.sonar_correction_on)
        return false;

    //desired drop (positive = want to go down, negative = want to go up)
	float desired_drop = _pos_sp(2) - _pos(2);

	//available drop (positive = can go down; negative = must go up)
	float available_drop = _local_pos.dist_bottom - _params.sonar_min_dist;

	bool alt_corrected = false;

	if (available_drop < 0){
        //must go up
        if (first_ground_correction(3) == 0.0f) {
            /* if this is first ground correction - remember this
             * @description: this feature is implemented in case range finder is biased
             *               physicly for some reason (dirsty lenses, elecronics bug)
             */
            first_ground_correction(0) = _pos(0);
            first_ground_correction(1) = _pos(1);
            first_ground_correction(2) = _pos(2);
            first_ground_correction(3) = available_drop - 2;
            DOG_PRINT("[pos,dist_bottom] First time range correction, drop %.3f\n"
                    ,(double) -available_drop);
        } else {
            // This is not a first time we are correcting altitude
            if (first_ground_correction(2) + first_ground_correction(3) > _pos(2)) {
                // According to position_estimator we already corrected vertical drop
                // If we are still in the acceptance radius - disable ground distance correction
                //math::Vector<2> cur_xy(_pos(0), _pos(1));
                //math::Vector<2> first_xy(first_ground_correction(0), first_ground_correction(1));
                DOG_PRINT("[pos,dist_bottom] %.3f > %.3f\n"
                        ,(double) (first_ground_correction(2) + first_ground_correction(3))
                        ,(double) _pos(2));
                //if ( (cur_xy - first_xy).length() < _params.accept_radius * 0.3f ) {
                    return false;
                //}
            }
        }

        if ( - desired_drop < - available_drop )
            // If we want to go up not sufficiently
        {
            _pos_sp(2) = _pos(2) + available_drop;
            _ground_position_invalid = true;
            _ground_setpoint_corrected = true;
            alt_corrected = true;
        }
	}
	else {
        if (first_ground_correction(3) != 0.0f) {
            DOG_PRINT("[pos,dist_bottom] reseting first_ground_correction\n");
            first_ground_correction.zero();
        }
	//can go down
		if (desired_drop > 0){
		//want to go down
			if (desired_drop > available_drop) {
			//want to go down too much
				//limit going down too much
                DOG_PRINT("[pos,dist_bottom] correcting drop from %.3f to %.3f\n", (double)desired_drop, (double)available_drop);
				_pos_sp(2) = _pos(2) + available_drop;
				_ground_setpoint_corrected = true;
				_ground_position_invalid = false;
				alt_corrected = true;
			}
			else {
			//want to go down ok - don't limit
            // there might be descend speed correction for smooth landing
			}
		}
	}
	_ground_position_available_drop = available_drop;
	return alt_corrected;
}

int mc_pos_control_main(int argc, char *argv[])
{
	if (argc < 1) {
		errx(1, "usage: mc_pos_control {start|stop|status}");
	}

	if (!strcmp(argv[1], "start")) {

		if (pos_control::g_control != nullptr) {
			errx(1, "already running");
		}

		pos_control::g_control = new MulticopterPositionControl;

		if (pos_control::g_control == nullptr) {
			errx(1, "alloc failed");
		}

		if (OK != pos_control::g_control->start()) {
			delete pos_control::g_control;
			pos_control::g_control = nullptr;
			err(1, "start failed");
		}

		exit(0);
	}

	if (!strcmp(argv[1], "stop")) {
		if (pos_control::g_control == nullptr) {
			errx(1, "not running");
		}

		delete pos_control::g_control;
		pos_control::g_control = nullptr;
		exit(0);
	}

	if (!strcmp(argv[1], "status")) {
		if (pos_control::g_control) {
			errx(0, "running");

		} else {
			errx(1, "not running");
		}
	}

	warnx("unrecognized command");
	return 1;
}
