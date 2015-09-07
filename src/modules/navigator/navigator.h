/***************************************************************************
 *
 *   Copyright (c) 2013-2014 PX4 Development Team. All rights reserved.
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
 * @file navigator.h
 * Helper class to access missions
 * @author Julian Oes <julian@oes.ch>
 * @author Anton Babushkin <anton.babushkin@me.com>
 * @author Thomas Gubler <thomasgubler@gmail.com>
 */

#ifndef NAVIGATOR_H
#define NAVIGATOR_H

#include <systemlib/perf_counter.h>

#include <controllib/blocks.hpp>
#include <controllib/block/BlockParam.hpp>

#include <uORB/uORB.h>
#include <uORB/topics/mission.h>
#include <uORB/topics/follow_offset.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/position_setpoint_triplet.h>
#include <uORB/topics/vehicle_global_position.h>
#include <uORB/topics/vehicle_gps_position.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/mission_result.h>
#include <uORB/topics/vehicle_attitude_setpoint.h>
#include <uORB/topics/target_global_position.h>
#include <uORB/topics/commander_request.h>
#include <uORB/topics/external_trajectory.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/position_restriction.h>

#include <commander/px4_custom_mode.h>

#include "navigator_mode.h"
#include "mission.h"
#include "loiter.h"
#include "rtl.h"
#include "datalinkloss.h"
#include "enginefailure.h"
#include "gpsfailure.h"
#include "rcloss.h"
#include "geofence.h"
#include "path_follow.hpp"
#include "leashed_follow.hpp"
#include "land.h"
#include "offset_follow.h"

#define NAVIGATOR_MODE_ARRAY_SIZE 11

class Navigator : public control::SuperBlock
{
public:
	/**
	 * Constructor
	 */
	Navigator();

	/**
	 * Destructor, also kills the navigators task.
	 */
	~Navigator();

	/**
	 * Start the navigator task.
	 *
	 * @return		OK on success.
	 */
	int		start();

	/**
	 * Display the navigator status.
	 */
	void		status();

	/**
	 * Add point to geofence
	 */
	void		add_fence_point(int argc, char *argv[]);

	/**
	 * Load fence from file
	 */
	void		load_fence_from_file(const char *filename);

	/**
	 * Publish the mission result so commander and mavlink know what is going on
	 */
	void publish_mission_result();

	/**
	 * Publish the attitude sp, only to be used in very special modes when position control is deactivated
	 * Example: mode that is triggered on gps failure
	 */
	void publish_att_sp();

	/**
	 * Publish a new position restriction for cable park mode
	 */
	void		publish_position_restriction();

    /*
     * Publish follow offset for offset follow modes
     */
    void        publish_follow_offset();

	/**
	 * Reset all validity flags of the triplet to "invalid"
	 * to prevent old values from taking effect
	 */
	void invalidate_setpoint_triplet();

	/**
	 * Helper function resets validity flags of a single setpoint structure to "invalid"
	 */
	inline static void invalidate_single_setpoint(position_setpoint_s &setpoint);

	/**
	 * Setters
	 */
	void		set_can_loiter_at_sp(bool can_loiter) { _can_loiter_at_sp = can_loiter; }
	void		set_position_setpoint_triplet_updated() { _pos_sp_triplet_updated = true; }
	void		set_commander_request_updated() { _commander_request_updated = true; }
    bool        set_next_path_point(double point[3], bool force = false, int num = 0);
    bool        get_path_points(int point_num, double point[3]);
    void        clear_path_points();
    bool        set_flag_reset_pfol_offs(bool value); // set value of _reset_path_follow_offset flag

	/**
	 * Getters
	 */
	struct vehicle_status_s*	    get_vstatus() { return &_vstatus; }
	struct vehicle_control_mode_s*	    get_control_mode() { return &_control_mode; }
	struct vehicle_global_position_s*   get_global_position() { return &_global_pos; }
	struct vehicle_gps_position_s*	    get_gps_position() { return &_gps_pos; }
	struct sensor_combined_s*	    get_sensor_combined() { return &_sensor_combined; }
	struct home_position_s*		    get_home_position() { return &_home_pos; }
	struct position_setpoint_triplet_s* get_position_setpoint_triplet() { return &_pos_sp_triplet; }
	struct follow_offset_s*             get_follow_offset() { return &_follow_offset; }
	struct mission_result_s*	    get_mission_result() { return &_mission_result; }
	struct vehicle_attitude_setpoint_s* get_att_sp() { return &_att_sp; }
    bool get_flag_reset_pfol_offs(); // get value of _reset_path_follow_offset flag

