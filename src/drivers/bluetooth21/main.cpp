#include <nuttx/config.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "daemon.hpp"
#include "util.hpp"

constexpr useconds_t
WAIT_PERIOD_us = 100*1000 /*us*/;

static void
usage(const char name[])
{
	fprintf(stderr, "Usage:\n"
		"\t%s start tty user listen\n"
		"\t%s           user one-connect\n"
		"\t%s           factory listen\n"
		"\t%s           factory one-connect\n"
		"\t%s status\n"
		"\t%s stop\n"
		"\n"
		, name, name, name, name, name, name
	);
}

#ifdef MODULE_COMMAND
#define XCAT(a, b)  a ## b
#define CONCAT(a, b)  XCAT(a, b)
#define main CONCAT(MODULE_COMMAND, _main)
#endif

extern "C" __EXPORT int
main(int argc, const char * argv[]);

int
main(int argc, const char * argv[])
{
	using BT::streq;
	using namespace BT::Daemon;

	if (argc >= 4 and streq(argv[1], "start"))
	{
		Main::start(argv);

		bool wait;
		do
		{
			usleep(WAIT_PERIOD_us);
			wait = Main::is_running() and not Main::has_started();
		}
		while (wait);

		if (not Main::is_running())
		{
			Main::report_status(stderr);
			return 1;
		}
	}
	else if (argc == 2 and streq(argv[1], "status"))
	{
		Main::report_status(stdout);
	}
	else if (argc == 2 and streq(argv[1], "stop"))
	{
		if (Main::is_running())
			Main::request_stop();
		else
			Main::report_status(stderr);

		while (Main::is_running())
		{
			usleep(WAIT_PERIOD_us);
			fputc('.', stderr);
			fflush(stderr);
		}
	}
	else
	{
		usage(argv[0]);
		return 1;
	}

	return 0;
}
