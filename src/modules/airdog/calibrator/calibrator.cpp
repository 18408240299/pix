#include <nuttx/config.h>

#include <drivers/drv_accel.h>
#include <drivers/calibration/calibration.hpp>
#include <drivers/drv_gyro.h> // ioctl commands
#include <drivers/drv_mag.h>
#include <drivers/drv_tone_alarm.h>
#include <systemlib/err.h>
#include <systemlib/systemlib.h>
#include <fcntl.h> // open
#include <mavlink/mavlink_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TODO! Still detector includes, not needed when still detector is moved to a seprate file
#include <modules/uORB/topics/sensor_combined.h>
#include <drivers/drv_hrt.h>
#include <geo/geo.h>
#include <errno.h>

#include "calibrator.hpp"
#include "calibration_commons.hpp"
#include "accel_calibration.hpp"
#include "gyro_calibration.hpp"
#include "level_calibration.hpp"
#include "mag_calibration.hpp"

#include <uORB/uORB.h>
#include <uORB/topics/calibrator.h>

namespace calibration {

enum class TONES : uint8_t {
	PREPARE = 0,
	START,
	WAITING_FOR_USER,
	NEGATIVE,
	FINISHED,
	ERROR,
	STOP, // stops the current tune
	WORKING
};

// TODO! Consider using this class for prepare and print_results functions too
enum class SENSOR_TYPE : uint8_t {
	GYRO = 0,
	MAG,
	ACCEL
};

static bool stopCalibration = true;
static int taskParameterWhat = 0;
static int taskParameterMavlinkFd = 0;

static int start_calibrate_task(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	int what = taskParameterWhat;
	int mavlink_fd = taskParameterMavlinkFd;

	switch (what)
	{
	case CALIBRATE_ACCELEROMETER:
		calibrate_accelerometer(mavlink_fd);
		break;

	case CALIBRATE_GYROSCOPE:
		calibrate_gyroscope(mavlink_fd);
		break;

	case CALIBRATE_MAGNETOMETER:
		calibrate_magnetometer(mavlink_fd);
		break;
	}

	return 0;
}

__EXPORT int calibrate_in_new_task(int what, int mavlink_fd)
{
	int calibration_task = 0;

	taskParameterWhat = what;
	taskParameterMavlinkFd = mavlink_fd;

	calibration_task = task_spawn_cmd("leash_app",
			SCHED_DEFAULT,
			SCHED_PRIORITY_DEFAULT - 30,
			3000,
			start_calibrate_task,
			nullptr);//(const char **)parameters);


	return calibration_task;
}

__EXPORT void calibrate_stop()
{
	stopCalibration = true;
}

// Common procedure for sensor calibration that waits for the user to get ready
inline void prepare(const char* sensor_type, const int beeper_fd);
// Translates TONES enum to specific tone_alarm tunes
inline void beep(const int beeper_fd, TONES tone);
// Print final calibration results
inline void print_results (CALIBRATION_RESULT res, const char* sensor_type, const int beeper_fd, int mavlink_fd);
// Print calibration scales and offsets currently set
inline void print_scales(SENSOR_TYPE sensor, int mavlink_fd);
// Helper function for print_scales
template <class scale_T> void print_scales_helper(const char* device_path, int command, int mavlink_fd);

__EXPORT bool calibrate_gyroscope(int mavlink_fd, const unsigned int sample_count,
		const unsigned int max_error_count,
		const int timeout) {
	CALIBRATION_RESULT res;

	struct calibrator_s calibrator;
	orb_advert_t to_calibrator = 0;

	stopCalibration = false;

	calibrator.remainingAxesCount = 0;
	calibrator.status = CALIBRATOR_CALIBRATING;
	calibrator.result = CALIBRATION_RESULT::SUCCESS;

	to_calibrator = orb_advertise(ORB_ID(calibrator), &calibrator);

	int beeper_fd = open(TONEALARM_DEVICE_PATH, O_RDONLY);
	if (beeper_fd < 0) { // This is rather critical
		warnx("Gyro calibration could not find beeper device. Aborting.");

		calibrator.status = CALIBRATOR_FINISH;
		calibrator.result = CALIBRATION_RESULT::FAIL;

		orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

		return (false);
	}
	prepare("Gyro", beeper_fd);
	if (!check_resting_state(1000, 500, mavlink_fd, 0.1f)) {
		warnx("Vehicle is not standing still! Check accel calibration.");
		mavlink_log_critical(mavlink_fd, "Vehicle is not standing still! Check accel calibration.");
		beep(beeper_fd, TONES::NEGATIVE);
		usleep(1500000); // Allow the tune to play out
		close(beeper_fd);

		calibrator.status = CALIBRATOR_FINISH;
		calibrator.result = CALIBRATION_RESULT::FAIL;

		orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

		return (false);
	}
	printf("Parameters: samples=%d, error count=%d, timeout=%d\n", sample_count, max_error_count, timeout);
	fflush(stdout);
	beep(beeper_fd, TONES::WORKING);
	res = do_gyro_calibration(sample_count, max_error_count, timeout);
	beep(beeper_fd, TONES::STOP);
	print_results(res, "gyro", beeper_fd, mavlink_fd);
	// TODO! Consider implementing different times inside print_result
	usleep(1500000); // Allow the tune to play out
	close(beeper_fd);

	calibrator.status = CALIBRATOR_FINISH;
	calibrator.result = res;

	orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

	if (res == CALIBRATION_RESULT::SUCCESS) {
		print_scales(SENSOR_TYPE::GYRO, mavlink_fd);
		return true;
	}
	else {
		return false;
	}
}

__EXPORT bool calibrate_magnetometer(int mavlink_fd, unsigned int sample_count,
		unsigned int max_error_count,
		unsigned int total_time,
		int poll_timeout_gap) {
	CALIBRATION_RESULT res;
	int beeper_fd = open(TONEALARM_DEVICE_PATH, O_RDONLY);
	if (beeper_fd < 0) { // This is rather critical
		warnx("Mag calibration could not find beeper device. Aborting.");
		return (false);
	}
	prepare("Mag", beeper_fd);

	struct calibrator_s calibrator;
	orb_advert_t to_calibrator = 0;

	stopCalibration = false;

	calibrator.status = CALIBRATOR_CALIBRATING;
	calibrator.remainingAxesCount = 0;
	calibrator.result = CALIBRATION_RESULT::SUCCESS;

	to_calibrator = orb_advertise(ORB_ID(calibrator), &calibrator);


	res = do_mag_builtin_calibration();
	// Could possibly fail in the future if "no internal calibration" warning will be implemented
	if (res == CALIBRATION_RESULT::SUCCESS) {
		printf("Sampling magnetometer offsets. Do a full rotation around each axis.\n");
		printf("Parameters: samples=%d, max_errors=%d,\n\ttotal_time=%d ms, timeout_gap=%d ms\n",
				sample_count, max_error_count, total_time, poll_timeout_gap);
		fflush(stdout); // ensure print finishes before calibration pauses the screen
		beep(beeper_fd, TONES::WAITING_FOR_USER);
		sleep(3); // hack because we don't detect if rotation has started
		beep(beeper_fd, TONES::STOP);
		beep(beeper_fd, TONES::WORKING);

		calibrator.status = CALIBRATOR_DANCE;
		calibrator.result = CALIBRATION_RESULT::SUCCESS;

		orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

		res = do_mag_offset_calibration(sample_count, max_error_count, total_time, poll_timeout_gap);
		beep(beeper_fd, TONES::STOP);
	}
	print_results(res, "mag", beeper_fd, mavlink_fd);
	close(beeper_fd);

	calibrator.status = CALIBRATOR_FINISH;
	calibrator.result = res;

	orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

	if (res == CALIBRATION_RESULT::SUCCESS) {
		print_scales(SENSOR_TYPE::MAG, mavlink_fd);
		return true;
	}
	else {
		return false;
	}
}

__EXPORT bool calibrate_accelerometer(int mavlink_fd, bool wait_for_console) {
	CALIBRATION_RESULT res;
	const char* axis_labels[] = {
			"+x",
			"-x",
			"+y",
			"-y",
			"+z",
			"-z"
	};
	int beeper_fd = open(TONEALARM_DEVICE_PATH, O_RDONLY);
	if (beeper_fd < 0) { // This is rather critical
		warnx("Accel calibration could not find beeper device. Aborting.");
		return (false);
	}
	if (!wait_for_console) { // Skip useless waiting if we're operating from console
		prepare("Accel", beeper_fd);
	}

	struct calibrator_s calibrator;
	orb_advert_t to_calibrator = 0;

	stopCalibration = false;

	calibrator.status = CALIBRATOR_DETECTING_SIDE;
	calibrator.remainingAxesCount = 6;
	calibrator.result = CALIBRATION_RESULT::SUCCESS;

	to_calibrator = orb_advertise(ORB_ID(calibrator), &calibrator);

	AccelCalibrator calib;
	res = calib.init();
	if (res == CALIBRATION_RESULT::SUCCESS) {
		while (calib.sampling_needed && !stopCalibration) {
			int remainingAxesCount = 0;
			printf("Rotate to one of the remaining axes: ");
			for (int i = 0; i < 6; ++i) {
				if (!calib.calibrated_axes[i]) {
					fputs(axis_labels[i], stdout);
					fputs(" ", stdout);
					remainingAxesCount++;
				}
			}
			fputs("\n", stdout);
			fflush(stdout); // ensure puts finished before calibration pauses the screen
			beep(beeper_fd, TONES::WAITING_FOR_USER);

			calibrator.status = CALIBRATOR_DETECTING_SIDE;
			calibrator.remainingAxesCount = remainingAxesCount;
			calibrator.result = CALIBRATION_RESULT::SUCCESS;

			orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

			if (wait_for_console) {
				printf("------ 00: Press Space to advance\n");
				pollfd console_poll;
				console_poll.fd = fileno(stdin);
				console_poll.events = POLLIN;
				int ret = poll(&console_poll, 1, 20000);
				if (ret != 1) {
					printf("------ 20: Poll error in manual calibration. Aborting\n");
					res = CALIBRATION_RESULT::FAIL;
					break;
				}
				char in_c;
				read(fileno(stdin), &in_c, 1);
				if (in_c != ' ') {
					printf("------ 21: Aborting on user's request\n");
					res = CALIBRATION_RESULT::FAIL;
					break;
				}
				else {
					printf("------ 01: Sampling\n");
				}
				res = calib.sample_axis(100000); // Detect axis faster as we are quite sure we're standing still
			}
			else {
				res = calib.sample_axis();
			}

			beep(beeper_fd, TONES::STOP);
			if (res == CALIBRATION_RESULT::SUCCESS) {
				beep(beeper_fd, TONES::WORKING);

				calibrator.status = CALIBRATOR_CALIBRATING;
				calibrator.result = CALIBRATION_RESULT::SUCCESS;
				calibrator.remainingAxesCount = remainingAxesCount;

				orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

				res = calib.read_samples();
				beep(beeper_fd, TONES::STOP);
				if (res == CALIBRATION_RESULT::SUCCESS) {
					printf("Successfully sampled the axis.\n");
				}
				else {
					calibrator.status = CALIBRATOR_CALIBRATING;
					calibrator.result = res;
					calibrator.remainingAxesCount = remainingAxesCount;

					orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);
					break;
				}
			}
			else if (res == CALIBRATION_RESULT::AXIS_DONE_FAIL) {
				calibrator.status = CALIBRATOR_DETECTING_SIDE;
				calibrator.remainingAxesCount = remainingAxesCount;
				calibrator.result = res;

				orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

				sleep(1); // ensures the tunes don't blend too much
				beep(beeper_fd, TONES::NEGATIVE);
				printf("Axis has been sampled already.\n");
				sleep(2); // gives time for negative tune to finish
			}
			else {
				calibrator.status = CALIBRATOR_DETECTING_SIDE;
				calibrator.remainingAxesCount = remainingAxesCount;
				calibrator.result = res;

				orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

				break;
			}
		}
		if (res == CALIBRATION_RESULT::SUCCESS) {
			res = calib.calculate_and_save();
		}
	}

