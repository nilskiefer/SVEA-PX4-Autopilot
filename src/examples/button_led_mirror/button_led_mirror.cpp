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

#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/posix.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "board_config.h"
#include "../led_bus/led_bus.hpp"

extern "C" __EXPORT int button_led_mirror_main(int argc, char *argv[]);

class ButtonLedMirror : public ModuleBase
{
public:
	static Descriptor desc;

	explicit ButtonLedMirror(uint32_t interval_ms) :
		_interval_us(interval_ms * 1000U)
	{
	}

	~ButtonLedMirror() override = default;

	static int task_spawn(int argc, char *argv[]);
	static ButtonLedMirror *instantiate(int argc, char *argv[]);
	static int run_trampoline(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);

	void run() override;
	int print_status() override;

private:
	static constexpr uint8_t kButtonCount = 6;
	static constexpr uint32_t kButtonPins[kButtonCount] = {
		GPIO_BTN_T1, GPIO_BTN_T2, GPIO_BTN_T3, GPIO_BTN_T4, GPIO_BTN_T5, GPIO_BTN_T6
	};

	uint32_t _interval_us{20000};
};

ModuleBase::Descriptor ButtonLedMirror::desc{task_spawn, custom_command, print_usage};

int ButtonLedMirror::print_status()
{
	PX4_INFO("running, poll interval: %.1f ms", (double)_interval_us / 1000.0);
	return 0;
}

int ButtonLedMirror::custom_command(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return print_usage("unknown command");
}

int ButtonLedMirror::run_trampoline(int argc, char *argv[])
{
	return ModuleBase::run_trampoline_impl(desc, [](int ac, char *av[]) -> ModuleBase * {
		return ButtonLedMirror::instantiate(ac, av);
	}, argc, argv);
}

int ButtonLedMirror::task_spawn(int argc, char *argv[])
{
	desc.task_id = px4_task_spawn_cmd("button_led_mirror",
					  SCHED_DEFAULT,
					  SCHED_PRIORITY_DEFAULT,
					  1300,
					  (px4_main_t)&run_trampoline,
					  (char *const *)argv);

	if (desc.task_id < 0) {
		desc.task_id = -1;
		return -errno;
	}

	return 0;
}

ButtonLedMirror *ButtonLedMirror::instantiate(int argc, char *argv[])
{
	uint32_t interval_ms = 20;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "t:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 't': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 2 || v > 1000) {
					PX4_ERR("invalid -t %ld (2..1000 ms)", v);
					return nullptr;
				}

				interval_ms = (uint32_t)v;
				break;
			}

		default:
			return nullptr;
		}
	}

	ButtonLedMirror *instance = new ButtonLedMirror(interval_ms);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
	}

	return instance;
}

void ButtonLedMirror::run()
{
	while (!should_exit()) {
		uint8_t mask = 0;

		for (uint8_t i = 0; i < kButtonCount; i++) {
			// Buttons are active-low with pull-ups.
			const bool pressed = !px4_arch_gpioread(kButtonPins[i]);

			if (pressed) {
				mask |= static_cast<uint8_t>(1u << i);
			}
		}

		led_bus::set_button_mask(mask);
		px4_usleep(_interval_us);
	}

	led_bus::clear_button_mask();
}

int ButtonLedMirror::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Mirrors BTN1..BTN6 to LED1..LED6 while each button is held.

Useful for validating board button and LED wiring.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("button_led_mirror", "example");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_INT('t', 20, 2, 1000, "Poll interval in milliseconds", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

int button_led_mirror_main(int argc, char *argv[])
{
	return ModuleBase::main(ButtonLedMirror::desc, argc, argv);
}
