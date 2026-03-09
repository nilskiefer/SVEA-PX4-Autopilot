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
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "../led_bus/led_bus.hpp"

extern "C" __EXPORT int led_pattern_main(int argc, char *argv[]);

class LedPattern : public ModuleBase<LedPattern>
{
public:
	LedPattern(uint32_t interval_ms, uint8_t led_count) :
		_interval_us(interval_ms * 1000U),
		_led_count(led_count)
	{
	}

	~LedPattern() override = default;

	static int task_spawn(int argc, char *argv[]);
	static LedPattern *instantiate(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);

	void run() override;
	int print_status() override;

private:
	static constexpr uint8_t kFrames[10] = {
		0b000001,
		0b000010,
		0b000100,
		0b001000,
		0b010000,
		0b100000,
		0b010000,
		0b001000,
		0b000100,
		0b000010
	};

	void publish_frame(uint8_t mask);

	uint32_t _interval_us{120000};
	uint8_t _led_count{6};
	uint32_t _step_count{0};
};

constexpr uint8_t LedPattern::kFrames[10];

int LedPattern::print_status()
{
	PX4_INFO("running, interval: %.1f ms, leds: %u, steps: %u",
		 (double)_interval_us / 1000.0, _led_count, (unsigned)_step_count);
	return 0;
}

int LedPattern::custom_command(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return print_usage("unknown command");
}

int LedPattern::task_spawn(int argc, char *argv[])
{
	int task_id = px4_task_spawn_cmd("led_pattern",
					 SCHED_DEFAULT,
					 SCHED_PRIORITY_DEFAULT,
					 1300,
					 run_trampoline,
					 (char *const *)argv);

	if (task_id < 0) {
		return -errno;
	}

	_task_id = task_id;
	return 0;
}

LedPattern *LedPattern::instantiate(int argc, char *argv[])
{
	uint32_t interval_ms = 120;
	uint8_t led_count = 6;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "t:n:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 't': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 20 || v > 2000) {
					PX4_ERR("invalid -t %ld (20..2000 ms)", v);
					return nullptr;
				}

				interval_ms = (uint32_t)v;
				break;
			}

		case 'n': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 1 || v > 6) {
					PX4_ERR("invalid -n %ld (1..6 LEDs)", v);
					return nullptr;
				}

				led_count = (uint8_t)v;
				break;
			}

		default:
			return nullptr;
		}
	}

	LedPattern *instance = new LedPattern(interval_ms, led_count);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
	}

	return instance;
}

void LedPattern::publish_frame(uint8_t mask)
{
	led_bus::set_pattern_mask(mask);
}

void LedPattern::run()
{
	const uint8_t frame_mask = (_led_count == 6) ? 0x3f : static_cast<uint8_t>((1u << _led_count) - 1u);
	_step_count = 0;

	while (!should_exit()) {
		const uint8_t frame = kFrames[_step_count % (sizeof(kFrames) / sizeof(kFrames[0]))] & frame_mask;
		publish_frame(frame);
		_step_count++;
		px4_usleep(_interval_us);
	}

	led_bus::clear_pattern_mask();
}

int LedPattern::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Simple example module that drives board LEDs in a looping pattern.

This is useful as a starter reference for adding modules with:
- `start/stop/status` lifecycle
- argument parsing
- periodic work loop
- publishing to a shared LED bus

### Notes
On Clicker 4 the LEDs are all physically red. Logical LED IDs (blue/amber/etc.)
still map to separate LED indices in the bus worker.

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("led_pattern", "example");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_INT('t', 120, 20, 2000, "Step interval in milliseconds", true);
	PRINT_MODULE_USAGE_PARAM_INT('n', 6, 1, 6, "How many LED indices to animate", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

int led_pattern_main(int argc, char *argv[])
{
	return LedPattern::main(argc, argv);
}
