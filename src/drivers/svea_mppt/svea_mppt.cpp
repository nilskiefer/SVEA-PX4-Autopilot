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

#include <drivers/device/i2c.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_sensor.h>
#include <parameters/param.h>
#include <px4_platform_common/i2c_spi_buses.h>
#include <px4_platform_common/module.h>
#include <nuttx/ioexpander/gpio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cmath>

class SveaMppt : public device::I2C, public I2CSPIDriver<SveaMppt>
{
public:
	SveaMppt(const I2CSPIDriverConfig &config) : I2C(config), I2CSPIDriver(config) {}
	~SveaMppt() override
	{
		if (_gpio_fd >= 0) {
			set_enabled(false);
			::close(_gpio_fd);
		}
	}

	static I2CSPIDriverBase *instantiate(const I2CSPIDriverConfig &config, int runtime_instance);
	static void print_usage();
	int init() override;
	void RunImpl();
	void print_status() override;

private:
	static float nominal_voltage(int code)
	{
		const float lower = code * (10000.f / 128.f);
		const float upper = 100.f + 10000.f - lower;
		// H and W are tied: the upper ladder/terminal path is in parallel with the wiper switch.
		const float digipot = lower + (upper * 35.f) / (upper + 35.f);
		return 1.04f * (1.f + 100000.f / (5760.f + digipot));
	}

	static int voltage_to_code(float voltage)
	{
		if (!std::isfinite(voltage) || voltage < 7.66f || voltage > 18.98f) {
			return -1;
		}

		int best = 0;

		for (int code = 1; code <= 127; ++code) {
			if (fabsf(nominal_voltage(code) - voltage) < fabsf(nominal_voltage(best) - voltage)) {
				best = code;
			}
		}

		return best;
	}

	int probe() override;
	int set_enabled(bool enabled);
	void fail(const char *reason);
	void exit_and_cleanup() override
	{
		set_enabled(false);
		I2CSPIDriverBase::exit_and_cleanup();
	}

	param_t _enable_param{PARAM_INVALID};
	param_t _voltage_param{PARAM_INVALID};
	int _gpio_fd{-1};
	int _code{-1};
	float _target_v{0.f};
	bool _enabled{false};
	bool _fault{false};
};

int SveaMppt::set_enabled(bool enabled)
{
	// PCAL6524 sets the output latch low before switching direction. Never drive this net high.
	if (_gpio_fd < 0 || ::ioctl(_gpio_fd, GPIOC_SETPINTYPE, enabled ? GPIO_OUTPUT_PIN : GPIO_INPUT_PIN) != 0) {
		PX4_ERR("cannot %s /dev/gpio6", enabled ? "sink" : "release");
		return PX4_ERROR;
	}

	_enabled = enabled;
	return PX4_OK;
}

int SveaMppt::probe()
{
	uint8_t value = 0;
	return transfer(nullptr, 0, &value, 1);
}

int SveaMppt::init()
{
	_gpio_fd = ::open("/dev/gpio6", O_RDWR);

	if (set_enabled(false) != PX4_OK) {
		return PX4_ERROR;
	}

	_enable_param = param_find("SVEA_MPPT_EN");
	_voltage_param = param_find("SVEA_MPPT_V");

	if (_enable_param == PARAM_INVALID || _voltage_param == PARAM_INVALID) {
		PX4_ERR("MPPT parameters missing");
		return PX4_ERROR;
	}

	const int ret = I2C::init();

	if (ret == PX4_OK) {
		// Allow the asynchronous expander driver to apply the initial bypass request.
		ScheduleOnInterval(1000000, 100000);
	}

	return ret;
}

void SveaMppt::fail(const char *reason)
{
	_fault = true;
	set_enabled(false);
	ScheduleClear();
	PX4_ERR("%s; bypass requested, restart driver to clear fault", reason);
}

void SveaMppt::RunImpl()
{
	if (should_exit() || _fault) {
		return;
	}

	int32_t enable = 0;
	float voltage = 0.f;

	if (param_get(_enable_param, &enable) != PX4_OK || param_get(_voltage_param, &voltage) != PX4_OK
	    || (enable != 0 && enable != 1)) {
		fail("invalid MPPT parameters");
		return;
	}

	const int code = voltage_to_code(voltage);

	if (code < 0) {
		fail("SVEA_MPPT_V outside 7.66..18.98 V");
		return;
	}

	if (!enable && set_enabled(false) != PX4_OK) {
		fail("bypass request failed");
		return;
	}

	if (code != _code) {
		const uint8_t command[2] {0x00, static_cast<uint8_t>(code)};

		if (transfer(command, sizeof(command), nullptr, 0) != PX4_OK) {
			fail("wiper write failed");
			return;
		}
	}

	uint8_t readback = 0;

	if (transfer(nullptr, 0, &readback, 1) != PX4_OK || readback != code) {
		fail("wiper readback failed");
		return;
	}

	_code = code;
	_target_v = voltage;

	if (enable && set_enabled(true) != PX4_OK) {
		fail("enable request failed");
	}
}

void SveaMppt::print_status()
{
	I2CSPIDriverBase::print_status();

	if (_code >= 0) {
		PX4_INFO("target=%.3f V, nominal quantized target=%.3f V (not measured)",
			 (double)_target_v, (double)nominal_voltage(_code));
	}

	PX4_INFO("TPL0401B code=%d, gpio6=%s (requested), fault=%d", _code,
		 _enabled ? "sink low / digital adjustment" : "released / fixed divider", _fault);
}

I2CSPIDriverBase *SveaMppt::instantiate(const I2CSPIDriverConfig &config, int runtime_instance)
{
	if (config.bus != 2 || config.i2c_address != 0x3e) {
		PX4_ERR("PMB3 requires bus 2, address 0x3e and primary expander GPIO6");
		return nullptr;
	}

	auto *instance = new SveaMppt(config);

	if (instance && instance->init() != PX4_OK) {
		delete instance;
		return nullptr;
	}

	return instance;
}

void SveaMppt::print_usage()
{
	PRINT_MODULE_DESCRIPTION("PMB3 CN3722 MPPT setpoint via TPL0401B and primary expander GPIO6.");
	PRINT_MODULE_USAGE_NAME("svea_mppt", "driver");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, false);
	PRINT_MODULE_USAGE_PARAMS_I2C_ADDRESS(0x3e);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

extern "C" __EXPORT int svea_mppt_main(int argc, char *argv[])
{
	BusCLIArguments cli{true, false};
	cli.i2c_address = 0x3e;
	cli.default_i2c_frequency = 100000;

	while (cli.getOpt(argc, argv, "") != EOF) {}

	const char *verb = cli.optArg();
	BusInstanceIterator iterator(MODULE_NAME, cli, DRV_DEVTYPE_UNUSED);

	if (verb && !strcmp(verb, "start")) {
		return SveaMppt::module_start(cli, iterator);
	}

	if (verb && !strcmp(verb, "stop")) {
		return SveaMppt::module_stop(iterator);
	}

	if (verb && !strcmp(verb, "status")) {
		return SveaMppt::module_status(iterator);
	}

	SveaMppt::print_usage();
	return PX4_ERROR;
}
