/*
 * Driver for the HMC5883 via SPI.
 */

#include <nuttx/config.h>

#ifndef CONFIG_SCHED_WORKQUEUE
# error This requires CONFIG_SCHED_WORKQUEUE.
#endif

extern "C" { __EXPORT int hmc5883spi_main(int argc, char *argv[]); }

#include <drivers/device/spi.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>

#include <board_config.h>

#include <systemlib/perf_counter.h>
#include <systemlib/err.h>

#include <drivers/drv_mag.h>
#include <drivers/drv_hrt.h>
#include <drivers/device/ringbuffer.h>
#include <drivers/calibration/calibration.hpp>

#include <uORB/uORB.h>
#include <uORB/topics/subsystem_info.h>

#include <float.h>
#include <getopt.h>
#include <lib/conversion/rotation.h>

#define HMC5883L_DEVICE_PATH_EXT "/dev/hmc5883_ext"

/* oddly, ERROR is not defined for c++ */
#ifndef ERROR
# define ERROR (-1)
#endif

namespace hmc5883spi
{

class xSPI : public device::SPI
{
public:
	/* SPI protocol address bits */
	static constexpr uint8_t DIR_READ       = 0x80;
	static constexpr uint8_t DIR_WRITE	= 0;
	static constexpr uint8_t ADDR_INCREMENT	= 0x40;

	xSPI(const char *name, const char *devname,
		int bus, enum spi_dev_e device, enum spi_mode_e mode,
		uint32_t frequency, int irq = 0
	) : SPI(name, devname, bus, device, mode, frequency, irq)
	{}

protected:
	int
	read_reg(unsigned reg, uint8_t &value)
	{
		uint8_t cmd[2];

		cmd[0] = reg | DIR_READ;
		cmd[1] = 0;

		int r = transfer(cmd, cmd, 2);
		value = cmd[1];
		return r;
	}

	template <size_t N>
	int read_reg_seq(unsigned reg, uint8_t (&b)[N])
	{
		uint8_t cmd[1 + N];
		memset(cmd, 0, sizeof(cmd));

		cmd[0] = reg | DIR_READ | ADDR_INCREMENT;

		int r = transfer(cmd, cmd, sizeof(cmd));
		memcpy(b, cmd + 1, N);
		return r;
	}

	int
	write_reg(unsigned reg, uint8_t value)
	{
		uint8_t	cmd[2];

		cmd[0] = reg | DIR_WRITE;
		cmd[1] = value;

		return transfer(cmd, nullptr, sizeof(cmd));
	}
};

/* Max measurement rate is 160Hz, however with 160 it will be set to 166 Hz, therefore workaround using 150 */
#define HMC5883_CONVERSION_INTERVAL	(1000000 / 150)	/* microseconds */

#define ADDR_CONF_A			0x00
#define ADDR_CONF_B			0x01
#define ADDR_MODE			0x02
#define ADDR_DATA_OUT_X_MSB		0x03
#define ADDR_DATA_OUT_X_LSB		0x04
#define ADDR_DATA_OUT_Z_MSB		0x05
#define ADDR_DATA_OUT_Z_LSB		0x06
#define ADDR_DATA_OUT_Y_MSB		0x07
#define ADDR_DATA_OUT_Y_LSB		0x08
#define ADDR_STATUS			0x09
#define ADDR_ID_A			0x0a
#define ADDR_ID_B			0x0b
#define ADDR_ID_C			0x0c

/* modes not changeable outside of driver */
#define HMC5883L_MODE_NORMAL		(0 << 0)  /* default */
#define HMC5883L_MODE_POSITIVE_BIAS	(1 << 0)  /* positive bias */
#define HMC5883L_MODE_NEGATIVE_BIAS	(1 << 1)  /* negative bias */

#define HMC5883L_AVERAGING_1		(0 << 5) /* conf a register */
#define HMC5883L_AVERAGING_2		(1 << 5)
#define HMC5883L_AVERAGING_4		(2 << 5)
#define HMC5883L_AVERAGING_8		(3 << 5)

#define MODE_REG_CONTINOUS_MODE		(0 << 0)
#define MODE_REG_SINGLE_MODE		(1 << 0) /* default */

#define STATUS_REG_DATA_OUT_LOCK	(1 << 1) /* page 16: set if data is only partially read, read device to reset */
#define STATUS_REG_DATA_READY		(1 << 0) /* page 16: set if all axes have valid measurements */

#define ID_A_WHO_AM_I			'H'
#define ID_B_WHO_AM_I			'4'
#define ID_C_WHO_AM_I			'3'


class HMC5883 : public xSPI
{
public:
	HMC5883(int bus, const char* path, spi_dev_e device, enum Rotation rotation);
	virtual ~HMC5883();

	virtual int		init();

	virtual ssize_t		read(struct file *filp, char *buffer, size_t buflen);
	virtual int		ioctl(struct file *filp, int cmd, unsigned long arg);

	/**
	 * Diagnostics - print some basic information about the driver.
	 */
	void			print_info();

protected:
	virtual int		probe();

private:
	work_s			_work;
	unsigned		_measure_ticks;

	RingBuffer		*_reports;
	mag_calibration_s		_calibration;
	float 			_range_scale;
	float 			_range_ga;
	bool			_collect_phase;
	int			_class_instance;

	orb_advert_t		_mag_topic;
	orb_advert_t		_subsystem_pub;
	orb_id_t		_mag_orb_id;

	perf_counter_t		_sample_perf;
	perf_counter_t		_comms_errors;
	perf_counter_t		_buffer_overflows;
	perf_counter_t		_range_errors;
	perf_counter_t		_conf_errors;

	/* status reporting */
	bool			_sensor_ok;		/**< sensor was found and reports ok */
	bool			_calibrated;		/**< the calibration is valid */

	int			_bus;			/**< the bus the device is connected to */
	enum Rotation		_rotation;