	struct commander_request_s*	get_commander_request() { return &_commander_request;};
	struct target_global_position_s*    get_target_position() {return &_target_pos; }
	struct external_trajectory_s*		get_target_trajectory() {return &_target_trajectory;}
	struct vehicle_attitude_s*			get_vehicle_attitude() {return &_vehicle_attitude;}
	int		get_onboard_mission_sub() { return _onboard_mission_sub; }
	int		get_offboard_mission_sub() { return _offboard_mission_sub; }
	int		get_vehicle_command_sub() { return _vcommand_sub; }
	Geofence&	get_geofence() { return _geofence; }
	bool		get_can_loiter_at_sp() { return _can_loiter_at_sp; }
	float		get_loiter_radius() { return _param_loiter_radius.get(); }
	float		get_acceptance_radius() { return _param_acceptance_radius.get(); }
	int		get_mavlink_fd() { return _mavlink_fd; }
	
	void public_vehicle_attitude_update();
	int  public_poll_update_sensor_combined_and_vehicle_attitude(const unsigned timeout_ms);

private:

	bool		_task_should_exit;		/**< if true, sensor task should exit */
	int		_navigator_task;		/**< task handle for sensor task */

	int		_mavlink_fd;			/**< the file descriptor to send messages over mavlink */

	int		_global_pos_sub;		/**< global position subscription */
	int		_gps_pos_sub;		/**< gps position subscription */
	int		_sensor_combined_sub;		/**< sensor combined subscription */
	int		_home_pos_sub;			/**< home position subscription */
	int		_vstatus_sub;			/**< vehicle status subscription */
	int		_capabilities_sub;		/**< notification of vehicle capabilities updates */
	int		_control_mode_sub;		/**< vehicle control mode subscription */
	int		_onboard_mission_sub;		/**< onboard mission subscription */
	int		_offboard_mission_sub;		/**< offboard mission subscription */
	int		_param_update_sub;		/**< param update subscription */
	int 	_vcommand_sub;			/**< vehicle control subscription */
	int 	_target_pos_sub; 		/**< target position subscription */
	int		_target_trajectory_sub;	/**< target trajectory subscription */
	int		_vehicle_attitude_sub;	/**< vehicle attitude subscription - not polled as part of the main loop*/
    double   _first_leash_point[3];  /**< first point to draw path in cable park */
    double   _last_leash_point[3];   /**< last point to draw path in cable park */
    bool _flag_reset_pfol_offs;

	orb_advert_t	_pos_sp_triplet_pub;		/**< publish position setpoint triplet */
    orb_advert_t    _pos_restrict_pub;          /**< publish position restriction */
	orb_advert_t	_mission_result_pub;
	orb_advert_t	_att_sp_pub;			/**< publish att sp
							  used only in very special failsafe modes
							  when pos control is deactivated */
	orb_advert_t 	_commander_request_pub; 		/**< publish commander requests */
    orb_advert_t    _follow_offset_pub;             /**< publish follow offset of offset follow modes */

	vehicle_status_s				_vstatus;		/**< vehicle status */
	vehicle_control_mode_s				_control_mode;		/**< vehicle control mode */
	vehicle_global_position_s			_global_pos;		/**< global vehicle position */
	vehicle_gps_position_s				_gps_pos;		/**< gps position */
	sensor_combined_s				_sensor_combined;	/**< sensor values */
	home_position_s					_home_pos;		/**< home position for RTL */
	mission_item_s 					_mission_item;		/**< current mission item */
	navigation_capabilities_s			_nav_caps;		/**< navigation capabilities */
	position_setpoint_triplet_s			_pos_sp_triplet;	/**< triplet of position setpoints */
	follow_offset_s                     _follow_offset;
	position_restriction_s		_pos_restrict;	/**< position restriction*/

