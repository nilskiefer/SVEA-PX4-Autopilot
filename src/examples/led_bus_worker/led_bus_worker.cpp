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

#include <drivers/drv_board_led.h>
#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/posix.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "../led_bus/led_bus.hpp"

extern "C" __EXPORT int led_bus_worker_main(int argc, char *argv[]);

class LedBusWorker : public ModuleBase
{
public:
	static Descriptor desc;

	explicit LedBusWorker(uint32_t interval_ms) :
		_interval_us(interval_ms * 1000U)
	{
	}

	~LedBusWorker() override = default;

	static int task_spawn(int argc, char *argv[]);
	static LedBusWorker *instantiate(int argc, char *argv[]);
	static int run_trampoline(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);

	void run() override;
	int print_status() override;

private:
	static constexpr uint8_t kLedCount = 6;
	// Physical LED order (L1..L6) mapped to PX4 logical LED indices.
	static constexpr uint8_t kLogicalLedForPhysical[kLedCount] = {1, 0, 2, 3, 4, 5};

	uint32_t _interval_us{5000};
	uint8_t _last_mask{0xff};
};

ModuleBase::Descriptor LedBusWorker::desc{task_spawn, custom_command, print_usage};

int LedBusWorker::print_status()
{
	PX4_INFO("running, poll interval: %.1f ms", (double)_interval_us / 1000.0);
	return 0;
}

int LedBusWorker::custom_command(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return print_usage("unknown command");
}

int LedBusWorker::run_trampoline(int argc, char *argv[])
{
	return ModuleBase::run_trampoline_impl(desc, [](int ac, char *av[]) -> ModuleBase * {
		return LedBusWorker::instantiate(ac, av);
	}, argc, argv);
}

int LedBusWorker::task_spawn(int argc, char *argv[])
{
	desc.task_id = px4_task_spawn_cmd("led_bus_worker",
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

LedBusWorker *LedBusWorker::instantiate(int argc, char *argv[])
{
	uint32_t interval_ms = 5;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "t:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 't': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 1 || v > 100) {
					PX4_ERR("invalid -t %ld (1..100 ms)", v);
					return nullptr;
				}

				interval_ms = (uint32_t)v;
				break;
			}

		default:
			return nullptr;
		}
	}

	LedBusWorker *instance = new LedBusWorker(interval_ms);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
	}

	return instance;
}

void LedBusWorker::run()
{
	drv_led_start();
	const int fd = px4_open(LED0_DEVICE_PATH, 0);

	if (fd < 0) {
		PX4_ERR("failed to open %s (%d)", LED0_DEVICE_PATH, errno);
		return;
	}

	led_bus::clear_pattern_mask();
	led_bus::clear_button_mask();

	for (uint8_t i = 0; i < kLedCount; i++) {
		px4_ioctl(fd, LED_OFF, kLogicalLedForPhysical[i]);
	}

	while (!should_exit()) {
		const uint8_t mask = led_bus::get_combined_mask();

		if (mask != _last_mask) {
			for (uint8_t i = 0; i < kLedCount; i++) {
				const bool on = ((mask >> i) & 0x1u) != 0;
				px4_ioctl(fd, on ? LED_ON : LED_OFF, kLogicalLedForPhysical[i]);
			}

			_last_mask = mask;
		}

		px4_usleep(_interval_us);
	}

	for (uint8_t i = 0; i < kLedCount; i++) {
		px4_ioctl(fd, LED_OFF, kLogicalLedForPhysical[i]);
	}

	px4_close(fd);
}

int LedBusWorker::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Single-writer LED bus worker. Other modules publish desired LED masks and
this module applies the combined state to hardware.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("led_bus_worker", "example");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_INT('t', 5, 1, 100, "Worker period in milliseconds", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

int led_bus_worker_main(int argc, char *argv[])
{
	return ModuleBase::main(LedBusWorker::desc, argc, argv);
}