	struct mag_report	_last_report;           /**< used for info() */

	uint8_t			_range_bits;
	uint8_t			_conf_reg;

	/**
	 * Initialise the automatic measurement state machine and start it.
	 *
	 * @note This function is called at open and error time.  It might make sense
	 *       to make it more aggressive about resetting the bus in case of errors.
	 */
	void			start();

	/**
	 * Stop the automatic measurement state machine.
	 */
	void			stop();

	/**
	 * Reset the device
	 */
	int			reset();

	int			sample_excited(struct file *filp, float averages[3]);

	/**
	 * Perform the on-sensor scale calibration routine.
	 *
	 * @note The sensor will continue to provide measurements, these
	 *	 will however reflect excited data which does not correspond to
	 *	 any magnetic field until the calibration routine has been completed.
	 *
	 * @param filp opened file pointer to the mag device
	 */
	int			calibrate(struct file *filp);

	/**
	 * Switch on/off the artificial magnetic field emulation on the sensor
	 *
	 * @param enable set to 1 to enable self-test positive strap, -1 to enable
	 *        negative strap, 0 to set to normal mode
	 */
	int			set_excitement(int enable);

	/**
	 * Set the sensor range.
	 *
	 * Sets the internal range to handle at least the argument in Gauss.
	 */
	int 			set_range(unsigned range);

	/**
	 * check the sensor range.
	 *
	 * checks that the range of the sensor is correctly set, to
	 * cope with communication errors causing the range to change
	 */
	void 			check_range(void);

	/**
	 * check the sensor configuration.
	 *
	 * checks that the config of the sensor is correctly set, to
	 * cope with communication errors causing the configuration to
	 * change
	 */
	void 			check_conf(void);

	/**
	 * Perform a poll cycle; collect from the previous measurement
	 * and start a new one.
	 *
	 * This is the heart of the measurement state machine.  This function
	 * alternately starts a measurement, or collects the data from the
	 * previous measurement.
	 *
	 * When the interval between measurements is greater than the minimum
	 * measurement interval, a gap is inserted between collection
	 * and measurement to provide the most recent measurement possible
	 * at the next interval.
	 */
	void			cycle();

	/**
	 * Static trampoline from the workq context; because we don't have a
	 * generic workq wrapper yet.
	 *
	 * @param arg		Instance pointer for the driver that is polling.
	 */
	static void		cycle_trampoline(void *arg);

	/**
	 * Issue a measurement command.
	 *
	 * @return		OK if the measurement command was successful.
	 */
	int			measure();

	/**
	 * Collect the result of the most recent measurement.
	 */
	int			collect();

	/**
	 * Convert a big-endian signed 16-bit value to a float.
	 *
	 * @param in		A signed 16-bit big-endian value.
	 * @return		The floating-point representation of the value.
	 */
	float			meas_to_float(uint8_t in[2]);

	/**
	 * Check the current calibration and update device status
	 *
	 * @return 0 if calibration is ok, 1 else
	 */
	 int 			check_calibration();

	 /**
	 * Check the current scale calibration
	 *
	 * @return 0 if scale calibration is ok, 1 else
	 */
	 int 			check_scale();

	 /**
	 * Check the current offset calibration
	 *
	 * @return 0 if offset calibration is ok, 1 else
	 */
	 int 			check_offset();

	/* this class has pointer data members, do not allow copying it */
	HMC5883(const HMC5883&);
	HMC5883 operator=(const HMC5883&);
};

HMC5883::HMC5883(int bus, const char* path, spi_dev_e device, enum Rotation rotation) :
	xSPI("HMC5883", path, bus, device, SPIDEV_MODE3, 8000000 /* 8 MHz */),
	_work{},
	_measure_ticks(0),
	_reports(nullptr),
	_calibration{},
	_range_scale(0), /* default range scale from counts to gauss */
	_range_ga(1.3f),
	_collect_phase(false),
	_class_instance(-1),
	_mag_topic(-1),
	_subsystem_pub(-1),
	_mag_orb_id(nullptr),
	_sample_perf(perf_alloc(PC_ELAPSED, "hmc5883_read")),
	_comms_errors(perf_alloc(PC_COUNT, "hmc5883_comms_errors")),
	_buffer_overflows(perf_alloc(PC_COUNT, "hmc5883_buffer_overflows")),
	_range_errors(perf_alloc(PC_COUNT, "hmc5883_range_errors")),
	_conf_errors(perf_alloc(PC_COUNT, "hmc5883_conf_errors")),
	_sensor_ok(false),
	_calibrated(false),
	_bus(bus),
	_rotation(rotation),
	_last_report{0},
	_range_bits(0),
	_conf_reg(0x80) // Temperature compensation
{
	_device_id.devid_s.devtype = DRV_MAG_DEVTYPE_HMC5883;

	// enable debug() calls
	_debug_enabled = false;

	// work_cancel in the dtor will explode if we don't do this...
	memset(&_work, 0, sizeof(_work));
}

HMC5883::~HMC5883()
{
	/* make sure we are truly inactive */
	stop();

	if (_reports != nullptr)
		delete _reports;

	if (_class_instance != -1)
		unregister_class_devname(MAG_DEVICE_PATH, _class_instance);

	// free perf counters
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_buffer_overflows);
	perf_free(_range_errors);
	perf_free(_conf_errors);
}