	mission_result_s				_mission_result;
	vehicle_attitude_setpoint_s					_att_sp;
	target_global_position_s 			_target_pos;		/**< global target position */
	external_trajectory_s				_target_trajectory; /**< target trajectory */
	vehicle_attitude_s					_vehicle_attitude;	/**< vehicle attitude - not updated as part of the main loop*/
	commander_request_s					_commander_request;

	bool 		_mission_item_valid;		/**< flags if the current mission item is valid */

	perf_counter_t	_loop_perf;			/**< loop performance counter */

	Geofence	_geofence;			/**< class that handles the geofence */
	bool		_geofence_violation_warning_sent; /**< prevents spaming to mavlink */

	bool		_inside_fence;			/**< vehicle is inside fence */

	NavigatorMode	*_navigation_mode;		/**< abstract pointer to current navigation mode class */
	Mission		_mission;			/**< class that handles the missions */
	Loiter		_loiter;			/**< class that handles loiter */
	RTL 		_rtl;				/**< class that handles RTL */
	RCLoss 		_rcLoss;				/**< class that handles RTL according to
							  OBC rules (rc loss mode) */
	DataLinkLoss	_dataLinkLoss;			/**< class that handles the OBC datalink loss mode */
	EngineFailure	_engineFailure;			/**< class that handles the engine failure mode
							  (FW only!) */
	GpsFailure	_gpsFailure;			/**< class that handles the OBC gpsfailure loss mode */
	PathFollow	_path_follow;		/**< class that handles Follow Path*/
	OffsetFollow    _offset_follow;		/**< class that handles Follow Path*/
    Leashed     _cable_path;            /**< class that handles Cable Park */

    Land        _land;                  /**/

	NavigatorMode *_navigation_mode_array[NAVIGATOR_MODE_ARRAY_SIZE];	/**< array of navigation modes */

	bool		_can_loiter_at_sp;			/**< flags if current position SP can be used to loiter */
	bool		_pos_sp_triplet_updated;		/**< flags if position SP triplet needs to be published */
	bool		_commander_request_updated;		/**< flags if commander request needs to be published */

	control::BlockParamFloat _param_loiter_radius;	/**< loiter radius for fixedwing */
	control::BlockParamFloat _param_acceptance_radius;	/**< acceptance for takeoff */
	control::BlockParamInt _param_datalinkloss_obc;	/**< if true: obc mode on data link loss enabled */
	control::BlockParamInt _param_rcloss_obc;	/**< if true: obc mode on rc loss enabled */
	/**
	 * Retrieve global position
	 */
	void		global_position_update();

	/**
	 * Retrieve gps position
	 */
	void		gps_position_update();

	/**
	 * Retrieve sensor values
	 */
	void		sensor_combined_update();

	/**
	 * Retrieve home position
	 */
	void		home_position_update();

	/**
	 * Retreive navigation capabilities
	 */
	void		navigation_capabilities_update();

	/**
	 * Retrieve vehicle status
	 */
	void		vehicle_status_update();

	/**
	 * Retrieve vehicle control mode
	 */
	void		vehicle_control_mode_update();

	/**
	 * Update parameters
	 */
	void		params_update();

	/**
	 * Retrieve target global position
	 */
	void		target_position_update();

	/**
	 * Retrieve target trajectory
	 */
	void 		target_trajectory_update();
	
	/**
	 * Retrieve vehicle attitude
	 */
	void 		vehicle_attitude_update();

	/**
	 * Shim for calling task_main from task_create.
	 */
	static void	task_main_trampoline(int argc, char *argv[]);

	/**
	 * Main task.
	 */
	void		task_main();

	/**
	 * Translate mission item to a position setpoint.
	 */
	void		mission_item_to_position_setpoint(const mission_item_s *item, position_setpoint_s *sp);

	/**
	 * Publish a new position setpoint triplet for position controllers
	 */
	void		publish_position_setpoint_triplet();


	/**
	 * Publish requests for commander
	 */
	void 		publish_commander_request();

	/* this class has ptr data members, so it should not be copied,
	 * consequently the copy constructors are private.
	 */
	Navigator(const Navigator&);
	Navigator operator=(const Navigator&);
};
#endif
