/*
 * Copyright (c) 2015 The Henrik Schondorff
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>

#include "comm-host.h"
#include "gec_lock.h"
#include "misc_util.h"

#include "cb_hddledd.h"



//#define SIMULATION		// debugging, dont talk to EC


typedef void (*logf_t) (const char * format, ...);
typedef int (*ledf_t) (int interface, hddled_color color);

void log_printf(const char * format, ...);
void log_syslog(const char * format, ...);
int init_ec();
int close_ec();



// char pgm_version[20];
logf_t log_ = log_printf;
int exit_daemon = 0;

void sig_handler(int signal) {
	exit_daemon = 1;
}

static void start_daemon (const char *log_name, int facility) {
	int i;
	pid_t pid;

	// kill parent
	if ((pid = fork ()) != 0)
		exit (EXIT_FAILURE);

	// takeover session
	if (setsid() < 0) {
		printf("%s cant take over session!\n", log_name);
		exit (EXIT_FAILURE);
	}

	// igone SIGHUP signal */
	signal(SIGHUP, SIG_IGN);
	/* Das Kind terminieren */
	if ((pid = fork ()) != 0)
		exit (EXIT_FAILURE);

	// use dummy working dir
	chdir ("/");

	// dont inherit umask from parent
	umask (0);

	// close all open file descriptors
	for (i = sysconf (_SC_OPEN_MAX); i > 0; i--)
		close (i);

	// open syslog
	openlog (log_name, LOG_PID | LOG_CONS| LOG_NDELAY, facility);
}

int set_led_cmd(int interface, hddled_color color)
{
	int rv = -1;
	struct ec_params_led_control p;
	struct ec_response_led_control r;

	//printf("set hddled to '%d'\n", color);

	memset(p.brightness, 0, sizeof(p.brightness));
	p.flags = 0;
	p.led_id = LEDTYPE_POWER;

	switch(color)
	{
		case HDDLED_BLUE:
			p.brightness[LEDCOLOR_BLUE] = 0xff;
			break;

		case HDDLED_ORANGE:
			p.brightness[LEDCOLOR_YELLOW] = 0xff;
			break;

		case HDDLED_OFF:
			/* Brightness initialized to 0 for each color. */
			break;

		default:
		case HDDLED_AUTO:
			p.flags = EC_LED_FLAGS_AUTO;
			break;
	}
#ifdef SIMULATION
	return 0;
#endif
	if(init_ec(interface) == 0) {
		rv = ec_command(EC_CMD_LED_CONTROL, 1, &p, sizeof(p), &r, sizeof(r));
		close_ec();
	}
	return (rv < 0 ? rv : 0);
}

int set_led_gpio(int interface, hddled_color color)
{
	int gpio0 = 0;
	int gpio1 = 0;
	struct ec_params_gpio_set p;
	int rv = -1;

	switch((int)color)
	{
		case HDDLED_BLUE:
			gpio0 = 0;
			gpio1 = 1;
			break;

		case HDDLED_ORANGE:
			gpio0 = 1;
			gpio1 = 0;
			break;

		case HDDLED_OFF:
			gpio0 = 1;
			gpio1 = 1;
			break;
	}
#ifndef SIMULATION
	if(gpio0|gpio1)
	{
		if(init_ec(interface) == 0) {
			strcpy(p.name, PWR_LED0_NAME);
			p.val = gpio0;
			rv = ec_command(EC_CMD_GPIO_SET, 0, &p, sizeof(p), NULL, 0);

			strcpy(p.name, PWR_LED1_NAME);
			p.val = gpio1;
			rv |=  ec_command(EC_CMD_GPIO_SET, 0, &p, sizeof(p), NULL, 0);
			close_ec();
		}
	}
#endif
	return rv;
}

int cmd_gpio_get (char *name)
{
	struct ec_params_gpio_get_v1 p_v1;
	struct ec_response_gpio_get_v1 r_v1;
	int rv;
	int cmdver = 1;

	if (!ec_cmd_version_supported(EC_CMD_GPIO_GET, cmdver)) {
		struct ec_params_gpio_get p;
		struct ec_response_gpio_get r;

		/* Fall back to version 0 command */
		cmdver = 0;

		strcpy(p.name, name);

		rv = ec_command(EC_CMD_GPIO_GET, cmdver, &p,
				sizeof(p), &r, sizeof(r));
		if (rv < 0)
			return rv;

		//printf("GPIO %s = %d\n", p.name, r.val);
		return 0;
	}

	p_v1.subcmd = EC_GPIO_GET_BY_NAME;
	strcpy(p_v1.get_value_by_name.name, name);

	rv = ec_command(EC_CMD_GPIO_GET, cmdver, &p_v1,
			sizeof(p_v1), &r_v1, sizeof(r_v1));

	if (rv < 0)
		return rv;

	return 0;
}