int
HMC5883::init()
{
	int ret = ERROR;

	/* do SPI init (and probe) first */
	ret = xSPI::init();
	if (ret != OK) {
		fprintf(stderr, "SPI::init failed: %i %s\n", ret, strerror(-ret));
		goto out;
	}

	/* allocate basic report buffers */
	_reports = new RingBuffer(2, sizeof(mag_report));
	if (_reports == nullptr) {
		fprintf(stderr, "new RingBuffer failed\n");
		goto out;
	}

	/* reset the device configuration */
	reset();

	_class_instance = register_class_devname(MAG_DEVICE_PATH);


	switch (_class_instance) {
		case CLASS_DEVICE_PRIMARY:
			_mag_orb_id = ORB_ID(sensor_mag0);
			break;

		case CLASS_DEVICE_SECONDARY:
			_mag_orb_id = ORB_ID(sensor_mag1);
			break;

		case CLASS_DEVICE_TERTIARY:
			_mag_orb_id = ORB_ID(sensor_mag2);
			break;
	}

	ret = OK;
	/* sensor is ok, but not calibrated */
	_sensor_ok = true;
out:
	return ret;
}

int HMC5883::set_range(unsigned range)
{
	if (range < 1) {
		_range_bits = 0x00;
		_range_scale = 1.0f / 1370.0f;
		_range_ga = 0.88f;

	} else if (range <= 1) {
		_range_bits = 0x01;
		_range_scale = 1.0f / 1090.0f;
		_range_ga = 1.3f;

	} else if (range <= 2) {
		_range_bits = 0x02;
		_range_scale = 1.0f / 820.0f;
		_range_ga = 1.9f;

	} else if (range <= 3) {
		_range_bits = 0x03;
		_range_scale = 1.0f / 660.0f;
		_range_ga = 2.5f;

	} else if (range <= 4) {
		_range_bits = 0x04;
		_range_scale = 1.0f / 440.0f;
		_range_ga = 4.0f;

	} else if (range <= 4.7f) {
		_range_bits = 0x05;
		_range_scale = 1.0f / 390.0f;
		_range_ga = 4.7f;

	} else if (range <= 5.6f) {
		_range_bits = 0x06;
		_range_scale = 1.0f / 330.0f;
		_range_ga = 5.6f;

	} else {
		_range_bits = 0x07;
		_range_scale = 1.0f / 230.0f;
		_range_ga = 8.1f;
	}

	int ret;

	/*
	 * Send the command to set the range
	 */
	ret = write_reg(ADDR_CONF_B, (_range_bits << 5));

	if (OK != ret)
		perf_count(_comms_errors);

	uint8_t range_bits_in;
	ret = read_reg(ADDR_CONF_B, range_bits_in);

	if (OK != ret)
		perf_count(_comms_errors);

	return !(range_bits_in == (_range_bits << 5));
}

int
HMC5883::probe()
{
	uint8_t data[3] = {0, 0, 0};

	if (read_reg_seq(ADDR_ID_A, data))
		debug("read_reg fail");

	if ((data[0] != ID_A_WHO_AM_I) ||
	    (data[1] != ID_B_WHO_AM_I) ||
	    (data[2] != ID_C_WHO_AM_I)) {
		debug("ID byte mismatch (%02x,%02x,%02x)", data[0], data[1], data[2]);
		return -EIO;
	}

	return OK;
}

ssize_t
HMC5883::read(struct file *filp, char *buffer, size_t buflen)
{
	unsigned count = buflen / sizeof(struct mag_report);
	struct mag_report *mag_buf = reinterpret_cast<struct mag_report *>(buffer);
	int ret = 0;

	/* buffer must be large enough */
	if (count < 1)
		return -ENOSPC;

	/* if automatic measurement is enabled */
	if (_measure_ticks > 0) {
		/*
		 * While there is space in the caller's buffer, and reports, copy them.
		 * Note that we may be pre-empted by the workq thread while we are doing this;
		 * we are careful to avoid racing with them.
		 */
		while (count--) {
			if (_reports->get(mag_buf)) {
				ret += sizeof(struct mag_report);
				mag_buf++;
			}
		}

		/* if there was no data, warn the caller */
		return ret ? ret : -EAGAIN;
	}

	/* manual measurement - run one conversion */
	/* XXX really it'd be nice to lock against other readers here */
	do {
		_reports->flush();

		/* trigger a measurement */
		if (OK != measure()) {
			ret = -EIO;
			break;
		}

		/* wait for it to complete */
		usleep(HMC5883_CONVERSION_INTERVAL);

		/* run the collection phase */
		if (OK != collect()) {
			ret = -EIO;
			break;
		}

		if (_reports->get(mag_buf)) {
			ret = sizeof(struct mag_report);
		}
	} while (0);

	return ret;
}

