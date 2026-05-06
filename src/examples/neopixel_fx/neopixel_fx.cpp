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
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <drivers/drv_neopixel.h>
#include <drivers/drv_hrt.h>

extern "C" __EXPORT int neopixel_fx_main(int argc, char *argv[]);

class NeopixelFx : public ModuleBase
{
public:
	static Descriptor desc;
	static constexpr uint8_t kMaxPriority = 2;

	enum class Effect : uint8_t {
		Rainbow = 0,
		Police,
		Breathe
	};

	NeopixelFx(Effect effect, uint32_t period_ms, uint8_t led_index, uint8_t priority, uint8_t r, uint8_t g, uint8_t b) :
		_effect(effect),
		_period_us(period_ms * 1000U),
		_led_index(led_index),
		_priority(priority),
		_base_r(r),
		_base_g(g),
		_base_b(b)
	{}

	~NeopixelFx() override = default;

	static int task_spawn(int argc, char *argv[]);
	static NeopixelFx *instantiate(int argc, char *argv[]);
	static int run_trampoline(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);

	void run() override;
	int print_status() override;

private:
	static bool parse_effect(const char *name, Effect &out);
	static bool parse_color(const char *name, uint8_t &r, uint8_t &g, uint8_t &b);
	static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b);
	void write_rgb(uint8_t r, uint8_t g, uint8_t b);
	void write_off();

	Effect _effect{Effect::Rainbow};
	uint32_t _period_us{1000000};
	uint8_t _led_index{0};
	uint8_t _priority{2};
	uint32_t _step{0};
	uint8_t _base_r{0};
	uint8_t _base_g{0};
	uint8_t _base_b{255};
	neopixel::NeoLEDData _led_data[1]{};
};

ModuleBase::Descriptor NeopixelFx::desc{task_spawn, custom_command, print_usage};

int NeopixelFx::print_status()
{
	const char *mode = "rainbow";

	switch (_effect) {
	case Effect::Rainbow: mode = "rainbow"; break;
	case Effect::Police:  mode = "police"; break;
	case Effect::Breathe: mode = "breathe"; break;
	}

	PX4_INFO("running effect=%s period=%.1fms led=%u prio=%u base_rgb=(%u,%u,%u) frames=%lu",
		 mode, (double)_period_us / 1000.0, _led_index, _priority, _base_r, _base_g, _base_b,
		 (unsigned long)_step);
	return 0;
}

int NeopixelFx::custom_command(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return print_usage("unknown command");
}

int NeopixelFx::run_trampoline(int argc, char *argv[])
{
	return ModuleBase::run_trampoline_impl(desc, [](int ac, char *av[]) -> ModuleBase * {
		return NeopixelFx::instantiate(ac, av);
	}, argc, argv);
}

int NeopixelFx::task_spawn(int argc, char *argv[])
{
	desc.task_id = px4_task_spawn_cmd("neopixel_fx",
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

bool NeopixelFx::parse_effect(const char *name, Effect &out)
{
	if (strcmp(name, "rainbow") == 0) {
		out = Effect::Rainbow;
		return true;
	}

	if (strcmp(name, "police") == 0) {
		out = Effect::Police;
		return true;
	}

	if (strcmp(name, "breathe") == 0) {
		out = Effect::Breathe;
		return true;
	}

	return false;
}

bool NeopixelFx::parse_color(const char *name, uint8_t &r, uint8_t &g, uint8_t &b)
{
	if (strcmp(name, "red") == 0) {
		r = 255; g = 0; b = 0; return true;
	}

	if (strcmp(name, "green") == 0) {
		r = 0; g = 255; b = 0; return true;
	}

	if (strcmp(name, "blue") == 0) {
		r = 0; g = 0; b = 255; return true;
	}

	if (strcmp(name, "cyan") == 0) {
		r = 0; g = 255; b = 255; return true;
	}

	if (strcmp(name, "yellow") == 0) {
		r = 255; g = 255; b = 0; return true;
	}

	if (strcmp(name, "purple") == 0) {
		r = 255; g = 0; b = 255; return true;
	}

	if (strcmp(name, "white") == 0) {
		r = 255; g = 255; b = 255; return true;
	}

	return false;
}

void NeopixelFx::hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b)
{
	// Integer HSV->RGB, h in [0,359], s,v in [0,255]
	const uint16_t region = h / 60U;
	const uint16_t remainder = (h - (region * 60U)) * 255U / 60U;
	const uint16_t p = ((uint16_t)v * (255U - s)) / 255U;
	const uint16_t q = ((uint16_t)v * (255U - ((uint16_t)s * remainder) / 255U)) / 255U;
	const uint16_t t = ((uint16_t)v * (255U - ((uint16_t)s * (255U - remainder)) / 255U)) / 255U;

	switch (region % 6U) {
	case 0: r = v;           g = (uint8_t)t; b = (uint8_t)p; break;
	case 1: r = (uint8_t)q;  g = v;          b = (uint8_t)p; break;
	case 2: r = (uint8_t)p;  g = v;          b = (uint8_t)t; break;
	case 3: r = (uint8_t)p;  g = (uint8_t)q; b = v;          break;
	case 4: r = (uint8_t)t;  g = (uint8_t)p; b = v;          break;
	default:r = v;           g = (uint8_t)p; b = (uint8_t)q; break;
	}
}

NeopixelFx *NeopixelFx::instantiate(int argc, char *argv[])
{
	Effect effect = Effect::Rainbow;
	uint32_t period_ms = 1200;
	uint8_t led_index = 0;
	uint8_t priority = 2;
	uint8_t base_r = 0;
	uint8_t base_g = 0;
	uint8_t base_b = 255;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:t:l:p:c:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			if (!parse_effect(myoptarg, effect)) {
				PX4_ERR("invalid -m %s (rainbow|police|breathe)", myoptarg);
				return nullptr;
			}
			break;

		case 't': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 100 || v > 10000) {
					PX4_ERR("invalid -t %ld (100..10000 ms)", v);
					return nullptr;
				}

				period_ms = (uint32_t)v;
				break;
			}

		case 'l': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 0 || v > 7) {
					PX4_ERR("invalid -l %ld (0..7)", v);
					return nullptr;
				}

				led_index = (uint8_t)v;
				break;
			}

		case 'p': {
				const long v = strtol(myoptarg, nullptr, 10);

				if (v < 0 || v > kMaxPriority) {
					PX4_ERR("invalid -p %ld (0..%u)", v, kMaxPriority);
					return nullptr;
				}

				priority = (uint8_t)v;
				break;
			}

		case 'c':
			if (!parse_color(myoptarg, base_r, base_g, base_b)) {
				PX4_ERR("invalid -c %s (red|green|blue|cyan|yellow|purple|white)", myoptarg);
				return nullptr;
			}
			break;

		default:
			return nullptr;
		}
	}

	NeopixelFx *instance = new NeopixelFx(effect, period_ms, led_index, priority, base_r, base_g, base_b);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
	}

	return instance;
}

