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

#include <errno.h>
#include <px4_platform_common/module.h>
#include <string.h>

static uORB::PublicationMulti<gpio_out_s> g_gpio_out_pub{ORB_ID(gpio_out)};
static uORB::PublicationMulti<gpio_config_s> g_gpio_config_pub{ORB_ID(gpio_config)};

static const struct gpio_operations_s g_gpio_ops {
	PCAL6524::gpio_read,
	PCAL6524::gpio_write,
	nullptr,
	nullptr,
	PCAL6524::gpio_setpintype,
};

PCAL6524::PCAL6524(const I2CSPIDriverConfig &config_in) :
	I2C(config_in),
	I2CSPIDriver(config_in)
{
}

PCAL6524::~PCAL6524()
{
	ScheduleClear();
	cleanup_uorb();
	perf_free(_cycle_perf);
	perf_free(_comms_errors);
	perf_free(_register_check);
}

int PCAL6524::init()
{
	int ret = I2C::init();

	if (ret != PX4_OK) {
		PX4_ERR("I2C init failed");
		return ret;
	}

	_iodir = config.direction & 0x00FFFFFFu;
	_olat = config.state & 0x00FFFFFFu;
	_pull_select = config.pullup & 0x00FFFFFFu;
	_pull_enable = config.pullup & 0x00FFFFFFu;
	_check_every = 10;

	ScheduleNow();
	return PX4_OK;
}