int
HMC5883::ioctl(struct file *filp, int cmd, unsigned long arg)
{
	switch (cmd) {
	case SENSORIOCSPOLLRATE: {
		switch (arg) {

			/* switching to manual polling */
		case SENSOR_POLLRATE_MANUAL:
			stop();
			_measure_ticks = 0;
			return OK;

			/* external signalling (DRDY) not supported */
		case SENSOR_POLLRATE_EXTERNAL:

			/* zero would be bad */
		case 0:
			return -EINVAL;

			/* set default/max polling rate */
		case SENSOR_POLLRATE_MAX:
		case SENSOR_POLLRATE_DEFAULT: {
				/* do we need to start internal polling? */
				bool want_start = (_measure_ticks == 0);

				/* set interval for next measurement to minimum legal value */
				_measure_ticks = USEC2TICK(HMC5883_CONVERSION_INTERVAL);

				/* if we need to start the poll state machine, do it */
				if (want_start)
					start();

				return OK;
			}

			/* adjust to a legal polling interval in Hz */
		default: {
				/* do we need to start internal polling? */
				bool want_start = (_measure_ticks == 0);

				/* convert hz to tick interval via microseconds */
				unsigned ticks = USEC2TICK(1000000 / arg);

				/* check against maximum rate */
				if (ticks < USEC2TICK(HMC5883_CONVERSION_INTERVAL))
					return -EINVAL;

				/* update interval for next measurement */
				_measure_ticks = ticks;

				/* if we need to start the poll state machine, do it */
				if (want_start)
					start();

				return OK;
			}
		}
	}

	case SENSORIOCGPOLLRATE:
		if (_measure_ticks == 0)
			return SENSOR_POLLRATE_MANUAL;

		return 1000000/TICK2USEC(_measure_ticks);

	case SENSORIOCSQUEUEDEPTH: {
			/* lower bound is mandatory, upper bound is a sanity check */
			if ((arg < 1) || (arg > 100))
				return -EINVAL;

			irqstate_t flags = irqsave();
			if (!_reports->resize(arg)) {
				irqrestore(flags);
				return -ENOMEM;
			}
			irqrestore(flags);

			return OK;
		}

	case SENSORIOCGQUEUEDEPTH:
		return _reports->size();

	case SENSORIOCRESET:
		return reset();

	case MAGIOCSSAMPLERATE:
		/* same as pollrate because device is in single measurement mode*/
		return ioctl(filp, SENSORIOCSPOLLRATE, arg);

	case MAGIOCGSAMPLERATE:
		/* same as pollrate because device is in single measurement mode*/
		return 1000000/TICK2USEC(_measure_ticks);

	case MAGIOCSRANGE:
		return set_range(arg);

	case MAGIOCGRANGE:
		return _range_ga;

	case MAGIOCSLOWPASS:
	case MAGIOCGLOWPASS:
		/* not supported, no internal filtering */
		return -EINVAL;

	case MAGIOCSSCALE:
		/* set new scale factors */
		// memcpy(&_scale, (mag_scale *)arg, sizeof(_scale));
		_calibration = *((mag_calibration_s *) arg);
		/* check calibration, but not actually return an error */
		(void)check_calibration();
		return 0;

	case MAGIOCGSCALE:
		/* copy out scale factors */
		// memcpy((mag_scale *)arg, &_scale, sizeof(_scale));
		*((mag_calibration_s *) arg) = _calibration;
		return 0;

	case MAGIOCCALIBRATE:
		return calibrate(filp);

	case MAGIOCEXSTRAP:
		return set_excitement((int) arg);

	case MAGIOCSELFTEST:
		return check_calibration();

	case MAGIOCGEXTERNAL:
		return 1;

	default:
		/* give it to the superclass */
		return xSPI::ioctl(filp, cmd, arg);
	}
}

void
HMC5883::start()
{
	/* reset the report ring and state machine */
	_collect_phase = false;
	_reports->flush();

	/* schedule a cycle to start things */
	work_queue(HPWORK, &_work, (worker_t)&HMC5883::cycle_trampoline, this, 1);
}

void
HMC5883::stop()
{
	work_cancel(HPWORK, &_work);
}

int
HMC5883::reset()
{
	write_reg(ADDR_CONF_A, _conf_reg);
	return set_range(_range_ga);
}

void
HMC5883::cycle_trampoline(void *arg)
{
	HMC5883 *dev = (HMC5883 *)arg;

	dev->cycle();
}

void
HMC5883::cycle()
{
	/* collection phase? */
	if (_collect_phase) {

		/* perform collection */
		if (OK != collect()) {
			debug("collection error");
			/* restart the measurement state machine */
			start();
			return;
		}

		/* next phase is measurement */
		_collect_phase = false;

		/*
		 * Is there a collect->measure gap?
		 */
		if (_measure_ticks > USEC2TICK(HMC5883_CONVERSION_INTERVAL)) {

			/* schedule a fresh cycle call when we are ready to measure again */
			work_queue(HPWORK,
				   &_work,
				   (worker_t)&HMC5883::cycle_trampoline,
				   this,
				   _measure_ticks - USEC2TICK(HMC5883_CONVERSION_INTERVAL));

			return;
		}
	}

	/* measurement phase */
	if (OK != measure())
		debug("measure error");

	/* next phase is collection */
	_collect_phase = true;

	/* schedule a fresh cycle call when the measurement is done */
	work_queue(HPWORK,
		   &_work,
		   (worker_t)&HMC5883::cycle_trampoline,
		   this,
		   USEC2TICK(HMC5883_CONVERSION_INTERVAL));
}

int
HMC5883::measure()
{
	int ret;

	/*
	 * Send the command to begin a measurement.
	 */
	ret = write_reg(ADDR_MODE, MODE_REG_SINGLE_MODE);

	if (OK != ret)
		perf_count(_comms_errors);

	return ret;
}