	calibrator.status = CALIBRATOR_FINISH;
	calibrator.result = res;

	orb_publish(ORB_ID(calibrator), to_calibrator, &calibrator);

	if (wait_for_console) {
		if (res == CALIBRATION_RESULT::SUCCESS) {
			printf("------ 02: Calibration succeeded\n");
		}
		else {
			printf("------ 22: Calibration failed\n");
		}
	}

	print_results(res, "accel", beeper_fd, mavlink_fd);
	close(beeper_fd);
	if (res == CALIBRATION_RESULT::SUCCESS) {
		print_scales(SENSOR_TYPE::ACCEL, mavlink_fd);
		return true;
	}
	else {
		return false;
	}
}

__EXPORT bool calibrate_level() {
	if (!check_resting_state(3000, 800, 0, 0.2f)) {
		warnx("Vehicle is not standing still! Check accelerometer calibration!");
		return false;
	}
	warnx("Starting level calibration!");
	CALIBRATION_RESULT res;
	res = do_calibrate_level();
	print_results(res, "level", 0, 0);
	return (res == CALIBRATION_RESULT::SUCCESS);
}

// TODO! Get rid of copy-paste with optional mavlink reporting
__EXPORT bool check_resting_state(unsigned int timeout, unsigned int minimal_time, int mavlink_fd, float threshold) {
	int fd = open(ACCEL_DEVICE_PATH, O_RDONLY);
	if (fd < 0) {
		printf("Failed to open accel to check stillness\n");
		if (mavlink_fd != 0) {
			mavlink_log_critical(mavlink_fd, "Failed to open accel to check stillness");
		}
		return false;
	}
	if (ioctl(fd, ACCELIOCSELFTEST, 0) != 0) {
		printf("Accel self test failed. Check calibration\n");
		if (mavlink_fd != 0) {
			mavlink_log_critical(mavlink_fd, "Accel self test failed. Check calibration");
		}
		close(fd);
		return false;
	}
	close (fd);
	// TODO! ^ Would become SENSOR_DATA_FAIL while timeouts -> just FAIL

	// set up the poller
	int sensor_topic = orb_subscribe(ORB_ID(sensor_combined));
	sensor_combined_s report;
	pollfd poll_data;
	poll_data.fd = sensor_topic;
	poll_data.events = POLLIN;

	int poll_res = 0;

	// Squared for comparison a > g + threshold or a < g - threshold, we get: |a^2 - g^2 - threshold^2| > 2 * g * threshold
	const float acc_diff = CONSTANTS_ONE_G * CONSTANTS_ONE_G + threshold * threshold;
	const float to_compare = 2.0f * CONSTANTS_ONE_G * threshold;

	unsigned int error_count = 0;
	hrt_abstime end_time = hrt_absolute_time() + timeout * 1000;
	minimal_time *= 1000;
	hrt_abstime still_start = 0;
	if (minimal_time == 0) {
		still_start = 1; // Allows to use the same logic as "repeated still measurement"
	}
	unsigned int max_error_count = 100;
	bool res = false;
	float tot_acc;
	while (error_count <= max_error_count && hrt_absolute_time() < end_time) {
		// poll expects an array of length 1, but single pointer will work too
		poll_res = poll(&poll_data, 1, 100);
		if (poll_res == 1) {
			if (orb_copy(ORB_ID(sensor_combined), sensor_topic, &report) == 0) {
				tot_acc = 0.0f;
				for (int i = 0; i < 3; ++i) {
					tot_acc += report.accelerometer_m_s2[i] * report.accelerometer_m_s2[i];
				}
				// Moving
				if (fabsf(tot_acc - acc_diff) > to_compare) {
					res = false;
					if (still_start != 0) {
						// printf("Not still! Acc: % 9.6f\n", (double) sqrtf(tot_acc)); // TODO! Debug output
						still_start = 0;
					}
					if (minimal_time == 0) {
						break; // Quit instantly in single sample mode
					}
				}
				// First still measurement
				else if (still_start == 0) {
					still_start = hrt_absolute_time();
				}
				// Single sample mode or repeated still measurement with enough time passed
				else if (minimal_time == 0 || (hrt_absolute_time() - still_start >= minimal_time)) {
					res = true;
					break;
				}
			}
			else {
				++error_count;
			}
		}
		else { // res == 0 - timeout, res < 0 - errors, res > 1 - most probably, corrupted memory.
			++error_count;
			printf("Kuso! Poll error! Return: %d, errno: %d, errcnt: %d\n", poll_res, errno, error_count); // TODO! Debug output. "Kuso!" in Japanese is roughly equivalent to "shit!"
		}

	}
	close(sensor_topic);

	return res;
}

