/****************************************************************************
 *
 *   Copyright (c) 2013, 2014 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 n*    notice, this list of conditions and the following disclaimer.
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
 * @file state_machine_helper.cpp
 * State machine helper functions implementations
 *
 * @author Thomas Gubler <thomasgubler@student.ethz.ch>
 * @author Julian Oes <julian@oes.ch>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <board_config.h>


#include <uORB/uORB.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/differential_pressure.h>
#include <uORB/topics/airspeed.h>
#include <uORB/topics/vehicle_gps_position.h>
#include <uORB/topics/sensor_combined.h>
#include <systemlib/systemlib.h>
#include <systemlib/param/param.h>
#include <systemlib/err.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_accel.h>
#include <drivers/drv_airspeed.h>
#include <drivers/drv_device.h>
#include <drivers/drv_pwm_output.h>
#include <airdog/calibrator/calibrator.hpp>
#include <mavlink/mavlink_log.h>
#include <position_estimator_inav/inertial_filter.h> // for initial position check

#include "state_machine_helper.h"
#include "commander_helper.h"

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

// This array defines the arming state transitions. The rows are the new state, and the columns
// are the current state. Using new state and current  state you can index into the array which
// will be true for a valid transition or false for a invalid transition. In some cases even
// though the transition is marked as true additional checks must be made. See arming_state_transition
// code for those checks.
static const bool arming_transitions[ARMING_STATE_MAX][ARMING_STATE_MAX] = {
	//                                  INIT,  STANDBY, ARMED, ARMED_ERROR, STANDBY_ERROR, REBOOT, IN_AIR_RESTORE
	{ /* ARMING_STATE_INIT */           true,  true,    false, false,       false,         false,  false },
	{ /* ARMING_STATE_STANDBY */        true,  true,    true,  true,        false,         false,  false },
	{ /* ARMING_STATE_ARMED */          false, true,    true,  false,       false,         false,  true },
	{ /* ARMING_STATE_ARMED_ERROR */    false, false,   true,  true,        false,         false,  false },
	{ /* ARMING_STATE_STANDBY_ERROR */  true,  true,    false, true,        true,          false,  false },
	{ /* ARMING_STATE_REBOOT */         true,  true,    false, false,       true,          true,   true },
	{ /* ARMING_STATE_IN_AIR_RESTORE */ false, false,   false, false,       false,         false,  false }, // NYI
};

// You can index into the array with an arming_state_t in order to get it's textual representation
static const char * const state_names[ARMING_STATE_MAX] = {
	"ARMING_STATE_INIT",
	"ARMING_STATE_STANDBY",
	"ARMING_STATE_ARMED",
	"ARMING_STATE_ARMED_ERROR",
	"ARMING_STATE_STANDBY_ERROR",
	"ARMING_STATE_REBOOT",
	"ARMING_STATE_IN_AIR_RESTORE",
};