int
HMC5883::collect()
{
	uint8_t	hmc_report[6];
	struct { int16_t x, y, z; } report;

	int	ret;
	uint8_t	cmd;
	uint8_t check_counter;

	perf_begin(_sample_perf);
	struct mag_report new_report;

	/* this should be fairly close to the end of the measurement, so the best approximation of the time */
	new_report.timestamp = hrt_absolute_time();
        new_report.error_count = perf_event_count(_comms_errors);

	/*
	 * @note  We could read the status register here, which could tell us that
	 *        we were too early and that the output registers are still being
	 *        written.  In the common case that would just slow us down, and
	 *        we're better off just never being early.
	 */

	/* get measurements from the device */
	cmd = ADDR_DATA_OUT_X_MSB;
	ret = read_reg_seq(cmd, hmc_report);

	if (ret != OK) {
		perf_count(_comms_errors);
		debug("data/status read error");
		goto out;
	}

	/* swap the data we just received */
	report.x = (((int16_t)hmc_report[0]) << 8) + hmc_report[1];
	report.z = (((int16_t)hmc_report[2]) << 8) + hmc_report[3];
	report.y = (((int16_t)hmc_report[4]) << 8) + hmc_report[5];

	/*
	 * If any of the values are -4096, there was an internal math error in the sensor.
	 * Generalise this to a simple range check that will also catch some bit errors.
	 */
	if ((abs(report.x) > 2048) ||
	    (abs(report.y) > 2048) ||
	    (abs(report.z) > 2048)) {
		perf_count(_comms_errors);
		goto out;
	}

	/*
	 * RAW outputs
	 * Don't align shit, just report raw data
	 */
	new_report.x_raw = report.x;
	new_report.y_raw = report.y;
	new_report.z_raw = report.z;

	{
		/* the standard external mag by 3DR has x pointing to the
		 * right, y pointing backwards, and z down, therefore switch x
		 * and y and invert y */
		auto raw = report;
		report.x = -raw.y;
		report.y = raw.x;
		report.z = raw.z;
	}

	// range, offset and scale
	new_report.x = ((report.x * _range_scale) - _calibration.offsets(0)) * _calibration.scales(0);
	new_report.y = ((report.y * _range_scale) - _calibration.offsets(1)) * _calibration.scales(1);
	new_report.z = ((report.z * _range_scale) - _calibration.offsets(2)) * _calibration.scales(2);

	// apply user specified rotation
	rotate_3f(_rotation, new_report.x, new_report.y, new_report.z);

	if (!(_pub_blocked)) {

		if (_mag_topic != -1) {
			/* publish it */
			orb_publish(_mag_orb_id, _mag_topic, &new_report);
		} else {
			_mag_topic = orb_advertise(_mag_orb_id, &new_report);

			if (_mag_topic < 0)
				debug("ADVERT FAIL");
		}
	}

	_last_report = new_report;

	/* post a report to the ring */
	if (_reports->force(&new_report)) {
		perf_count(_buffer_overflows);
	}

	/* notify anyone waiting for data */
	poll_notify(POLLIN);

	/*
	  periodically check the range register and configuration
	  registers. With a bad I2C cable it is possible for the
	  registers to become corrupt, leading to bad readings. It
	  doesn't happen often, but given the poor cables some
	  vehicles have it is worth checking for.
	 */
	check_counter = perf_event_count(_sample_perf) % 256;
	if (check_counter == 0) {
		check_range();
	}
	if (check_counter == 128) {
		check_conf();
	}

	ret = OK;

out:
	perf_end(_sample_perf);
	return ret;
}

int HMC5883::sample_excited(struct file * filp, float averages[3]) {

	int fd = ::open(HMC5883L_DEVICE_PATH_EXT, O_RDONLY);
	int res = OK, ret;
	int good_count = 0;
	struct mag_report report;
	float sum_excited[3] = {0.0f, 0.0f, 0.0f};
	float low_bound = 0.62208, high_bound = 1.472;

	// discard 10 samples to let the sensor settle
	for (uint8_t i = 0; i < 10; i++) {
		struct pollfd fds;

		/* wait for data to be ready */
		fds.fd = fd;
		fds.events = POLLIN;
		ret = ::poll(&fds, 1, 2000);

		if (ret != 1) {
			warn("timed out waiting for sensor data");
			if (ret == OK) {
				res = -1;
			}
			else {
				res = ret;
			}
			break;
		}

		/* now go get it */
		ret = read(filp, (char*) (&report), sizeof(report));

		if (ret != sizeof(report)) {
			warn("periodic read failed");
			res = -EIO;
			break;
		}
	}

	if (res == OK) {
		/* read the sensor up to 50x, stopping when we have 10 good values */
		for (uint8_t i = 0; i < 50 && good_count < 10; i++) {
			struct pollfd fds;

			/* wait for data to be ready */
			fds.fd = fd;
			fds.events = POLLIN;
			ret = ::poll(&fds, 1, 2000);

			if (ret != 1) {
				warn("timed out waiting for sensor data");
				if (ret == OK) {
					res = -1;
				}
				else {
					res = ret;
				}
				break;
			}

			/* now go get it */
			ret = read(filp, (char*) (&report), sizeof(report));

			if (ret != sizeof(report)) {
				warn("periodic read failed");
				res = -EIO;
				break;
			}

			if (fabsf(report.x) > low_bound && fabsf(report.x) < high_bound
					&& fabsf(report.y) > low_bound && fabsf(report.y) < high_bound
					&& fabsf(report.z) > low_bound && fabsf(report.z) < high_bound) {
				good_count++;
				sum_excited[0] += report.x;
				sum_excited[1] += report.y;
				sum_excited[2] += report.z;
			}

			//warnx("periodic read %u", i);
			//warnx("measurement: %.6f  %.6f  %.6f", (double)report.x, (double)report.y, (double)report.z);
		}

		if (res == OK) {
			if (good_count < 5) {
				warn("failed calibration, too few good positive samples");
				res = -EIO;
			}
			else {
				for (uint8_t i = 0; i < 3; ++i) {
					averages[i] = sum_excited[i] / good_count;
				}
			}
		}
	}
	return res;
}

