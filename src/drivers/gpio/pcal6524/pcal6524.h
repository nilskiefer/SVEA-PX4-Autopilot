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

#pragma once

#include <drivers/device/i2c.h>
#include <drivers/drv_hrt.h>
#include <lib/drivers/device/Device.hpp>
#include <lib/perf/perf_counter.h>
#include <nuttx/config.h>
#include <nuttx/ioexpander/gpio.h>
#include <px4_platform_common/i2c_spi_buses.h>
#include <uORB/PublicationMulti.hpp>
#include <uORB/SubscriptionCallback.hpp>
#include <uORB/topics/gpio_config.h>
#include <uORB/topics/gpio_in.h>
#include <uORB/topics/gpio_out.h>

#ifndef CONFIG_DEV_GPIO
#error "CONFIG_DEV_GPIO is required to use PCAL6524 GPIO expander, enable it in your NuttX config"
#endif

class PCAL6524CallbackHandler : public uORB::SubscriptionCallback
{
public:
	uint32_t dev_id{0};
	uint32_t input{0};

	explicit PCAL6524CallbackHandler(orb_id_t id) : uORB::SubscriptionCallback(id) {}

	void call() override
	{
		px4::msg::GpioIn new_input{};

		for (int i = 0; i < new_input.MAX_INSTANCES; i++) {
			ChangeInstance(i);

			if (update(&new_input) && new_input.device_id == dev_id) {
				input = new_input.state;
				break;
			}
		}
	}
};

struct pcal6524_gpio_dev_s {
	struct gpio_dev_s gpio;
	uint32_t mask{0};
	bool registered{false};
	PCAL6524CallbackHandler *callback_handler{nullptr};
};

enum class PCAL6524PinType : uint8_t {
	Output,
	Input,
	InputPullUp,
};

struct pcal6524_config_t {
	uint8_t device_type{0};
	uint8_t i2c_addr{0x22};
	uint8_t i2c_bus{0};
	uint8_t first_minor{0};
	uint8_t num_pins{24};
	uint8_t num_banks{3};
	uint16_t interval{10};
	uint32_t direction{0x00FFFFFF};
	uint32_t state{0};
	uint32_t pullup{0};
};

class PCAL6524 : public device::I2C, public I2CSPIDriver<PCAL6524>
{
public:
	PCAL6524(const I2CSPIDriverConfig &config);
	~PCAL6524() override;

	static I2CSPIDriverBase *instantiate(const I2CSPIDriverConfig &config, int runtime_instance);
	static void print_usage();

	int init() override;
	int probe() override;
	void RunImpl();
	void print_status() override;

	static int gpio_read(struct gpio_dev_s *dev, bool *value);
	static int gpio_write(struct gpio_dev_s *dev, bool value);
	static int gpio_setpintype(struct gpio_dev_s *dev, enum gpio_pintype_e pintype);

	pcal6524_config_t config{};

private:
	enum class Register : uint8_t {
		INPUT_PORT_0 = 0x00,
		INPUT_PORT_1 = 0x01,
		INPUT_PORT_2 = 0x02,
		OUTPUT_PORT_0 = 0x04,
		OUTPUT_PORT_1 = 0x05,
		OUTPUT_PORT_2 = 0x06,
		CONFIG_PORT_0 = 0x0C,
		CONFIG_PORT_1 = 0x0D,
		CONFIG_PORT_2 = 0x0E,
		PULL_ENABLE_0 = 0x4C,
		PULL_ENABLE_1 = 0x4D,
		PULL_ENABLE_2 = 0x4E,
		PULL_SELECT_0 = 0x50,
		PULL_SELECT_1 = 0x51,
		PULL_SELECT_2 = 0x52,
	};

	enum class STATE : uint8_t {
		INIT_COMMUNICATION,
		CONFIGURE,
		CHECK,
		RUNNING,
	} _state{STATE::INIT_COMMUNICATION};

	int init_uorb();
	void cleanup_uorb();

	int read_reg(uint8_t address, uint8_t &data);
	int write_reg(uint8_t address, uint8_t value);

	int read(uint32_t &mask);
	int write(uint32_t mask_set, uint32_t mask_clear);
	int configure(uint32_t mask, PCAL6524PinType type);
	int set_up();
	int sanity_check();
	int init_communication();

	static int register_gpios(const pcal6524_config_t &cfg, pcal6524_gpio_dev_s *gpio_handle, uint32_t dir_mask);
	static int unregister_gpios(const pcal6524_config_t &cfg, pcal6524_gpio_dev_s *gpio_handle);

	static constexpr uint8_t bank_output_reg(uint8_t bank)
	{
		return static_cast<uint8_t>(Register::OUTPUT_PORT_0) + bank;
	}

	static constexpr uint8_t bank_config_reg(uint8_t bank)
	{
		return static_cast<uint8_t>(Register::CONFIG_PORT_0) + bank;
	}

	static constexpr uint8_t bank_input_reg(uint8_t bank)
	{
		return static_cast<uint8_t>(Register::INPUT_PORT_0) + bank;
	}

	static constexpr uint8_t bank_pull_enable_reg(uint8_t bank)
	{
		return static_cast<uint8_t>(Register::PULL_ENABLE_0) + bank;
	}

	static constexpr uint8_t bank_pull_select_reg(uint8_t bank)
	{
		return static_cast<uint8_t>(Register::PULL_SELECT_0) + bank;
	}

	uORB::SubscriptionCallbackWorkItem _gpio_out_sub{this, ORB_ID(gpio_out)};
	uORB::SubscriptionCallbackWorkItem _gpio_config_sub{this, ORB_ID(gpio_config)};
	uORB::PublicationMulti<gpio_in_s> _gpio_in_pub{ORB_ID(gpio_in)};

	perf_counter_t _cycle_perf{perf_alloc(PC_ELAPSED, MODULE_NAME ": cycle")};
	perf_counter_t _comms_errors{perf_alloc(PC_COUNT, MODULE_NAME ": comms errors")};
	perf_counter_t _register_check{perf_alloc(PC_COUNT, MODULE_NAME ": register check")};

	uint32_t _olat{0};
	uint32_t _iodir{0x00FFFFFF};
	uint32_t _pull_enable{0};
	uint32_t _pull_select{0};
	pcal6524_gpio_dev_s _gpio_handle[24]{};

	uint16_t _check_every{10};
	uint16_t _run_counter{0};
};