int PCAL6524::probe()
{
	uint8_t dummy{0};

	for (int i = 0; i < 10; i++) {
		if (read_reg(static_cast<uint8_t>(Register::CONFIG_PORT_0), dummy) == PX4_OK) {
			return PX4_OK;
		}

		px4_usleep(10'000);
	}

	return PX4_ERROR;
}

int PCAL6524::read_reg(uint8_t address, uint8_t &data)
{
	int ret = transfer(&address, 1, &data, 1);

	if (ret != PX4_OK) {
		perf_count(_comms_errors);
	}

	return ret;
}

int PCAL6524::write_reg(uint8_t address, uint8_t value)
{
	uint8_t buf[2] {address, value};
	int ret = transfer(buf, sizeof(buf), nullptr, 0);

	if (ret != PX4_OK) {
		perf_count(_comms_errors);
	}

	return ret;
}

int PCAL6524::read(uint32_t &mask)
{
	mask = 0;
	int ret = PX4_OK;

	for (uint8_t bank = 0; bank < config.num_banks; bank++) {
		uint8_t in{0};
		ret |= read_reg(bank_input_reg(bank), in);
		mask |= static_cast<uint32_t>(in) << (8 * bank);
	}

	return ret;
}

int PCAL6524::write(uint32_t mask_set, uint32_t mask_clear)
{
	_olat = (_olat & ~mask_clear) | mask_set;
	_olat &= 0x00FFFFFFu;

	int ret = PX4_OK;

	for (uint8_t bank = 0; bank < config.num_banks; bank++) {
		const uint8_t bank_val = static_cast<uint8_t>((_olat >> (8 * bank)) & 0xFF);
		uint8_t verify{0};
		ret |= write_reg(bank_output_reg(bank), bank_val);
		ret |= read_reg(bank_output_reg(bank), verify);

		if (ret != PX4_OK || verify != bank_val) {
			return PX4_ERROR;
		}
	}

	return PX4_OK;
}

int PCAL6524::configure(uint32_t mask, PCAL6524PinType type)
{
	switch (type) {
	case PCAL6524PinType::Input:
		_iodir |= mask;
		_pull_enable &= ~mask;
		break;

	case PCAL6524PinType::InputPullUp:
		_iodir |= mask;
		_pull_enable |= mask;
		_pull_select |= mask;
		break;

	case PCAL6524PinType::Output:
		_iodir &= ~mask;
		_pull_enable &= ~mask;
		break;

	default:
		return -EINVAL;
	}

	_iodir &= 0x00FFFFFFu;
	_pull_enable &= 0x00FFFFFFu;
	_pull_select &= 0x00FFFFFFu;

	int ret = PX4_OK;

	for (uint8_t bank = 0; bank < config.num_banks; bank++) {
		const uint8_t iodir = static_cast<uint8_t>((_iodir >> (8 * bank)) & 0xFF);
		const uint8_t pull_en = static_cast<uint8_t>((_pull_enable >> (8 * bank)) & 0xFF);
		const uint8_t pull_sel = static_cast<uint8_t>((_pull_select >> (8 * bank)) & 0xFF);

		ret |= write_reg(bank_config_reg(bank), iodir);
		ret |= write_reg(bank_pull_enable_reg(bank), pull_en);
		ret |= write_reg(bank_pull_select_reg(bank), pull_sel);
	}

	return ret;
}

int PCAL6524::set_up()
{
	auto sync_registered_gpio_pintypes = [this]() {
		for (uint8_t i = 0; i < config.num_pins && i < 24; i++) {
			if (!_gpio_handle[i].registered) {
				continue;
			}

			const uint32_t mask = 1u << i;

			if ((_iodir & mask) == 0) {
				_gpio_handle[i].gpio.gp_pintype = GPIO_OUTPUT_PIN;

			} else if (((_pull_enable & mask) != 0) && ((_pull_select & mask) != 0)) {
				_gpio_handle[i].gpio.gp_pintype = GPIO_INPUT_PIN_PULLUP;

			} else {
				_gpio_handle[i].gpio.gp_pintype = GPIO_INPUT_PIN;
			}
		}
	};

	// Startup is strictly read-only:
	// adopt live HW state and do not write any registers here.
	uint32_t hw_iodir{0};
	uint32_t hw_olat{0};
	uint32_t hw_pull_en{0};
	uint32_t hw_pull_sel{0};

	for (uint8_t bank = 0; bank < config.num_banks; bank++) {
		uint8_t got_iodir{0};
		uint8_t got_olat{0};
		uint8_t got_pull_en{0};
		uint8_t got_pull_sel{0};

		if (read_reg(bank_config_reg(bank), got_iodir) != PX4_OK
		    || read_reg(bank_output_reg(bank), got_olat) != PX4_OK
		    || read_reg(bank_pull_enable_reg(bank), got_pull_en) != PX4_OK
		    || read_reg(bank_pull_select_reg(bank), got_pull_sel) != PX4_OK) {
			return PX4_ERROR;
		}

		hw_iodir |= static_cast<uint32_t>(got_iodir) << (8 * bank);
		hw_olat |= static_cast<uint32_t>(got_olat) << (8 * bank);
		hw_pull_en |= static_cast<uint32_t>(got_pull_en) << (8 * bank);
		hw_pull_sel |= static_cast<uint32_t>(got_pull_sel) << (8 * bank);
	}

	_iodir = hw_iodir & 0x00FFFFFFu;
	_olat = hw_olat & 0x00FFFFFFu;
	_pull_enable = hw_pull_en & 0x00FFFFFFu;
	_pull_select = hw_pull_sel & 0x00FFFFFFu;
	sync_registered_gpio_pintypes();
	return PX4_OK;
}

int PCAL6524::sanity_check()
{
	int ret = PX4_OK;

	for (uint8_t bank = 0; bank < config.num_banks; bank++) {
		const uint8_t exp_iodir = static_cast<uint8_t>((_iodir >> (8 * bank)) & 0xFF);
		const uint8_t exp_olat = static_cast<uint8_t>((_olat >> (8 * bank)) & 0xFF);
		const uint8_t exp_pull_en = static_cast<uint8_t>((_pull_enable >> (8 * bank)) & 0xFF);
		const uint8_t exp_pull_sel = static_cast<uint8_t>((_pull_select >> (8 * bank)) & 0xFF);
		uint8_t got_iodir{0};
		uint8_t got_olat{0};
		uint8_t got_pull_en{0};
		uint8_t got_pull_sel{0};

		ret |= read_reg(bank_config_reg(bank), got_iodir);
		ret |= read_reg(bank_output_reg(bank), got_olat);
		ret |= read_reg(bank_pull_enable_reg(bank), got_pull_en);
		ret |= read_reg(bank_pull_select_reg(bank), got_pull_sel);

		if (ret != PX4_OK || got_iodir != exp_iodir || got_olat != exp_olat
		    || got_pull_en != exp_pull_en || got_pull_sel != exp_pull_sel) {
			perf_count(_register_check);
			return PX4_ERROR;
		}
	}

	return PX4_OK;
}

int PCAL6524::init_uorb()
{
	if (!_gpio_config_sub.registerCallback() || !_gpio_out_sub.registerCallback()) {
		PX4_ERR("callback registration failed");
		return PX4_ERROR;
	}

	return PX4_OK;
}

void PCAL6524::cleanup_uorb()
{
	_gpio_config_sub.unregisterCallback();
	_gpio_out_sub.unregisterCallback();
	unregister_gpios(config, _gpio_handle);
}

int PCAL6524::init_communication()
{
	int ret = register_gpios(config, _gpio_handle, _iodir);

	if (ret != PX4_OK) {
		return ret;
	}

	return init_uorb();
}

void PCAL6524::RunImpl()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}

	int ret = PX4_OK;

	switch (_state) {
	case STATE::INIT_COMMUNICATION:
		ret = init_communication();

		if (ret == PX4_OK) {
			_state = STATE::CONFIGURE;
			ScheduleNow();

		} else {
			ScheduleDelayed(config.interval * 1000);
		}

		break;

	case STATE::CONFIGURE:
		ret = set_up();

		if (ret == PX4_OK) {
			_state = STATE::CHECK;
			ScheduleNow();

		} else {
			ScheduleDelayed(config.interval * 1000);
		}

		break;

	case STATE::CHECK:
		ret = sanity_check();

		if (ret == PX4_OK) {
			_state = STATE::RUNNING;
			ScheduleOnInterval(config.interval * 1000);

		} else {
			_state = STATE::CONFIGURE;
			ScheduleDelayed(config.interval * 1000);
		}

		break;

	case STATE::RUNNING:
		perf_begin(_cycle_perf);
		gpio_config_s cfg{};

		if (_gpio_config_sub.update(&cfg) && cfg.device_id == get_device_id()) {
			PCAL6524PinType type = PCAL6524PinType::Input;

			switch (cfg.config) {
			case gpio_config_s::INPUT_PULLUP:
				type = PCAL6524PinType::InputPullUp;
				break;

			case gpio_config_s::OUTPUT:
				type = PCAL6524PinType::Output;
				break;

			default:
				type = PCAL6524PinType::Input;
				break;
			}

			ret |= write(cfg.state, cfg.mask);
			ret |= configure(cfg.mask, type);
		}

		gpio_out_s output{};

		if (_gpio_out_sub.update(&output) && output.device_id == get_device_id()) {
			ret |= write(output.state, output.mask);
		}

		{
			gpio_in_s in{};
			in.timestamp = hrt_absolute_time();
			in.device_id = get_device_id();
			uint32_t input_state{0};

			if (read(input_state) == PX4_OK) {
				in.state = input_state;
				_gpio_in_pub.publish(in);
			}
		}

		_run_counter++;

		if (_run_counter >= _check_every || ret != PX4_OK) {
			_state = STATE::CHECK;
			_run_counter = 0;
			ScheduleClear();
			ScheduleNow();
		}

		perf_end(_cycle_perf);
		break;
	}
}