transition_result_t
arming_state_transition(struct vehicle_status_s *status,		///< current vehicle status
			const struct safety_s   *safety,		///< current safety settings
			arming_state_t          new_arming_state,	///< arming state requested
			struct actuator_armed_s *armed,			///< current armed status
			bool			fRunPreArmChecks,	///< true: run the pre-arm checks, false: no pre-arm checks, for unit testing
			const int               mavlink_fd)		///< mavlink fd for error reporting, 0 for none
{

	// Double check that our static arrays are still valid
	ASSERT(ARMING_STATE_INIT == 0);
	ASSERT(ARMING_STATE_IN_AIR_RESTORE == ARMING_STATE_MAX - 1);

	transition_result_t ret = TRANSITION_DENIED;
	arming_state_t current_arming_state = status->arming_state;
	bool feedback_provided = false;

	/* only check transition if the new state is actually different from the current one */
	if (new_arming_state == current_arming_state) {
		ret = TRANSITION_NOT_CHANGED;

	} else {

		/*
		 * Get sensing state if necessary
		 */
		int prearm_ret = OK;

		/* only perform the check if we have to */
		if (fRunPreArmChecks && new_arming_state == ARMING_STATE_ARMED) {
			prearm_ret = prearm_check(status, mavlink_fd);
		}

		/*
		 * Perform an atomic state update
		 */
		irqstate_t flags = irqsave();

		/* enforce lockdown in HIL */
		if (status->hil_state == HIL_STATE_ON) {
			armed->lockdown = true;

		} else {
			armed->lockdown = false;
		}

		// Check that we have a valid state transition
		bool valid_transition = arming_transitions[new_arming_state][status->arming_state];

		if (valid_transition) {
			// We have a good transition. Now perform any secondary validation.
			if (new_arming_state == ARMING_STATE_ARMED) {

				// TODO! Check for correct fix!
				int pwm_fd = open(PWM_OUTPUT_DEVICE_PATH, 0);
				if (pwm_fd < 0) {
					mavlink_log_critical(mavlink_fd, "can't open %s", PWM_OUTPUT_DEVICE_PATH);
					feedback_provided = true;
					valid_transition = false;
				}
				// 0x0F for 1234 channels
				else if (ioctl(pwm_fd, PWM_SERVO_SET_FORCE_SAFETY_OFF, 0x0F) != 0) {
					mavlink_log_critical(mavlink_fd, "Could not turn off pwm safety switch.");
					feedback_provided = true;
					valid_transition = false;
				}
				if (pwm_fd >= 0) {
					mavlink_log_info(mavlink_fd, "Successfully disabled pwm safty!");
					close(pwm_fd);
				}
				//      Do not perform pre-arm checks if coming from in air restore
				//      Allow if HIL_STATE_ON
				if (status->arming_state != ARMING_STATE_IN_AIR_RESTORE &&
					status->hil_state == HIL_STATE_OFF) {

					// Fail transition if pre-arm check fails
					if (prearm_ret) {
						/* the prearm check already prints the reject reason */
						feedback_provided = true;
						valid_transition = false;

					// Fail transition if we need safety switch press
					} else if (safety->safety_switch_available && !safety->safety_off) {

						mavlink_log_critical(mavlink_fd, "NOT ARMING: Press safety switch first!");
						feedback_provided = true;
						valid_transition = false;
					}

					// Perform power checks only if circuit breaker is not
					// engaged for these checks
					if (!status->circuit_breaker_engaged_power_check) {
						// Fail transition if power is not good
						if (!status->condition_power_input_valid) {

							mavlink_log_critical(mavlink_fd, "NOT ARMING: Connect power module.");
							feedback_provided = true;
							valid_transition = false;
						}

						// Fail transition if power levels on the avionics rail
						// are measured but are insufficient
						if (status->condition_power_input_valid && (status->avionics_power_rail_voltage > 0.0f)) {
							// Check avionics rail voltages
							if (status->avionics_power_rail_voltage < 4.2f) {
								mavlink_log_critical(mavlink_fd, "NOT ARMING: Avionics power low: %6.2f Volt", (double)status->avionics_power_rail_voltage);
								feedback_provided = true;
								valid_transition = false;
							} else if (status->avionics_power_rail_voltage < 4.5f) {
								mavlink_log_critical(mavlink_fd, "CAUTION: Avionics power low: %6.2f Volt", (double)status->avionics_power_rail_voltage);
								feedback_provided = true;
							} else if (status->avionics_power_rail_voltage > 5.4f) {
								mavlink_log_critical(mavlink_fd, "CAUTION: Avionics power high: %6.2f Volt", (double)status->avionics_power_rail_voltage);
								feedback_provided = true;
							}
						}
					}

				}
			} else if (new_arming_state == ARMING_STATE_STANDBY && status->arming_state == ARMING_STATE_ARMED_ERROR) {
				new_arming_state = ARMING_STATE_STANDBY_ERROR;
			}
		}

		// HIL can always go to standby
		if (status->hil_state == HIL_STATE_ON && new_arming_state == ARMING_STATE_STANDBY) {
			valid_transition = true;
		}

		/* Sensors need to be initialized for STANDBY state */
		if (new_arming_state == ARMING_STATE_STANDBY && !status->condition_system_sensors_initialized) {
			mavlink_log_critical(mavlink_fd, "NOT ARMING: Sensors not operational.");
			feedback_provided = true;
			valid_transition = false;
		}

		// Finish up the state transition
		if (valid_transition) {
			armed->armed = new_arming_state == ARMING_STATE_ARMED || new_arming_state == ARMING_STATE_ARMED_ERROR;
			armed->ready_to_arm = new_arming_state == ARMING_STATE_ARMED || new_arming_state == ARMING_STATE_STANDBY;
			ret = TRANSITION_CHANGED;
			status->arming_state = new_arming_state;
		}

		/* end of atomic state update */
		irqrestore(flags);
	}

	if (ret == TRANSITION_DENIED) {
		const char * str = "INVAL: %s - %s";
		/* only print to console here by default as this is too technical to be useful during operation */
		warnx(str, state_names[status->arming_state], state_names[new_arming_state]);

		/* print to MAVLink if we didn't provide any feedback yet */
		if (!feedback_provided) {
			mavlink_log_critical(mavlink_fd, str, state_names[status->arming_state], state_names[new_arming_state]);
		}
	}
	else if (ret == TRANSITION_CHANGED) {
#ifdef GPIO_VDD_FORCE_POWER
		stm32_gpiowrite(GPIO_VDD_FORCE_POWER, new_arming_state == ARMING_STATE_ARMED);
#endif
	}

	return ret;
}

