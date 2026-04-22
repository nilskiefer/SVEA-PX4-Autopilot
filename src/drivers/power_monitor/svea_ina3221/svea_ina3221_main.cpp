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

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>

#include "svea_ina3221.h"

namespace
{
constexpr int SHUNT_BITS = 10;
constexpr int SHUNT_MAX_MILLIOHM = (1 << SHUNT_BITS) - 1;

bool parse_shunt(const char *arg, float &out_ohm)
{
	if (arg == nullptr) {
		return false;
	}

	char *end = nullptr;
	const float v = std::strtof(arg, &end);

	if (end == arg || !std::isfinite(v) || v <= 0.f) {
		return false;
	}

	out_ohm = v;
	return true;
}

bool shunt_ohm_to_milliohm(const float shunt_ohm, int &out_milliohm)
{
	const int mOhm = static_cast<int>(lroundf(shunt_ohm * 1000.f));

	if (mOhm <= 0 || mOhm > SHUNT_MAX_MILLIOHM) {
		return false;
	}

	out_milliohm = mOhm;
	return true;
}

int pack_shunts_milliohm(const float shunt_ohm[INA3221_CHANNEL_COUNT])
{
	int mOhm[INA3221_CHANNEL_COUNT] {};

	for (unsigned i = 0; i < INA3221_CHANNEL_COUNT; i++) {
		if (!shunt_ohm_to_milliohm(shunt_ohm[i], mOhm[i])) {
			return -1;
		}
	}

	return (mOhm[0] & SHUNT_MAX_MILLIOHM) |
	       ((mOhm[1] & SHUNT_MAX_MILLIOHM) << SHUNT_BITS) |
	       ((mOhm[2] & SHUNT_MAX_MILLIOHM) << (2 * SHUNT_BITS));
}
}

I2CSPIDriverBase *SVEA_INA3221::instantiate(const I2CSPIDriverConfig &config, int runtime_instance)
{
	SVEA_INA3221 *instance = new SVEA_INA3221(config);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
		return nullptr;
	}

	if (config.keep_running) {
		if (instance->force_init() != PX4_OK) {
			PX4_INFO("Failed to init svea_ina3221 on bus %d, but will try again periodically.", config.bus);
		}

	} else {
		const int ret = instance->init();

		if (ret != PX4_OK) {
			PX4_ERR("init failed (%d) on bus %d addr 0x%02x", ret, config.bus, config.i2c_address);
			delete instance;
			return nullptr;
		}
	}

	return instance;
}

void SVEA_INA3221::print_usage()
{
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Driver for the SVEA-specific INA3221 triple-channel power monitor.

By default, all three channels use 16 mOhm shunts.

Use -r to set a single shunt value for all channels, or -x/-y/-z
for per-channel shunts (CH1/CH2/CH3).

Use -f keep-running mode if the module may power up after boot.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("svea_ina3221", "driver");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, false);
	PRINT_MODULE_USAGE_PARAMS_I2C_ADDRESS(0x40);
	PRINT_MODULE_USAGE_PARAMS_I2C_KEEP_RUNNING_FLAG();
	PRINT_MODULE_USAGE_PARAM_FLOAT('r', 0.016f, 0.000001f, 1.f, "Shunt resistor (Ohm), all channels", true);
	PRINT_MODULE_USAGE_PARAM_FLOAT('x', 0.016f, 0.000001f, 1.f, "Shunt resistor CH1 (Ohm)", true);
	PRINT_MODULE_USAGE_PARAM_FLOAT('y', 0.016f, 0.000001f, 1.f, "Shunt resistor CH2 (Ohm)", true);
	PRINT_MODULE_USAGE_PARAM_FLOAT('z', 0.016f, 0.000001f, 1.f, "Shunt resistor CH3 (Ohm)", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

extern "C" int svea_ina3221_main(int argc, char *argv[])
{
	int ch;
	using ThisDriver = SVEA_INA3221;

	BusCLIArguments cli{true, false};
	cli.i2c_address = INA3221_BASEADDR;
	cli.default_i2c_frequency = 100000;
	cli.support_keep_running = true;

	float shunt_ohm[INA3221_CHANNEL_COUNT] {
		INA3221_DEFAULT_SHUNT_OHM,
		INA3221_DEFAULT_SHUNT_OHM,
		INA3221_DEFAULT_SHUNT_OHM
	};

	while ((ch = cli.getOpt(argc, argv, "r:x:y:z:")) != EOF) {
		switch (ch) {
		case 'r': {
				float v = 0.f;

				if (!parse_shunt(cli.optArg(), v)) {
					PX4_ERR("invalid shunt value");
					return -1;
				}

				shunt_ohm[0] = v;
				shunt_ohm[1] = v;
				shunt_ohm[2] = v;
			}
			break;

		case 'x': {
				if (!parse_shunt(cli.optArg(), shunt_ohm[0])) {
					PX4_ERR("invalid CH1 shunt value");
					return -1;
				}
			}
			break;

		case 'y': {
				if (!parse_shunt(cli.optArg(), shunt_ohm[1])) {
					PX4_ERR("invalid CH2 shunt value");
					return -1;
				}
			}
			break;

		case 'z': {
				if (!parse_shunt(cli.optArg(), shunt_ohm[2])) {
					PX4_ERR("invalid CH3 shunt value");
					return -1;
				}
			}
			break;
		}
	}

	cli.custom1 = pack_shunts_milliohm(shunt_ohm);

	if (cli.custom1 <= 0) {
		PX4_ERR("invalid shunt values (allowed range: 1..%d mOhm)", SHUNT_MAX_MILLIOHM);
		return -1;
	}

	const char *verb = cli.optArg();

	if (!verb) {
		ThisDriver::print_usage();
		return -1;
	}

	BusInstanceIterator iterator(MODULE_NAME, cli, DRV_POWER_DEVTYPE_INA3221);

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