int ec_check_gpio_access(int interface, char *gpio)
{
	int rv = 1;

	if(init_ec(interface) == 0) {
		rv = cmd_gpio_get(gpio);
		close_ec();
		if(rv==0)
			return 0;
		// convert rv to EC error code
		rv = abs(rv+EECRESULT);
		if(rv==EC_RES_ACCESS_DENIED)
			log_("Error: cant access \"%s\" GPIO, system is locked\n", gpio);
		else if(rv==EC_RES_ERROR)
			log_("Error: cant find \"%s\" GPIO\n", gpio);
		else if(rv)
			log_("Error: unknown error %s\n", gpio);

	}
	return rv;
}

// modified from xfce4-diskperf-plugin
static int get_stats_data (dev_t p_iDevice, char *stats_act, int stat_size)
	/* Get disk performance statistics from STATISTICS_FILE_1 */
{
	const int       iMajorNo = (p_iDevice >> 8) & 0xFF;
	const int       iMinorNo = p_iDevice & 0xFF;
	FILE           *pF;
	unsigned int    major, minor;
	int             c, n;

	pF = fopen (STATS_FILENAME, "r");
	if (!pF) {
		perror (STATS_FILENAME);
		return (-1);
	}
	while (1) {
		n = fscanf (pF, "%u %u", &major, &minor);
		if (n != 2)
			goto Error;
		if ((major != iMajorNo) || (minor != iMinorNo)) {
			while ((c = fgetc (pF)) && (c != '\n'));	/* Goto next line */
			continue;
	}
	/* Read rest of line into stats_act */
	if (!(fgets (stats_act, stat_size, pF)))
	    goto Error;
	fclose (pF);
	return (0);
	}
  Error:
	fclose (pF);
	return (-1);
}				/* DevGetPerfData1() */

int ec_get_interface()
{
	int interface = 0;
	char *interface_name;
#ifndef SIMULATION
	set_command_offset(EC_CMD_PASSTHRU_OFFSET(0));

	if (acquire_gec_lock(GEC_LOCK_TIMEOUT_SECS) < 0) {
		fprintf(stderr, "Could not acquire GEC lock, did you run as root?\n");
		return 0;
	}

	interface=comm_init(COMM_ALL, EC_DEVICE_NAME);
	release_gec_lock();

	if (interface == 0) {
		log_("Couldn't find EC\n");
		return 0;
	}

	if(interface == COMM_DEV)
		interface_name = "dev";
	else if(interface == COMM_LPC)
		interface_name = "lpc";
	else
		interface_name = "i2c";

	log_("Using \"%s\" interface to EC\n", interface_name);
#endif
	return interface;
}

int init_ec(int interfaces)
{
#ifndef SIMULATION
	set_command_offset(EC_CMD_PASSTHRU_OFFSET(0));

	if (acquire_gec_lock(GEC_LOCK_TIMEOUT_SECS) < 0) {
		fprintf(stderr, "Could not acquire GEC lock.\n");
		return 1;
	}

	if (comm_init(interfaces, EC_DEVICE_NAME) == 0) {
		fprintf(stderr, "Couldn't find EC\n");
		release_gec_lock();
		return 1;
	}
#endif
	return 0;
}

int close_ec()
{
#ifndef SIMULATION
	release_gec_lock();
#endif
	return 0;
}
#if 0

#include <sys/file.h>
#include <errno.h>

int pid_file = open("/var/run/whatever.pid", O_CREAT | O_RDWR, 0666);
int rc = flock(pid_file, LOCK_EX | LOCK_NB);
if(rc) {
	if(EWOULDBLOCK == errno)
	    ; // another instance is running
}
else {
	// this is the first instance
}
#endif

void show_help()
{
	printf(DAEMON_NAME " v%s by " DAEMON_AUTHOR ", " __DATE__ "\n\n",
		DAEMON_VERSION );
	printf("Turns your Chromebook power LED into dual HDD-/power LED.\n");
	printf("Hdd activity is indicated by orange blinking of the power LED.\n\n");
	printf("Usage: " DAEMON_NAME " [-o on_time] [-f off_time] [-u update_time] [-d device_name]\n");
	printf("-c dont run as daemon, e.g. for debugging purpose\n");
	printf("-g use GPIO for LED control, faster but not possible on write protected systems\n");
	printf("-o specifies the hdd LED on time (=orange LED) in micro seconds\n");
	printf("-f specifies the hdd LED off time (=blue LED) in micro seconds\n");
	printf("-u specifies the disk statistics update intervall in micro seconds\n");
	printf("-d specifies the device name for monitoring, default is \"/dev/sda\"\n\n");
	printf("e.g. " DAEMON_NAME " -o 40000 -f 10000 -u 200000 -d /dev/sda\n\n");
}

int parse_cmdline(int argc, char * const argv[], config_t *config)
{
	long opt;
	int ch;
	static char dev_name[256];

	// pares command line
	while((ch = getopt(argc, argv, "gcd:o:f:u:h")) != -1) {
		switch(ch) {
			case 'g':
				config->gpio_acces = 1;
				break;

			case 'c':
				config->no_deamon = 1;
				break;

			case 'o':
				opt = atol(optarg);
				if(opt > 1000)
					config->on_time_us = opt;
				break;

			case 'f':
				opt = atol(optarg);
				if(opt > 1000)
					config->off_time_us = opt;
				break;

			case 'u':
				opt = atol(optarg);
				if(opt > 1000)
					config->update_time_us = opt;
				break;

			case 'd':
				strncpy(dev_name, optarg, DEV_NAME_LEN-1);
				config->devname = dev_name;
				break;

			default:
			case 'h':
				show_help();
				return 1;
		}
	}

	return 0;
}

