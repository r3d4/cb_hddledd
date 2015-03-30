/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm-host.h"
#include "ec_commands.h"

int (*ec_command_proto)(int command, int version,
			const void *outdata, int outsize,
			void *indata, int insize);

int (*ec_readmem)(int offset, int bytes, void *dest);

int ec_max_outsize, ec_max_insize;
//~ void *ec_outbuf;
//~ void *ec_inbuf;
static int command_offset;

int comm_init_dev(const char *device_name) __attribute__((weak));
int comm_init_lpc(void) __attribute__((weak));
int comm_init_i2c(void) __attribute__((weak));

static int fake_readmem(int offset, int bytes, void *dest)
{
	struct ec_params_read_memmap p;
	int c;
	char *buf;

	p.offset = offset;

	if (bytes) {
		p.size = bytes;
		c = ec_command(EC_CMD_READ_MEMMAP, 0, &p, sizeof(p),
			       dest, p.size);
		if (c < 0)
			return c;
		return p.size;
	}

	p.size = EC_MEMMAP_TEXT_MAX;

	c = ec_command(EC_CMD_READ_MEMMAP, 0, &p, sizeof(p), dest, p.size);
	if (c < 0)
		return c;

	buf = dest;
	for (c = 0; c < EC_MEMMAP_TEXT_MAX; c++) {
		if (buf[c] == 0)
			return c;
	}

	buf[EC_MEMMAP_TEXT_MAX - 1] = 0;
	return EC_MEMMAP_TEXT_MAX - 1;
}

void set_command_offset(int offset)
{
	command_offset = offset;
}

int ec_command(int command, int version,
	       const void *outdata, int outsize,
	       void *indata, int insize)
{
	/* Offset command code to support sub-devices */
	return ec_command_proto(command_offset + command, version,
				outdata, outsize,
				indata, insize);
}

int comm_init(int interfaces, const char *device_name)
{
	/* Default memmap access */
	ec_readmem = fake_readmem;

	/* Prefer new /dev method */
	if ((interfaces & COMM_DEV) && comm_init_dev &&
	    !comm_init_dev(device_name)) {
		return COMM_DEV;
	}
	/* Fallback to direct LPC on x86 */
	if ((interfaces & COMM_LPC) && comm_init_lpc && !comm_init_lpc()) {
		return COMM_LPC;
	}

	/* Fallback to direct i2c on ARM */
	if ((interfaces & COMM_I2C) && comm_init_i2c && !comm_init_i2c()) {
		return COMM_I2C;
	}

	/* Give up */
	fprintf(stderr, "Unable to establish host communication\n");
	return 0;
}