int HMC5883::calibrate(struct file *filp)
{
	int prev_rate = -1, prev_range = -1;
	int ret = 1;

	mag_calibration_s calib_previous;
	mag_calibration_s calib_null;

	/*
	 * According to the data sheet X and Y should contain 1.16 and
	 * Z should contain 1.08. But, possibly, it's an error
	 * as axis order in the register is: X, Z, Y and Y contains 1.08 instead.
	 * BUT after all the flipping-tripping magic in the collect function, X ends up
	 * having -Y values, Z = Z and Y = X. Thus, we should use X = -1.08, Z = 1.16
	 * and Y = 1.16 values as reference... and pray it works.
	 */
	float expected_cal_x2[3] = { -1.08f * 2.0f, 1.16f * 2.0f, 1.16f * 2.0f };
	float avg_positive[3], avg_negative[3];


	warnx("starting mag scale calibration");

	prev_rate = ioctl(filp, SENSORIOCGPOLLRATE, 0);
	if (prev_rate <= 0) {
		warn("failed to get previous poll rate");
		ret = 1;
		goto out;
	}

	/* start the sensor polling at 50 Hz */
	if (OK != ioctl(filp, SENSORIOCSPOLLRATE, 50)) {
		warn("failed to set 50Hz poll rate");
		ret = 1;
		goto out;
	}

	prev_range = ioctl(filp, MAGIOCGRANGE, 0);
	if (prev_range <= 0) {
		warn("failed to get previous mag range");
		ret = 1;
		goto out;
	}

	/* Set to 2.5 Gauss. We ask for 3 to get the right part of
         * the chained if statement above. */
	if (OK != ioctl(filp, MAGIOCSRANGE, 3)) {
		warnx("failed to set 2.5 Ga range");
		ret = 1;
		goto out;
	}

	if (OK != ioctl(filp, MAGIOCEXSTRAP, 1)) {
		warnx("failed to enable sensor positive excitement mode");
		ret = 1;
		goto out;
	}

	if (OK != ioctl(filp, MAGIOCGSCALE, (long unsigned int)&calib_previous)) {
		warn("WARNING: failed to get scale / offsets for mag");
		ret = 1;
		goto out;
	}

	if (OK != ioctl(filp, MAGIOCSSCALE, (long unsigned int)&calib_null)) {
		warn("WARNING: failed to set null scale / offsets for mag");
		ret = 1;
		goto out;
	}

	ret = sample_excited(filp, avg_positive);
	if (ret != OK) {
		warnx("Failed positive excitement sampling");
		goto out;
	}

	if (OK != ioctl(filp, MAGIOCEXSTRAP, (unsigned long) -1)) {
		warnx("failed to enable sensor negative excitement mode");
		ret = 1;
		goto out;
	}

	ret = sample_excited(filp, avg_negative);
	if (ret != OK) {
		warnx("Failed negative excitement sampling");
		goto out;
	}

#if 0
	warnx("measurement avg: %.6f  %.6f  %.6f",
	      (double)sum_excited[0]/good_count,
	      (double)sum_excited[1]/good_count,
	      (double)sum_excited[2]/good_count);
#endif

	// set scaling on the device
	for (uint8_t i = 0; i < 3; ++i) {
		calib_previous.scales(i) = fabsf(expected_cal_x2[i] / (avg_positive[i] - avg_negative[i]));
	}

	//warnx("axes scaling: %.6f  %.6f  %.6f", (double)calib_previous.scales(0), (double)calib_previous.scales(1), (double)calib_previous.scales(2));
	ret = OK;

out:

	if (OK != ioctl(filp, MAGIOCSSCALE, (long unsigned int)&calib_previous)) {
		warn("failed to set new scale / offsets for mag");
	}

	/* set back to normal mode */
	if (prev_rate > 0 && OK != ioctl(filp, SENSORIOCSPOLLRATE, prev_rate)) {
		warnx("failed to restore mag poll rate");
	}
	if (prev_range > 0 && OK != ioctl(filp, MAGIOCSRANGE, prev_range)) {
		warnx("failed to restore mag range");
	}
	if (OK != ioctl(filp, MAGIOCEXSTRAP, 0)) {
		warnx("failed to disable sensor calibration mode");
	}

	if (ret == OK) {
		if (!check_scale()) {
			warnx("mag scale calibration successfully finished.");
		} else {
			warnx("mag scale calibration finished with invalid results.");
			ret = ERROR;
		}

	} else {
		warnx("mag scale calibration failed.");
	}

	return ret;
}

int HMC5883::check_scale()
{
	bool scale_valid;

	if ((-FLT_EPSILON + 1.0f < _calibration.scales(0) && _calibration.scales(0) < FLT_EPSILON + 1.0f) &&
		(-FLT_EPSILON + 1.0f < _calibration.scales(1) && _calibration.scales(1) < FLT_EPSILON + 1.0f) &&
		(-FLT_EPSILON + 1.0f < _calibration.scales(2) && _calibration.scales(2) < FLT_EPSILON + 1.0f)) {
		/* scale is one */
		scale_valid = false;
	} else {
		scale_valid = true;
	}

	/* return 0 if calibrated, 1 else */
	return !scale_valid;
}

int HMC5883::check_offset()
{
	bool offset_valid;

	if ((-2.0f * FLT_EPSILON < _calibration.offsets(0) && _calibration.offsets(0) < 2.0f * FLT_EPSILON) &&
		(-2.0f * FLT_EPSILON < _calibration.offsets(1) && _calibration.offsets(1) < 2.0f * FLT_EPSILON) &&
		(-2.0f * FLT_EPSILON < _calibration.offsets(2) && _calibration.offsets(2) < 2.0f * FLT_EPSILON)) {
		/* offset is zero */
		offset_valid = false;
	} else {
		offset_valid = true;
	}

	/* return 0 if calibrated, 1 else */
	return !offset_valid;
}

int HMC5883::check_calibration()
{
	bool offset_valid = (check_offset() == OK);
	bool scale_valid  = (check_scale() == OK);

	if (_calibrated != (offset_valid && scale_valid)) {
		warnx("mag cal status changed %s%s", (scale_valid) ? "" : "scale invalid ",
					  (offset_valid) ? "" : "offset invalid");
		_calibrated = (offset_valid && scale_valid);


		// XXX Change advertisement

		/* notify about state change */
		struct subsystem_info_s info = {
			true,
			true,
			_calibrated,
			SUBSYSTEM_TYPE_MAG};

		if (!(_pub_blocked)) {
			if (_subsystem_pub > 0) {
				orb_publish(ORB_ID(subsystem_info), _subsystem_pub, &info);
			} else {
				_subsystem_pub = orb_advertise(ORB_ID(subsystem_info), &info);
			}
		}
	}

	/* return 0 if calibrated, 1 else */
	return (!_calibrated);
}

