/****************************************************************************
 *
 *   Copyright (C) 2026 PX4 Development Team. All rights reserved.
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

#include "pcal6524.h"

#include <drivers/drv_sensor.h>
#include <stdlib.h>
#include <string.h>

constexpr pcal6524_config_t default_config {
	.device_type = DRV_GPIO_DEVTYPE_PCAL6524,
	.i2c_addr = 0x22,
	.i2c_bus = 0,
	.first_minor = 0,
	.num_pins = 24,
	.num_banks = 3,
	.interval = 10,
	.direction = 0x00FFFFFF,
	.state = 0,
	.pullup = 0,
};

extern "C" int pcal6524_main(int argc, char *argv[])
{
	BusCLIArguments cli{true, false};
	cli.default_i2c_frequency = 400000;
	cli.i2c_address = 0x22;
	pcal6524_config_t cfg = default_config;

	int ch;

	while ((ch = cli.getOpt(argc, argv, "D:O:P:U:M:")) != EOF) {
		switch (ch) {
		case 'D':
			cfg.direction = static_cast<uint32_t>(strtoul(cli.optArg(), nullptr, 0)) & 0x00FFFFFFu;
			break;

		case 'O':
			cfg.state = static_cast<uint32_t>(strtoul(cli.optArg(), nullptr, 0)) & 0x00FFFFFFu;
			break;

		case 'P':
			cfg.pullup = static_cast<uint32_t>(strtoul(cli.optArg(), nullptr, 0)) & 0x00FFFFFFu;
			break;

		case 'U':
			cfg.interval = static_cast<uint16_t>(atoi(cli.optArg()));
			break;

		case 'M':
			cfg.first_minor = static_cast<uint8_t>(atoi(cli.optArg()));
			break;
		}
	}

	const char *verb = cli.optArg();

	if (!verb) {
		PCAL6524::print_usage();
		return -1;
	}

	cli.custom_data = &cfg;

	BusInstanceIterator iterator("PCAL6524", cli, cfg.device_type);

	if (!strcmp(verb, "start")) {
		return PCAL6524::module_start(cli, iterator);
	}

	if (!strcmp(verb, "stop")) {
		return PCAL6524::module_stop(iterator);
	}

	if (!strcmp(verb, "status")) {
		return PCAL6524::module_status(iterator);
	}

	PCAL6524::print_usage();
	return -1;
}