void NeopixelFx::write_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	_led_data[0].R() = r;
	_led_data[0].G() = g;
	_led_data[0].B() = b;
	(void)neopixel_write(_led_data, 1);
}

void NeopixelFx::write_off()
{
	write_rgb(0, 0, 0);
}

void NeopixelFx::run()
{
	_step = 0;
	const hrt_abstime start_us = hrt_absolute_time();
	const uint32_t frame_us = 20000U; // 50 Hz update

	if (neopixel_set_led_control_enabled(false) != PX4_OK) {
		PX4_ERR("neopixel driver not running (start with: neopixel start -n 1)");
		return;
	}

	while (!should_exit()) {
		const uint64_t elapsed = (uint64_t)(hrt_absolute_time() - start_us);
		const uint32_t period = (_period_us > 0) ? _period_us : 1U;
		const uint32_t phase_us = (uint32_t)(elapsed % period);

		switch (_effect) {
		case Effect::Rainbow: {
				const uint16_t hue = (uint16_t)((phase_us * 360ULL) / period);
				uint8_t r = 0, g = 0, b = 0;
				hsv_to_rgb(hue, 255, 255, r, g, b);
				write_rgb(r, g, b);
				break;
			}

		case Effect::Police: {
				// 4 phases: red on, off, blue on, off
				const uint32_t quarter = period / 4U;
				const uint32_t p = (quarter > 0) ? (phase_us / quarter) : 0U;
				switch (p % 4U) {
				case 0: write_rgb(255, 0, 0); break;
				case 1: write_off(); break;
				case 2: write_rgb(0, 0, 255); break;
				default: write_off(); break;
				}
				break;
			}

		case Effect::Breathe: {
				// Smooth triangle wave 0..1..0 across one period, with easing.
				const float x = (float)phase_us / (float)period; // [0,1)
				const float tri = (x < 0.5f) ? (x * 2.f) : ((1.f - x) * 2.f);
				const float eased = tri * tri * (3.f - 2.f * tri); // smoothstep
				const uint8_t r = (uint8_t)((float)_base_r * eased);
				const uint8_t g = (uint8_t)((float)_base_g * eased);
				const uint8_t b = (uint8_t)((float)_base_b * eased);
				write_rgb(r, g, b);
				break;
			}
		}

		_step++;
		px4_usleep(frame_us);
	}

	write_off();
	(void)neopixel_set_led_control_enabled(true);
}

int NeopixelFx::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Smooth neopixel effect generator with time-based interpolation.

Effects:
- `rainbow`: continuous HSV rainbow
- `police`: alternating red/blue flash
- `breathe`: smooth fade in/out of selected base color

This module writes directly to the neopixel backend (true RGB frames).

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("neopixel_fx", "example");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "rainbow", "rainbow|police|breathe", "Effect mode", true);
	PRINT_MODULE_USAGE_PARAM_INT('t', 1200, 100, 10000, "Effect period in milliseconds", true);
	PRINT_MODULE_USAGE_PARAM_INT('l', 0, 0, 7, "LED index", true);
	PRINT_MODULE_USAGE_PARAM_INT('p', 2, 0, kMaxPriority, "LED priority", true);
	PRINT_MODULE_USAGE_PARAM_STRING('c', "blue", "red|green|blue|cyan|yellow|purple|white", "Base color for breathe", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

int neopixel_fx_main(int argc, char *argv[])
{
	return ModuleBase::main(NeopixelFx::desc, argc, argv);
}