bool is_safe(const struct vehicle_status_s *status, const struct safety_s *safety, const struct actuator_armed_s *armed)
{
	// System is safe if:
	// 1) Not armed
	// 2) Armed, but in software lockdown (HIL)
	// 3) Safety switch is present AND engaged -> actuators locked
	if (!armed->armed || (armed->armed && armed->lockdown) || (safety->safety_switch_available && !safety->safety_off)) {
		return true;

	} else {
		return false;
	}
}

transition_result_t
main_state_transition(struct vehicle_status_s *status, main_state_t new_main_state, const int mavlink_fd)
{
	transition_result_t ret = TRANSITION_DENIED;

	/* transition may be denied even if the same state is requested because conditions may have changed */
	switch (new_main_state) {
	case MAIN_STATE_MANUAL:
	case MAIN_STATE_ACRO:
		ret = TRANSITION_CHANGED;
		break;

	case MAIN_STATE_ALTCTL:
		/* need at minimum altitude estimate */
		/* TODO: add this for fixedwing as well */
		if (!status->is_rotary_wing ||
		    (status->condition_local_altitude_valid ||
		     status->condition_global_position_valid)) {
			ret = TRANSITION_CHANGED;
		}
		break;

	case MAIN_STATE_POSCTL:
		/* need at minimum local position estimate */
		if (status->condition_local_position_valid ||
		    status->condition_global_position_valid) {
			ret = TRANSITION_CHANGED;
		}
		break;

	case MAIN_STATE_FOLLOW:
		/* need at minimum local position estimate */
		if (status->condition_local_position_valid || status->condition_global_position_valid) {
			float target_visibility_timeout_1;
			param_get(param_find("A_TRGT_VSB_TO_1"), &target_visibility_timeout_1);
			if (status->condition_target_position_valid) {
				ret = TRANSITION_CHANGED;
			}
			else if (status->main_state == MAIN_STATE_FOLLOW &&
					hrt_absolute_time() - status->last_target_time <= target_visibility_timeout_1 * 1000 * 1000) {
				// Already in Follow and target was lost for period less than timeout
				ret = TRANSITION_NOT_CHANGED; // Do not return deny before timeout
			}
		}
		break;

	case MAIN_STATE_AUTO_STANDBY:
		/* need valid arming state */
		if (status->arming_state == ARMING_STATE_STANDBY || status->arming_state == ARMING_STATE_ARMED)
			ret = TRANSITION_CHANGED;
		break;

	case MAIN_STATE_LOITER:
		/* need global position estimate, home and target position */
		if (status->condition_global_position_valid && status->condition_home_position_valid && status->condition_target_position_valid) {
			ret = TRANSITION_CHANGED;
		}
		break;

	case MAIN_STATE_AUTO_MISSION:
	    /* need global position and home position */
		if (status->condition_global_position_valid && status->condition_home_position_valid) {
			ret = TRANSITION_CHANGED;
		}
		break;
	case MAIN_STATE_RTL:
		/* Currently RTL is used as default failsafe mode thus all validations to be done in navigation state transition */
		ret = TRANSITION_CHANGED;
		break;

    case MAIN_STATE_EMERGENCY_RTL:
        if (status->condition_global_position_valid && status->condition_home_position_valid) {
            ret = TRANSITION_CHANGED;
        }
        break;

    case MAIN_STATE_EMERGENCY_LAND:
        ret = TRANSITION_CHANGED;
        break;

	case MAIN_STATE_CABLE_PARK:
        {
            /* need global position estimate */
            if (status->condition_path_points_valid && status->condition_global_position_valid && status->condition_target_position_valid) {
                ret = TRANSITION_CHANGED;
            }
            break;
        }
	case MAIN_STATE_ABS_FOLLOW:
    case MAIN_STATE_KITE_LITE:
    case MAIN_STATE_CIRCLE_AROUND:
    case MAIN_STATE_FRONT_FOLLOW:
        {
            /* need global position estimate */
            if (status->condition_global_position_valid && status->condition_target_position_valid) {
                ret = TRANSITION_CHANGED;
            }
            break;
        }

	case MAIN_STATE_AUTO_PATH_FOLLOW:
		/* need global position estimate */
		if (status->condition_global_position_valid && status->condition_target_position_valid) {
			ret = TRANSITION_CHANGED;
		}
		break;

	case MAIN_STATE_OFFBOARD:

		/* need offboard signal */
		if (!status->offboard_control_signal_lost) {
			ret = TRANSITION_CHANGED;
		}

		break;

	case MAIN_STATE_MAX:
	default:
		break;
	}

	if (ret == TRANSITION_CHANGED) {
		if (status->main_state != new_main_state) {
			mavlink_log_info(mavlink_fd, "[Main State Transition] Success: state %d to %d!", status->main_state, new_main_state);
			status->main_state = new_main_state;
		} else {
			ret = TRANSITION_NOT_CHANGED;
		}
	}

	return ret;
}