int HMC5883::set_excitement(int enable)
{
	int ret;
	/* arm the excitement strap */
	ret = read_reg(ADDR_CONF_A, _conf_reg);

	if (OK != ret)
		perf_count(_comms_errors);

	_conf_reg &= ~0x03; // no bias by default
	if (enable > 0) {
		_conf_reg |= 0x01; // positive bias
	} else if (enable != 0) {
		_conf_reg |= 0x02; // negative bias
	}

        // ::printf("set_excitement enable=%d regA=0x%x\n", (int)enable, (unsigned)_conf_reg);

	ret = write_reg(ADDR_CONF_A, _conf_reg);

	if (OK != ret)
		perf_count(_comms_errors);

	uint8_t conf_reg_ret;
	read_reg(ADDR_CONF_A, conf_reg_ret);

	//print_info();

	return !(_conf_reg == conf_reg_ret);
}

float
HMC5883::meas_to_float(uint8_t in[2])
{
	union {
		uint8_t	b[2];
		int16_t	w;
	} u;

	u.b[0] = in[1];
	u.b[1] = in[0];

	return (float) u.w;
}

void
HMC5883::print_info()
{
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_buffer_overflows);
	printf("poll interval:  %u ticks\n", _measure_ticks);
	printf("output  (%.2f %.2f %.2f)\n", (double)_last_report.x, (double)_last_report.y, (double)_last_report.z);
	printf("offsets (%.2f %.2f %.2f)\n", (double)_calibration.offsets(0), (double)_calibration.offsets(1), (double)_calibration.offsets(2));
	printf("scaling (%.2f %.2f %.2f) 1/range_scale %.2f range_ga %.2f\n",
	       (double)_calibration.scales(0), (double)_calibration.scales(1), (double)_calibration.scales(2),
	       (double)(1.0f/_range_scale), (double)_range_ga);
	_reports->print_info("report queue");
}

// ------------ Inherited from I2C ------------

/**
   check that the range register has the right value. This is done
   periodically to cope with I2C bus noise causing the range of the
   compass changing.
 */
void HMC5883::check_range(void)
{
	int ret;

	uint8_t range_bits_in;
	ret = read_reg(ADDR_CONF_B, range_bits_in);
	if (OK != ret) {
		perf_count(_comms_errors);
		return;
	}
	if (range_bits_in != (_range_bits<<5)) {
		perf_count(_range_errors);
		ret = write_reg(ADDR_CONF_B, (_range_bits << 5));
		if (OK != ret)
			perf_count(_comms_errors);
	}
}

/**
   check that the configuration register has the right value. This is
   done periodically to cope with I2C bus noise causing the
   configuration of the compass to change.
 */
void HMC5883::check_conf(void)
{
	int ret;

	uint8_t conf_reg_in;
	ret = read_reg(ADDR_CONF_A, conf_reg_in);
	if (OK != ret) {
		perf_count(_comms_errors);
		return;
	}
	if (conf_reg_in != _conf_reg) {
		perf_count(_conf_errors);
		ret = write_reg(ADDR_CONF_A, _conf_reg);
		if (OK != ret)
			perf_count(_comms_errors);
	}
}

// ------------ Local functions ------------

HMC5883	*g_dev = nullptr;

void	start(int bus, spi_dev_e spi_dev, enum Rotation rotation);
void	test(int bus);
void	reset(int bus);
void	info(int bus);
int	calibrate(int bus);
void	usage();

/**
 * Start the driver.
 *
 * This function call only returns once the driver
 * is either successfully up and running or failed to start.
 */
void
start(int bus, spi_dev_e spi_dev, enum Rotation rotation)
{
	int fd, r;

	if (g_dev != nullptr)
		errx(0, "already started external");

	g_dev = new HMC5883(bus, HMC5883L_DEVICE_PATH_EXT, spi_dev, rotation);
	if (g_dev != nullptr && OK != g_dev->init())
		goto fail;

	fd = open(HMC5883L_DEVICE_PATH_EXT, O_RDONLY);
	if (fd < 0)
		goto fail;

	r = ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT);

	close(fd);

	if (r < 0)
		goto fail;

	exit(0);
	return;

fail:
	perror("start");

	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;
	}

	errx(1, "HMC5883 SPI driver start failed");
}

/**
 * Perform some basic functional tests on the driver;
 * make sure we can collect data from the sensor in polled
 * and automatic modes.
 */
void
test(int bus)
{
	struct mag_report report;
	ssize_t sz;
	int ret;
	const char *path = HMC5883L_DEVICE_PATH_EXT;

	int fd = open(path, O_RDONLY);

	if (fd < 0)
		err(1, "%s open failed (try 'hmc5883 start')", path);

	/* do a simple demand read */
	sz = read(fd, &report, sizeof(report));

	if (sz != sizeof(report))
		err(1, "immediate read failed");

	warnx("single read");
	warnx("measurement: %.6f  %.6f  %.6f", (double)report.x, (double)report.y, (double)report.z);
	warnx("time:        %lld", report.timestamp);

	/* check if mag is onboard or external */
	if ((ret = ioctl(fd, MAGIOCGEXTERNAL, 0)) < 0)
		errx(1, "failed to get if mag is onboard or external");
	warnx("device active: %s", ret ? "external" : "onboard");

	/* set the queue depth to 5 */
	if (OK != ioctl(fd, SENSORIOCSQUEUEDEPTH, 10))
		errx(1, "failed to set queue depth");

	/* start the sensor polling at 2Hz */
	if (OK != ioctl(fd, SENSORIOCSPOLLRATE, 2))
		errx(1, "failed to set 2Hz poll rate");

	/* read the sensor 5x and report each value */
	for (unsigned i = 0; i < 5; i++) {
		struct pollfd fds;

		/* wait for data to be ready */
		fds.fd = fd;
		fds.events = POLLIN;
		ret = poll(&fds, 1, 2000);

		if (ret != 1)
			errx(1, "timed out waiting for sensor data");

		/* now go get it */
		sz = read(fd, &report, sizeof(report));

		if (sz != sizeof(report))
			err(1, "periodic read failed");

		warnx("periodic read %u", i);
		warnx("measurement: %.6f  %.6f  %.6f", (double)report.x, (double)report.y, (double)report.z);
		warnx("time:        %lld", report.timestamp);
	}

	errx(0, "PASS");
}