inline void prepare(const char* sensor_type, const int beeper_fd) {
	beep(beeper_fd, TONES::PREPARE);
	printf("%s calibration: preparing... waiting for user.\n", sensor_type);
	sleep(3);
	beep(beeper_fd, TONES::START);
	printf("Starting %s calibration.\n", sensor_type);
	sleep(1); // give some time for the tune to play
}

// TODO we can get rid of this function when tones are finalized.
// Alternatively this function can be used to pass feedback to Mavlink
inline void beep(const int beeper_fd, TONES tone) {
	int mapped_tone;
	switch (tone) {
	case TONES::PREPARE:
		mapped_tone = TONE_NOTIFY_NEUTRAL_TUNE;
		break;
	case TONES::START:
		mapped_tone = TONE_PROCESS_START;
		break;
	case TONES::NEGATIVE:
		mapped_tone = TONE_WRONG_INPUT;
		break;
	case TONES::WAITING_FOR_USER: // should be continuous - tune string starts with MB
		mapped_tone = TONE_WAITING_INPUT;
		break;
	case TONES::FINISHED:
		mapped_tone = TONE_NOTIFY_POSITIVE_TUNE;
		break;
	case TONES::ERROR:
		mapped_tone = TONE_GENERAL_ERROR;
		break;
	case TONES::STOP: // used to stop the "WAITING_FOR_USER" tune
		mapped_tone = TONE_STOP_TUNE;
		break;
	case TONES::WORKING:
		mapped_tone = TONE_PROCESSING; // should be continuous - tune string starts with MB
		break;
	}
	// currently errors are ignored
	ioctl(beeper_fd, TONE_SET_ALARM, mapped_tone);
}