void
airdog_state_transition(struct vehicle_status_s *status, airdog_state_t new_airdog_state, const int mavlink_fd) {
    status->airdog_state = new_airdog_state;

    const char * str = "unknown";

    switch (new_airdog_state){
    	case AIRD_STATE_STANDBY:
            str = "standby";
            break;
        case AIRD_STATE_LANDED:
            str = "landed";
            break;
        case AIRD_STATE_PREFLIGHT_MOTOR_CHECK:
            str = "preflight motor check";
            break;
        case AIRD_STATE_TAKING_OFF:
            str = "taking_off";
            break;
        case AIRD_STATE_LANDING:
            str = "landing";
            break;
        case AIRD_STATE_IN_AIR:
            str = "in air";
            break;
    }

    mavlink_log_info(mavlink_fd, "Airdog state machine state: %s", str);
}

/**
 * Transition from one hil state to another
 */
transition_result_t hil_state_transition(hil_state_t new_state, int status_pub, struct vehicle_status_s *current_status, const int mavlink_fd)
{
	transition_result_t ret = TRANSITION_DENIED;

	if (current_status->hil_state == new_state) {
		ret = TRANSITION_NOT_CHANGED;

	} else {
		switch (new_state) {
		case HIL_STATE_OFF:
			/* we're in HIL and unexpected things can happen if we disable HIL now */
			mavlink_log_critical(mavlink_fd, "#audio: Not switching off HIL (safety)");
			ret = TRANSITION_DENIED;
			break;

		case HIL_STATE_ON:
			if (current_status->arming_state == ARMING_STATE_INIT
			    || current_status->arming_state == ARMING_STATE_STANDBY
			    || current_status->arming_state == ARMING_STATE_STANDBY_ERROR) {

				/* Disable publication of all attached sensors */
				/* list directory */
				DIR		*d;
				d = opendir("/dev");

				if (d) {
					struct dirent	*direntry;
					char devname[24];

					while ((direntry = readdir(d)) != NULL) {

						/* skip serial ports */
						if (!strncmp("tty", direntry->d_name, 3)) {
							continue;
						}

						/* skip mtd devices */
						if (!strncmp("mtd", direntry->d_name, 3)) {
							continue;
						}

						/* skip ram devices */
						if (!strncmp("ram", direntry->d_name, 3)) {
							continue;
						}

						/* skip MMC devices */
						if (!strncmp("mmc", direntry->d_name, 3)) {
							continue;
						}

						/* skip mavlink */
						if (!strcmp("mavlink", direntry->d_name)) {
							continue;
						}

						/* skip console */
						if (!strcmp("console", direntry->d_name)) {
							continue;
						}

						/* skip null */
						if (!strcmp("null", direntry->d_name)) {
							continue;
						}

						snprintf(devname, sizeof(devname), "/dev/%s", direntry->d_name);

						int sensfd = ::open(devname, 0);

						if (sensfd < 0) {
							warn("failed opening device %s", devname);
							continue;
						}

						int block_ret = ::ioctl(sensfd, DEVIOCSPUBBLOCK, 1);
						close(sensfd);

						printf("Disabling %s: %s\n", devname, (block_ret == OK) ? "OK" : "ERROR");
					}
					closedir(d);
					ret = TRANSITION_CHANGED;
					mavlink_log_critical(mavlink_fd, "Switched to ON hil state");


				} else {
					/* failed opening dir */
					mavlink_log_info(mavlink_fd, "FAILED LISTING DEVICE ROOT DIRECTORY");
					ret = TRANSITION_DENIED;
				}
			} else {
				mavlink_log_critical(mavlink_fd, "Not switching to HIL when armed");
				ret = TRANSITION_DENIED;
			}
			break;

		default:
			warnx("Unknown HIL state");
			break;
		}
	}

	if (ret == TRANSITION_CHANGED) {
		current_status->hil_state = new_state;
		current_status->timestamp = hrt_absolute_time();
		// XXX also set lockdown here
		orb_publish(ORB_ID(vehicle_status), status_pub, current_status);
	}
	return ret;
}