void PCAL6524::print_status()
{
	I2CSPIDriverBase::print_status();
	perf_print_counter(_cycle_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_register_check);
	PX4_INFO("startup mode: preserve live expander state (read-only)");
	PX4_INFO("iodir: 0x%06lx state: 0x%06lx pullup: 0x%06lx", (unsigned long)_iodir,
		 (unsigned long)_olat, (unsigned long)_pull_select);
}

void PCAL6524::print_usage()
{
	PRINT_MODULE_USAGE_NAME("pcal6524", "driver");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, false);
	PRINT_MODULE_USAGE_PARAMS_I2C_ADDRESS(0x22);
	PRINT_MODULE_USAGE_PARAM_INT('D', 0xFFFFFF, 0, 0xFFFFFF, "Direction (1=Input, 0=Output)", true);
	PRINT_MODULE_USAGE_PARAM_INT('O', 0, 0, 0xFFFFFF, "Output state", true);
	PRINT_MODULE_USAGE_PARAM_INT('P', 0, 0, 0xFFFFFF, "Pull-up mask", true);
	PRINT_MODULE_USAGE_PARAM_INT('U', 10, 1, 1000, "Update interval [ms]", true);
	PRINT_MODULE_USAGE_PARAM_INT('M', 0, 0, 255, "First minor number", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

I2CSPIDriverBase *PCAL6524::instantiate(const I2CSPIDriverConfig &config_in, int runtime_instance)
{
	PCAL6524 *instance = new PCAL6524(config_in);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
		return nullptr;
	}

	instance->config = *static_cast<const pcal6524_config_t *>(config_in.custom_data);
	instance->config.i2c_bus = config_in.bus;
	instance->config.i2c_addr = config_in.i2c_address;

	if (instance->init() != PX4_OK) {
		delete instance;
		return nullptr;
	}

	return instance;
}