inline void print_results(CALIBRATION_RESULT res, const char* sensor_type, const int beeper_fd, int mavlink_fd) {
	const char* errors[] = { // allows to index by (error code)
			"No errors reported.\n", // code = 0 = SUCCESS
			"Calibration failed.\n", // code = 1 = FAIL
			"Failed to reset sensor scale.\n", // code = 2 = SCALE_RESET_FAIL
			"Failed to apply sensor scale.\n", // code = 3 = SCALE_APPLY_FAIL
			"Failed to get sane data from sensor.\n", // code = 4 = SENSOR_DATA_FAIL
			"Failed to save parameters to EEPROM.\n", // code = 5 = PARAMETER_DEFAULT_FAIL
			"Failed to set scaling parameters.\n", // code = 6 = PARAMETER_SET_FAIL
			"Failed to read sensor scale.\n", // code = 7 = SCALE_READ_FAIL
			"Axis has been sampled already.\n" // code = 8 = AXIS_DONE_FAIL
	};
	const size_t errors_size = sizeof(errors) / sizeof(*errors);

	printf("Calibration finished with status: %d.\n", res);
	if (res != CALIBRATION_RESULT::SUCCESS)	{
		beep(beeper_fd, TONES::ERROR);
		if ((int)res < errors_size) {
			printf(errors[(int)res]); // converts CALIBRATION_RESULT to errors index
			if (mavlink_fd != 0) {
				mavlink_log_critical(mavlink_fd, errors[(int) res]);
			}
		}
	}
	else {
		// QGround uses text matching to detect success, so don't change the message as long as we are using QGround
		printf("%s calibration: done\n", sensor_type);
		if (mavlink_fd != 0) {
			mavlink_log_info(mavlink_fd, "%s calibration: done", sensor_type);
		}
		beep(beeper_fd, TONES::FINISHED);
	}
}