/**
 * Check failsafe and main status and set navigation status for navigator accordingly
 */
bool set_nav_state(struct vehicle_status_s *status, const bool data_link_loss_enabled, const bool mission_finished,
		const bool stay_in_failsafe, const int mavlink_fd)

{
	navigation_state_t nav_state_old = status->nav_state;

	bool armed = (status->arming_state == ARMING_STATE_ARMED || status->arming_state == ARMING_STATE_ARMED_ERROR);
	status->failsafe = false;
	// There are more fallbacks, so assume fallback by default
	status->nav_state_fallback = true;

	/* evaluate main state to decide in normal (non-failsafe) mode */
	switch (status->main_state) {
	case MAIN_STATE_ACRO:
	case MAIN_STATE_MANUAL:
	case MAIN_STATE_ALTCTL:
	case MAIN_STATE_POSCTL:
	case MAIN_STATE_FOLLOW:
		/* require RC for all manual modes */
		if ((status->rc_signal_lost || status->rc_signal_lost_cmd) && armed) {
			status->failsafe = true;

			if (status->condition_global_position_valid && status->condition_home_position_valid) {
				status->nav_state = NAVIGATION_STATE_AUTO_RCRECOVER;
			} else if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}

		} else {
			// Only intended nav modes follow, so no fallback
			status->nav_state_fallback = false;
			switch (status->main_state) {
			case MAIN_STATE_ACRO:
				status->nav_state = NAVIGATION_STATE_ACRO;
				break;

			case MAIN_STATE_MANUAL:
				status->nav_state = NAVIGATION_STATE_MANUAL;
				break;

			case MAIN_STATE_ALTCTL:
				status->nav_state = NAVIGATION_STATE_ALTCTL;
				break;

			case MAIN_STATE_POSCTL:
				status->nav_state = NAVIGATION_STATE_POSCTL;
				break;

			// TODO! [AK] Consider falling back to Loiter in case signal is lost (might not work correctly in mc_pos)
			case MAIN_STATE_FOLLOW:
				status->nav_state = NAVIGATION_STATE_FOLLOW;
				break;

			default:
				status->nav_state = NAVIGATION_STATE_MANUAL;
				break;
			}
		}
		break;
	case MAIN_STATE_AUTO_STANDBY:
		status->nav_state_fallback = false;
		status->nav_state = NAVIGATION_STATE_AUTO_STANDBY;
		break;
	case MAIN_STATE_AUTO_MISSION:
		/* go into failsafe
		 * - if commanded to do so
		 * - if we have an engine failure
		 * - if either the datalink is enabled and lost as well as RC is lost
		 * - if there is no datalink and the mission is finished */
		if (status->engine_failure_cmd) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDENGFAIL;
		} else if (status->data_link_lost_cmd) {
			status->nav_state = NAVIGATION_STATE_AUTO_RTGS;
		} else if (status->gps_failure_cmd) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDGPSFAIL;
		} else if (status->rc_signal_lost_cmd) {
			status->nav_state = NAVIGATION_STATE_AUTO_RTGS; //XXX
		/* Finished handling commands which have priority , now handle failures */
		} else if (status->gps_failure) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDGPSFAIL;
		} else if (status->engine_failure) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDENGFAIL;
		} else if (((status->data_link_lost && data_link_loss_enabled) && status->rc_signal_lost) ||
		    (!data_link_loss_enabled && status->rc_signal_lost && mission_finished)) {
			status->failsafe = true;

			if (status->condition_global_position_valid && status->condition_home_position_valid) {
				status->nav_state = NAVIGATION_STATE_AUTO_RTGS;
			} else if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}

		/* also go into failsafe if just datalink is lost */
		} else if (status->data_link_lost && data_link_loss_enabled) {
			status->failsafe = true;

			if (status->condition_global_position_valid && status->condition_home_position_valid) {
				status->nav_state = NAVIGATION_STATE_AUTO_RTGS;
			} else if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}

		/* don't bother if RC is lost and mission is not yet finished */
		} else if (status->rc_signal_lost && !stay_in_failsafe) {

			status->nav_state_fallback = false;
			/* this mode is ok, we don't need RC for missions */
			status->nav_state = NAVIGATION_STATE_AUTO_MISSION;
		} else if (!stay_in_failsafe){
			status->nav_state_fallback = false;
			/* everything is perfect */
			status->nav_state = NAVIGATION_STATE_AUTO_MISSION;
		}
		break;

	case MAIN_STATE_LOITER:
		/* go into failsafe on a engine failure */
		if (status->engine_failure) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDENGFAIL;
		// TODO! [AK] Shouldn't it be target signal?
		/* also go into failsafe if just datalink is lost */
		} else if (status->data_link_lost && data_link_loss_enabled) {
			status->failsafe = true;

			if (status->condition_global_position_valid && status->condition_home_position_valid) {
				status->nav_state = NAVIGATION_STATE_AUTO_RTGS;
			} else if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}

		/* don't bother if RC is lost if datalink is connected */
		} else if (status->rc_signal_lost) {

			status->nav_state_fallback = false;
			/* this mode is ok, we don't need RC for loitering */
			status->nav_state = NAVIGATION_STATE_LOITER;
		} else {
			status->nav_state_fallback = false;
			/* everything is perfect */
			status->nav_state = NAVIGATION_STATE_LOITER;
		}
		break;

	case MAIN_STATE_RTL:
		/* require global position and home, also go into failsafe on an engine failure */

		if (status->engine_failure) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDENGFAIL;
		} else if ((!status->condition_global_position_valid ||
					!status->condition_home_position_valid)) {
			status->failsafe = true;

			if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}
		} else {
			status->nav_state_fallback = false;
			status->nav_state = NAVIGATION_STATE_RTL;
		}
		break;

    case MAIN_STATE_EMERGENCY_RTL:
		if (status->engine_failure) {
			status->nav_state = NAVIGATION_STATE_AUTO_LANDENGFAIL;
		} else if ((!status->condition_global_position_valid ||
					!status->condition_home_position_valid)) {
			status->failsafe = true;

			if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}
		} else {
			status->nav_state_fallback = false;
			status->nav_state = NAVIGATION_STATE_RTL;
		}
        break;
    case MAIN_STATE_EMERGENCY_LAND:
		status->nav_state_fallback = false;
        status->nav_state = NAVIGATION_STATE_LAND;
        break;

	case MAIN_STATE_CABLE_PARK:
	case MAIN_STATE_ABS_FOLLOW:
	case MAIN_STATE_AUTO_PATH_FOLLOW:
    case MAIN_STATE_CIRCLE_AROUND:
    case MAIN_STATE_KITE_LITE:
    case MAIN_STATE_FRONT_FOLLOW:

		float target_visibility_timeout_1;
		param_get(param_find("A_TRGT_VSB_TO_1"), &target_visibility_timeout_1);
		if ((!status->condition_target_position_valid &&
				hrt_absolute_time() - status->last_target_time > target_visibility_timeout_1 * 1000 * 1000)) {
			// On first timeout when status.condition_target_position_valid is false go into aim-and-shoot
			if (status->nav_state != NAVIGATION_STATE_LOITER && status->nav_state != NAVIGATION_STATE_AUTO_LANDENGFAIL) {
				mavlink_log_info(mavlink_fd, "Target signal time-out, switching to Aim-and-shoot.");
			}
			// Ignore more complex Loiter fallbacks
			if (status->engine_failure) {
				status->nav_state = NAVIGATION_STATE_AUTO_LANDENGFAIL;
			}
			else {
				status->nav_state = NAVIGATION_STATE_LOITER;
			}
        } else {
			status->nav_state_fallback = false;
			switch (status->main_state) {
                case MAIN_STATE_CABLE_PARK:
                    status->nav_state = NAVIGATION_STATE_CABLE_PARK;
                    break;
                case MAIN_STATE_ABS_FOLLOW:
                    status->nav_state = NAVIGATION_STATE_ABS_FOLLOW;
                    break;
                case MAIN_STATE_AUTO_PATH_FOLLOW:
                    status->nav_state = NAVIGATION_STATE_AUTO_PATH_FOLLOW;
                    break;
                case MAIN_STATE_KITE_LITE:
                    status->nav_state = NAVIGATION_STATE_KITE_LITE;
                    break;
                case MAIN_STATE_CIRCLE_AROUND:
                    status->nav_state = NAVIGATION_STATE_CIRCLE_AROUND;
                    break;
                case MAIN_STATE_FRONT_FOLLOW:
                    status->nav_state = NAVIGATION_STATE_FRONT_FOLLOW;
                    break;
            }

        }
        break;

	case MAIN_STATE_OFFBOARD:
		/* require offboard control, otherwise stay where you are */
		if (status->offboard_control_signal_lost && !status->rc_signal_lost) {
			status->failsafe = true;

			status->nav_state = NAVIGATION_STATE_POSCTL;
		} else if (status->offboard_control_signal_lost && status->rc_signal_lost) {
			status->failsafe = true;

			if (status->condition_local_position_valid) {
				status->nav_state = NAVIGATION_STATE_LAND;
			} else if (status->condition_local_altitude_valid) {
				status->nav_state = NAVIGATION_STATE_DESCEND;
			} else {
				status->nav_state = NAVIGATION_STATE_TERMINATION;
			}
		} else {
			status->nav_state_fallback = false;
			status->nav_state = NAVIGATION_STATE_OFFBOARD;
		}
	default:
		break;
	}

	return status->nav_state != nav_state_old;
}