int PCAL6524::register_gpios(const pcal6524_config_t &cfg, pcal6524_gpio_dev_s *gpio_handle, uint32_t dir_mask)
{
	const auto devid = device::Device::DeviceId{
		device::Device::DeviceBusType_I2C,
		cfg.i2c_bus,
		cfg.i2c_addr,
		static_cast<uint8_t>(cfg.device_type)
	};

	auto *callback_handler = new PCAL6524CallbackHandler(ORB_ID(gpio_in));

	if (callback_handler == nullptr) {
		return PX4_ERROR;
	}

	callback_handler->dev_id = devid.devid;

	if (!callback_handler->registerCallback()) {
		delete callback_handler;
		return PX4_ERROR;
	}

	bool all_registered = true;

	for (uint8_t i = 0; i < cfg.num_pins; i++) {
		const uint32_t mask = 1u << i;

		if (!gpio_handle[i].registered) {
			if (dir_mask & mask) {
				gpio_handle[i] = {{GPIO_INPUT_PIN, {}, &g_gpio_ops}, mask, false, nullptr};

			} else {
				gpio_handle[i] = {{GPIO_OUTPUT_PIN, {}, &g_gpio_ops}, mask, false, nullptr};
			}

			gpio_handle[i].callback_handler = callback_handler;
			const int ret = gpio_pin_register(&gpio_handle[i].gpio, cfg.first_minor + i);

			if (ret != OK) {
				all_registered = false;

			} else {
				gpio_handle[i].registered = true;
			}
		}
	}

	if (!all_registered) {
		for (uint8_t i = 0; i < cfg.num_pins; i++) {
			if (gpio_handle[i].registered) {
				gpio_pin_unregister(&gpio_handle[i].gpio, cfg.first_minor + i);
				gpio_handle[i].registered = false;
			}

			gpio_handle[i].callback_handler = nullptr;
		}

		callback_handler->unregisterCallback();
		delete callback_handler;
		return PX4_ERROR;
	}

	for (uint8_t i = 0; i < cfg.num_pins; i++) {
		gpio_handle[i].callback_handler = callback_handler;
	}

	return PX4_OK;
}

int PCAL6524::unregister_gpios(const pcal6524_config_t &cfg, pcal6524_gpio_dev_s *gpio_handle)
{
	if (gpio_handle[0].callback_handler) {
		auto *handler = gpio_handle[0].callback_handler;
		handler->unregisterCallback();
		delete handler;
	}

	for (uint8_t i = 0; i < cfg.num_pins; i++) {
		if (gpio_handle[i].registered) {
			gpio_pin_unregister(&gpio_handle[i].gpio, cfg.first_minor + i);
			gpio_handle[i].registered = false;
			gpio_handle[i].callback_handler = nullptr;
		}
	}

	return PX4_OK;
}

int PCAL6524::gpio_read(struct gpio_dev_s *dev, bool *value)
{
	auto *gpio = reinterpret_cast<pcal6524_gpio_dev_s *>(dev);

	if (gpio == nullptr || gpio->callback_handler == nullptr || value == nullptr) {
		return -EINVAL;
	}

	*value = (gpio->callback_handler->input & gpio->mask) != 0;
	return OK;
}

int PCAL6524::gpio_write(struct gpio_dev_s *dev, bool value)
{
	auto *gpio = reinterpret_cast<pcal6524_gpio_dev_s *>(dev);

	if (gpio == nullptr || gpio->callback_handler == nullptr) {
		return -EINVAL;
	}

	gpio_out_s msg{};
	msg.timestamp = hrt_absolute_time();
	msg.device_id = gpio->callback_handler->dev_id;
	msg.mask = gpio->mask;
	msg.state = value ? gpio->mask : 0u;
	return g_gpio_out_pub.publish(msg) ? OK : -ETIMEDOUT;
}

int PCAL6524::gpio_setpintype(struct gpio_dev_s *dev, enum gpio_pintype_e pintype)
{
	auto *gpio = reinterpret_cast<pcal6524_gpio_dev_s *>(dev);

	if (gpio == nullptr || gpio->callback_handler == nullptr) {
		return -EINVAL;
	}

	gpio_config_s msg{};
	msg.timestamp = hrt_absolute_time();
	msg.device_id = gpio->callback_handler->dev_id;
	msg.mask = gpio->mask;

	switch (pintype) {
	case GPIO_INPUT_PIN:
		msg.config = gpio_config_s::INPUT;
		gpio->gpio.gp_pintype = GPIO_INPUT_PIN;
		break;

	case GPIO_INPUT_PIN_PULLUP:
		msg.config = gpio_config_s::INPUT_PULLUP;
		gpio->gpio.gp_pintype = GPIO_INPUT_PIN_PULLUP;
		break;

	case GPIO_OUTPUT_PIN:
		msg.config = gpio_config_s::OUTPUT;
		gpio->gpio.gp_pintype = GPIO_OUTPUT_PIN;
		break;

	default:
		return -ENOTSUP;
	}

	return g_gpio_config_pub.publish(msg) ? OK : -ETIMEDOUT;
}