inline void print_scales(SENSOR_TYPE sensor, int mavlink_fd) {
	calibration_values_s calibration;
	int dev_fd = 0;
	int ioctl_cmd = 0;

	switch (sensor) {
	case SENSOR_TYPE::GYRO:
		dev_fd = open(GYRO_DEVICE_PATH, O_RDONLY);
		ioctl_cmd = GYROIOCGSCALE;
		break;
	case SENSOR_TYPE::MAG:
		dev_fd = open(MAG_DEVICE_PATH, O_RDONLY);
		ioctl_cmd = MAGIOCGSCALE;
		break;
	case SENSOR_TYPE::ACCEL:
		dev_fd = open(ACCEL_DEVICE_PATH, O_RDONLY);
		ioctl_cmd = ACCELIOCGSCALE;
		break;
	}
	if (ioctl(dev_fd, ioctl_cmd, (unsigned long) &calibration) == 0) {
		print_calibration(calibration, mavlink_fd);
	}
	close(dev_fd);
}

} // End calibration namespace

// Execution messages
#define MSG_CALIBRATION_USAGE "Usage: %s module_name\nmodule_name is one of accel, gyro, mag, baro, airspeed, rc, all\n" \
		"Advanced mode - gyro supports 3 parameters: sample count, max error count\n" \
		"and timeout in ms (defaults: 5000, 1000, 1000)\n"
