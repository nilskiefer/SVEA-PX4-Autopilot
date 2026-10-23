/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
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

#include "bq769x2.h"

#include <drivers/drv_sensor.h>
#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/param.h>

#include <stdlib.h>
#include <string.h>

void BQ769x2::print_usage()
{
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Driver for TI BQ769x2 battery monitor ICs.

This implementation follows a strict startup path:
it probes the IC and optionally applies data-memory configuration before scheduling runtime sampling.

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("bq769x2", "driver");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, false);
	PRINT_MODULE_USAGE_PARAMS_I2C_ADDRESS(0x08);
	PRINT_MODULE_USAGE_PARAM_INT('t', 1, 1, 3, "battery index (1..3)", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

extern "C" int bq769x2_main(int argc, char *argv[])
{
	using ThisDriver = BQ769x2;
	BusCLIArguments cli{true, false};
	cli.default_i2c_frequency = 400000;
	cli.custom1 = 1;

	int32_t default_addr = BQ769X2_DEFAULT_I2C_ADDR;
	const param_t addr_param = param_find("BQ769X2_ADDR");

	if (addr_param != PARAM_INVALID) {
		param_get(addr_param, &default_addr);
	}

	cli.i2c_address = static_cast<uint8_t>(default_addr);

	int ch;

	while ((ch = cli.getOpt(argc, argv, "t:")) != EOF) {
		switch (ch) {
		case 't':
			cli.custom1 = static_cast<int>(strtol(cli.optArg(), nullptr, 0));
			break;
		}
	}

	const char *verb = cli.optArg();

	if (!verb) {
		ThisDriver::print_usage();
		return -1;
	}

	BusInstanceIterator iterator(MODULE_NAME, cli, DRV_BAT_DEVTYPE_BQ769X2);

	if (!strcmp(verb, "start")) {
		return ThisDriver::module_start(cli, iterator);
	}

	if (!strcmp(verb, "stop")) {
		return ThisDriver::module_stop(iterator);
	}

	if (!strcmp(verb, "status")) {
		return ThisDriver::module_status(iterator);
	}

	ThisDriver::print_usage();
	return -1;
}