/**
 * Automatic scale calibration.
 *
 * Basic idea:
 *
 *   output = (ext field +- 1.1 Ga self-test) * scale factor
 *
 * and consequently:
 *
 *   1.1 Ga = (excited - normal) * scale factor
 *   scale factor = (excited - normal) / 1.1 Ga
 *
 *   sxy = (excited - normal) / 766	| for conf reg. B set to 0x60 / Gain = 3
 *   sz  = (excited - normal) / 713	| for conf reg. B set to 0x60 / Gain = 3
 *
 * By subtracting the non-excited measurement the pure 1.1 Ga reading
 * can be extracted and the sensitivity of all axes can be matched.
 *
 * SELF TEST OPERATION
 * To check the HMC5883L for proper operation, a self test feature in incorporated
 * in which the sensor offset straps are excited to create a nominal field strength
 * (bias field) to be measured. To implement self test, the least significant bits
 * (MS1 and MS0) of configuration register A are changed from 00 to 01 (positive bias)
 * or 10 (negetive bias), e.g. 0x11 or 0x12.
 * Then, by placing the mode register into single-measurement mode (0x01),
 * two data acquisition cycles will be made on each magnetic vector.
 * The first acquisition will be a set pulse followed shortly by measurement
 * data of the external field. The second acquisition will have the offset strap
 * excited (about 10 mA) in the positive bias mode for X, Y, and Z axes to create
 * about a Â±1.1 gauss self test field plus the external field. The first acquisition
 * values will be subtracted from the second acquisition, and the net measurement
 * will be placed into the data output registers.
 * Since self test adds ~1.1 Gauss additional field to the existing field strength,
 * using a reduced gain setting prevents sensor from being saturated and data registers
 * overflowed. For example, if the configuration register B is set to 0x60 (Gain=3),
 * values around +766 LSB (1.16 Ga * 660 LSB/Ga) will be placed in the X and Y data
 * output registers and around +713 (1.08 Ga * 660 LSB/Ga) will be placed in Z data
 * output register. To leave the self test mode, change MS1 and MS0 bit of the
 * configuration register A back to 00 (Normal Measurement Mode), e.g. 0x10.
 * Using the self test method described above, the user can scale sensor
 */
int calibrate(int bus)
{
	int ret;
	const char *path = HMC5883L_DEVICE_PATH_EXT;

	int fd = open(path, O_RDONLY);

	if (fd < 0)
		err(1, "%s open failed (try 'start' the driver", path);

	if (OK != (ret = ioctl(fd, MAGIOCCALIBRATE, fd))) {
		warnx("failed to enable sensor calibration mode");
	}

	close(fd);

	if (ret == OK) {
		errx(0, "PASS");

	} else {
		errx(1, "FAIL");
	}
}

/**
 * Reset the driver.
 */
void
reset(int bus)
{
	const char *path = HMC5883L_DEVICE_PATH_EXT;

	int fd = open(path, O_RDONLY);

	if (fd < 0)
		err(1, "failed ");

	if (ioctl(fd, SENSORIOCRESET, 0) < 0)
		err(1, "driver reset failed");

	if (ioctl(fd, SENSORIOCSPOLLRATE, SENSOR_POLLRATE_DEFAULT) < 0)
		err(1, "driver poll restart failed");

	exit(0);
}

/**
 * Print a little info about the driver.
 */
void
info(int bus)
{
	if (g_dev == nullptr)
		errx(1, "driver not running");

	printf("state @ %p\n", g_dev);
	g_dev->print_info();

	exit(0);
}

void
usage()
{
	warnx("missing command: try 'start', 'info', 'test', 'reset', 'info', 'calibrate'");
	warnx("options:");
	warnx("    -R rotation");
	warnx("    -C calibrate on start");
}

} // namespace hmc5883spi

int
hmc5883spi_main(int argc, char *argv[])
{
	using namespace hmc5883spi;

	int ch;
	const int bus = SPI_HMC5883_BUS;
	const spi_dev_e spi_dev = (spi_dev_e)SPI_HMC5883_DEV;

	enum Rotation rotation = ROTATION_NONE;
        bool do_calibrate = false;

	while ((ch = getopt(argc, argv, "R:C")) != EOF) {
		switch (ch) {
		case 'R':
			rotation = (enum Rotation)atoi(optarg);
			break;
		case 'C':
			do_calibrate = true;
			break;
		default:
			usage();
			exit(0);
		}
	}

	const char *verb = argv[optind];

	/*
	 * Start/load the driver.
	 */
	if (!strcmp(verb, "start")) {
		start(bus, spi_dev, rotation);
		if (do_calibrate) {
			if (calibrate(bus) == 0) {
				errx(0, "calibration successful");

			} else {
				errx(1, "calibration failed");
			}
		}
	}

	/*
	 * Test the driver/device.
	 */
	if (!strcmp(verb, "test"))
		test(bus);

	/*
	 * Reset the driver.
	 */
	if (!strcmp(verb, "reset"))
		reset(bus);

	/*
	 * Print driver information.
	 */
	if (!strcmp(verb, "info") || !strcmp(verb, "status"))
		info(bus);

	/*
	 * Autocalibrate the scaling
	 */
	if (!strcmp(verb, "calibrate")) {
		if (calibrate(bus) == 0) {
			errx(0, "calibration successful");

		} else {
			errx(1, "calibration failed");
		}
	}

	errx(1, "unrecognized command, try 'start', 'test', 'reset' 'calibrate' or 'info'");
}
