/*
 * Copyright (c) 2015 The Henrik Schondorff
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef _CB_HDDLEDD_H_
#define _CB_HDDLEDD_H_

#define DAEMON_NAME				"cb_hddledd"
#define DAEMON_VERSION			"0.10"
#define DAEMON_AUTHOR			"Henrik Schondorff"
#define PID_FILE				"/var/run/cb_hddledd.pid"

#define EC_DEVICE_NAME			"cros_ec"
#define STATS_FILENAME 			"/proc/diskstats"
#define HDD_NAME				"/dev/sda"

#define GEC_LOCK_TIMEOUT_SECS	30  /* 30 secs */

#define CHECK_INTERVAL 			200000
#define LEDON_INTERVAL 			40000
#define LEDOFF_INTERVAL 		10000

#define PWR_LED0_NAME			"PWR_LED0_L"	// GPIO name LED Pin 0
#define PWR_LED1_NAME			"PWR_LED1_L"	// GPIO name LED Pin 1

#define DEV_NAME_LEN			256


/* in Makefile:
 * GIT_VERSION := $(shell git describe --abbrev=7 --dirty --always --tags)
*/
#ifndef	GIT_VERSION
#define GIT_VERSION 			"nongit"
#endif


typedef struct config_t {
	unsigned long on_time_us;	 // LED on time (= orange LED)
	unsigned long off_time_us;	 // LED off time (= blue LED)
	unsigned long update_time_us; // delay of stats update
	char *devname;	// device name e.g. /dev/sda
	int no_deamon;				//no run as daemon
	int gpio_acces;				// use dricet GPIO to toggle LED
}config_t;

typedef struct devperf_t {
    uint64_t        timestamp_ns;
    uint64_t        rbytes;	/* Number of bytes read from the device */
    uint64_t        wbytes;	/* Number of bytes written to the device */
    uint64_t        rbusy_ns;	/* Device read busy time */
    uint64_t        wbusy_ns;	/* Device write busy time */
    int32_t         qlen;	/* Current queue length */
} devperf_t;

typedef struct devstat_t {
	uint64_t 		rsect;
	uint64_t 		ruse;
	uint64_t 		wsect;
	uint64_t 		wuse;
	uint64_t 		running;
	uint64_t 		use;
} devstat_t;

typedef enum  {
	HDDLED_BLUE,
	HDDLED_ORANGE,
	HDDLED_OFF,
	HDDLED_AUTO,
} hddled_color;

typedef enum {
	LEDCOLOR_RED,
	LEDCOLOR_GREE,
	LEDCOLOR_BLUE,
	LEDCOLOR_YELLOW,
	LEDCOLOR_WHITE,
} cb_led_color;

typedef enum {
	LEDTYPE_BATTERY,
	LEDTYPE_POWER,
	LEDTYPE_ADAPTER,
} cb_led_id;


#endif