int prearm_check(const struct vehicle_status_s *status, const int mavlink_fd)
{
	int ret;
	bool failed = false;
	int gyro_calib_on_arm = 0;
	float gyro_calib_temp;
	unsigned int gyro_calib_date;
	uint64_t gyro_calib_usec;
	vehicle_gps_position_s gps_data;
	sensor_combined_s sensors;
	int recalibration_date_diff;
	float recalibration_temp_diff;

	int fd = open(ACCEL_DEVICE_PATH, O_RDONLY);

	if (fd < 0) {
		mavlink_log_critical(mavlink_fd, "ARM FAIL: ACCEL SENSOR MISSING");
		failed = true;
		goto system_eval;
	}

	ret = ioctl(fd, ACCELIOCSELFTEST, 0);

	if (ret != OK) {
		mavlink_log_critical(mavlink_fd, "ARM FAIL: ACCEL CALIBRATION");
		failed = true;
		goto system_eval;
	}

	/* check measurement result range */
	struct accel_report acc;
	ret = read(fd, &acc, sizeof(acc));

	if (ret == sizeof(acc)) {
		/* evaluate values */
		float accel_magnitude = sqrtf(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);

		if (accel_magnitude < 4.0f || accel_magnitude > 15.0f /* m/s^2 */) {
			mavlink_log_critical(mavlink_fd, "ARM FAIL: ACCEL RANGE, hold still");
			/* this is frickin' fatal */
			failed = true;
			goto system_eval;
		}
	} else {
		mavlink_log_critical(mavlink_fd, "ARM FAIL: ACCEL READ");
		/* this is frickin' fatal */
		failed = true;
		goto system_eval;
	}
	close(fd);

	param_get(param_find("A_CALIB_GYRO_ARM"), &gyro_calib_on_arm);
	param_get(param_find("SENS_GYRO_CTEMP"), &gyro_calib_temp);
	param_get(param_find("SENS_GYRO_CDATE"), &gyro_calib_date);
	param_get(param_find("A_CALIB_dTEMP_C"), &recalibration_temp_diff);
	param_get(param_find("A_CALIB_dDATE_H"), &recalibration_date_diff);
	if (status->hil_state == HIL_STATE_ON) {
		gyro_calib_on_arm = 0; // Don't calibrate in HIL
	}

	fd = orb_subscribe(ORB_ID(vehicle_gps_position));
	// Should fail if GPS fix is not obtained yet
	ret = orb_copy(ORB_ID(vehicle_gps_position), fd, &gps_data);
	close(fd);
	gyro_calib_usec = ((uint64_t) gyro_calib_date) * 60 * 60 * 1000000;

	// Consider not calibrating gyro if GPS time went forwards but was less than N hours away from the last calibration
	if (ret == 0 && gps_data.time_gps_usec > gyro_calib_usec &&
			(gyro_calib_usec + ((uint64_t)recalibration_date_diff) * 24*60*60*1000000 > gps_data.time_gps_usec)) {
		fd = orb_subscribe(ORB_ID(sensor_combined));
		ret = orb_copy(ORB_ID(sensor_combined), fd, &sensors);
		close(fd);
		// Don't calibrate gyro if temperature difference from the last calibration was less than N degrees
		if (ret == 0 && (fabsf(sensors.baro_temp_celcius - gyro_calib_temp) < recalibration_temp_diff)) {
			gyro_calib_on_arm = 0;
		}
	}

	/* Launch gyro calibration in cases where prearm checks are required */
	// TODO! Consider removing the sanity checks above - gyro has stricter accel checks
	if (gyro_calib_on_arm == 1 && !calibration::calibrate_gyroscope(mavlink_fd, 1000, 20, 100)) { // Parameters reduced to allow faster results
		mavlink_log_critical(mavlink_fd, "Prearm gyro calibration failed!");
		failed = true;
		goto system_eval;
	}

    /* check valid GPS if required and controll if current local_position change rate is low
     * this should be done due to specific lpos altitude calculation. Arm without low vertical speed
     * (still correcting initial *without GPS* altitude) can result in wrong initial possition
     */
	if (status->require_gps && !status->condition_global_position_valid) {
        failed = true;
        goto system_eval;
	}

	/* Perform airspeed check only if circuit breaker is not
	 * engaged and it's not a rotary wing */
	if (!status->circuit_breaker_engaged_airspd_check && !status->is_rotary_wing) {
		fd = orb_subscribe(ORB_ID(airspeed));

		struct airspeed_s airspeed;

		if ((ret = orb_copy(ORB_ID(airspeed), fd, &airspeed)) ||
			(hrt_elapsed_time(&airspeed.timestamp) > (50 * 1000))) {
			mavlink_log_critical(mavlink_fd, "ARM FAIL: AIRSPEED SENSOR MISSING");
			failed = true;
			goto system_eval;
		}
		close(fd);

		if (fabsf(airspeed.indicated_airspeed_m_s > 6.0f)) {
			mavlink_log_critical(mavlink_fd, "AIRSPEED WARNING: WIND OR CALIBRATION MISSING");
			// XXX do not make this fatal yet
		}
	}

system_eval:
	close(fd);
	return (failed);
}