void log_printf(const char * format, ...)
{
	char buffer[256];
	va_list args;
	va_start (args, format);
	vsprintf (buffer, format, args);
	printf("%s", buffer);
	va_end (args);
}

void log_syslog(const char * format, ...)
{
	char buffer[256];
	va_list args;
	va_start (args, format);
	vsprintf (buffer, format, args);
	syslog( LOG_NOTICE, "%s", buffer);
	va_end (args);
}

#define STAT_BUFF_SIZE			256

int main(int argc, char *argv[])
{
	int rv = EXIT_SUCCESS;
	int status;
	int ec_interface;
	struct stat disk_stat;
	char stats_act[STAT_BUFF_SIZE];	// buffer: performance stat line for device
	char stats_act_old[STAT_BUFF_SIZE];
	config_t config = {
		.on_time_us 	= LEDON_INTERVAL,
		.off_time_us	= LEDOFF_INTERVAL,
		.update_time_us = CHECK_INTERVAL,
		.devname		= HDD_NAME,
		.no_deamon		= 0,
		.gpio_acces		= 0,
	};
	ledf_t set_led = set_led_cmd;

	//~ char *git_version=GIT_VERSION;
	//~ snprintf(pgm_version, sizeof(pgm_version)-1, "%s-%s", DAEMON_VERSION,
		//~ *git_version?git_version:"nongit");

	if(parse_cmdline(argc, argv, &config))
		exit(EXIT_FAILURE);

// handle signals for controlled shutdown
	signal(SIGTERM, sig_handler);
	signal(SIGINT, 	sig_handler);

	if(!config.no_deamon) {
		start_daemon (DAEMON_NAME, LOG_LOCAL0);
		log_ = log_syslog;
	}


	// allow only one instance
	int pid_file = open(PID_FILE, O_CREAT | O_RDWR, 0666);
	int rc = flock(pid_file, LOCK_EX | LOCK_NB);
	if(rc) {
		int errsv = errno;
		if(EWOULDBLOCK == errsv) {
			log_("Error: another instance is already running\n"); // another instance is running
			exit (EXIT_FAILURE);
		}
	}

	// get dev id for hdd name
	stat (config.devname, &disk_stat);
	status = get_stats_data (disk_stat.st_rdev, &stats_act[0],
		sizeof(stats_act));
	if (status == -1) {
		 log_("Device statistics unavailable for \"%s\".\n", config.devname);
		 exit(EXIT_FAILURE);
	}

	ec_interface = ec_get_interface();
	if( ec_interface == 0)
		exit(EXIT_FAILURE);

	if(config.gpio_acces) {
		if(ec_check_gpio_access(ec_interface, PWR_LED0_NAME) ||
		   ec_check_gpio_access(ec_interface, PWR_LED1_NAME)) {
				log_("GPIO access to LED not available using fallback\n");
				set_led = set_led_cmd;
				//exit(EXIT_FAILURE);
		} else
			set_led = set_led_gpio;
	}

	// disable led contolling by EC
	// we use much faster gpio toggle instaed
	// LED_GPIO is open drain = active low
	set_led_cmd(ec_interface, HDDLED_OFF);
	set_led(ec_interface, HDDLED_BLUE);

	log_("Started HDD-LED for \"%s\": t_on= %d us, t_off= %d us, t_update= %d us\n",
		config.devname,
		config.on_time_us,
		config.off_time_us,
		config.update_time_us);

	while (!exit_daemon) {
		// read disk stats from proc
		if(get_stats_data (disk_stat.st_rdev, &stats_act[0],
			sizeof(stats_act)) == -1) {
				log_("Device statistics unavailable for \"%s\".\n", config.devname);
				rv = EXIT_FAILURE;
				goto out;
			}

		//compare if stats have changed
		while (memcmp(stats_act_old, stats_act, STAT_BUFF_SIZE) != 0) {
			int ec=0;
			//save old stats for compare
			memcpy(stats_act_old, stats_act, STAT_BUFF_SIZE);
			get_stats_data (disk_stat.st_rdev, &stats_act[0], sizeof(stats_act));

			// turn the light on
			ec = set_led(ec_interface, HDDLED_ORANGE);
			usleep(config.on_time_us);

			// turn it off
			ec |= set_led(ec_interface, HDDLED_BLUE);
			usleep(config.off_time_us);

			if(ec) {
				log_("Error: cant control LED\n");
				rv = EXIT_FAILURE;
				goto out;
			}
		}

		usleep(config.update_time_us);
	}

out:
	// !!! set to automatic color on exit
	// !!! important for indication of suspend
	set_led_cmd(ec_interface, HDDLED_AUTO);
	log_("Stopped\n");

	if(!config.no_deamon)
		closelog();

	return rv;
}