#define MSG_CALIBRATION_NOT_IMPLEMENTED "Not supported yet. Sorry.\n"
#define MSG_CALIBRATION_WRONG_MODULE "Unknown module name \"%s\". Try accel, gyro, mag, baro, airspeed, rc, all\n"
#define MSG_CALIBRATION_GYRO_WRONG_PARAM "0 or 3 parameters required.\nValid ranges for samples 1-1000000, for errors 0-5000, for timeout 2-10000.\n"
#define MSG_CALIBRATION_MAG_WRONG_PARAM "0 or 4 parameters required.\nValid ranges for samples 100-total_time/5, for errors 0-sample_count,\nfor time 1-1000000, for gap 1-100.\n"
#define MSG_CALIBRATION_ACCEL_WRONG_PARAM "Only 'manual' parameter supported.\n"

extern "C" __EXPORT int calibrator_main(int argc, char ** argv)
{
	using namespace calibration;
	char* sensname;

	if (argc < 2 || argc > 6) {
		fprintf(stderr, MSG_CALIBRATION_USAGE, argv[0]);
		return 1;
	}
	sensname = argv[1];

	if (strcmp(sensname, "accel") == 0) {
		if (argc > 3) {
			fprintf(stderr, MSG_CALIBRATION_ACCEL_WRONG_PARAM);
			return 1;
		}
		bool wait_for_console = false;
		if (argc == 3) {
			if (strcmp(argv[2], "manual") != 0) {
				fprintf(stderr, MSG_CALIBRATION_ACCEL_WRONG_PARAM);
				return 1;
			}
			wait_for_console = true;
		}
		return ((int) !calibrate_accelerometer(0, wait_for_console));
	}
	else if (strcmp(sensname,"gyro") == 0) {
		if (argc == 2) {
			return ((int) calibrate_gyroscope());
		} else if (argc == 5) {
			long samples = strtol(argv[2], nullptr, 0);
			long max_errors = strtol(argv[3], nullptr, 0);
			long timeout = strtol(argv[4], nullptr, 0);
			if (samples < 1 || samples > 1000000 ||
					timeout < 2 || timeout > 10000 ||
					max_errors < 0 || max_errors > 5000) { // sanity checks
				fprintf(stderr, MSG_CALIBRATION_GYRO_WRONG_PARAM);
				return 1;
			}
			return ((int) !calibrate_gyroscope(0, (unsigned int) samples, (unsigned int) max_errors, (int) timeout));
		}
		else {
			fprintf(stderr, MSG_CALIBRATION_GYRO_WRONG_PARAM);
			return 1;
		}
	}
	else if (strcmp(sensname,"mag") == 0) {
		long sample_count;
		long max_error_count;
		long total_time;
		long poll_timeout_gap;
		if (argc == 6) {
			sample_count = strtol(argv[2], nullptr, 0);
			max_error_count = strtol(argv[3], nullptr, 0);
			total_time = strtol(argv[4], nullptr, 0);
			poll_timeout_gap = strtol(argv[5], nullptr, 0);
			if (max_error_count < 0 || max_error_count > sample_count ||
					total_time < 1000 || total_time > 1000000 ||
					poll_timeout_gap < 1 || poll_timeout_gap > 100 ||
					sample_count < 100 || (total_time / sample_count) < 5) {
				fprintf(stderr, MSG_CALIBRATION_MAG_WRONG_PARAM);
				return 1;
			}
			return((int) !calibrate_magnetometer(0, (unsigned int) sample_count, (unsigned int) max_error_count, (unsigned int) total_time, (int) poll_timeout_gap));
		}
		else if (argc == 2) {
			return((int) !calibrate_magnetometer());
		}
		else {
			fprintf(stderr, MSG_CALIBRATION_MAG_WRONG_PARAM);
			return 1;
		}
	}
	else if (strcmp(sensname,"baro") == 0) {
		fprintf(stderr, MSG_CALIBRATION_NOT_IMPLEMENTED);
		return 1;
	}
	else if (strcmp(sensname,"rc") == 0) {
		fprintf(stderr, MSG_CALIBRATION_NOT_IMPLEMENTED);
		return 1;
	}
	else if (strcmp(sensname,"airspeed") == 0) {
		fprintf(stderr, MSG_CALIBRATION_NOT_IMPLEMENTED);
		return 1;
	}
	else if (strcmp(sensname,"all") == 0) {
		fprintf(stderr, MSG_CALIBRATION_NOT_IMPLEMENTED);
		return 1;
	}
	// TODO! TMP HACK!
	else if (strcmp(sensname,"still") == 0) {
		unsigned int timeout;
		unsigned int minimal_timeout;
		float threshold;
		timeout = strtol(argv[2], nullptr, 0);
		minimal_timeout = strtol(argv[3], nullptr, 0);
		threshold = (float) strtod(argv[4], nullptr);
		bool res = check_resting_state(timeout, minimal_timeout, 0, threshold);
		if (res) {
			printf("Yes!\n");
		}
		else {
			printf("No!\n");
		}
	}
	else if (strcmp(sensname,"level") == 0) {
		if (calibrate_level()) {
			printf("Done!\n");
		}
		else {
			printf("Fail!\n");
		}
	}
	else {
		fprintf(stderr, MSG_CALIBRATION_WRONG_MODULE, sensname);
		return 1;
	}

	return 0;
}
