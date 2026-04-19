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
#include <px4_platform_common/log.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <errno.h>
#include <fcntl.h>
#include <nuttx/ioexpander/gpio.h>
#include <uORB/Subscription.hpp>
#include <uORB/topics/vehicle_status.h>
#include <unistd.h>

extern "C" __EXPORT int svea_power_gate_main(int argc, char *argv[]);

class SveaPowerGate : public ModuleBase, public px4::ScheduledWorkItem
{
public:
	static Descriptor desc;

	SveaPowerGate() : ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::lp_default) {}
	~SveaPowerGate() override = default;

	static int task_spawn(int argc, char *argv[]);
	static SveaPowerGate *instantiate(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]) { return print_usage("unknown command"); }
	static int print_usage(const char *reason = nullptr);

	void Run() override;
	int print_status() override;

private:
	static constexpr const char *kEscEnDev = "/dev/gpio9";
	static constexpr const char *kServoEnDev = "/dev/gpio10";
	static constexpr useconds_t kRailStaggerUs = 50000;
	static constexpr uint32_t kPollIntervalUs = 100000;

	int write_gpio(const char *dev, bool high);
	void apply_power(bool on);

	uORB::Subscription _vehicle_status_sub{ORB_ID(vehicle_status)};
	bool _armed{false};
	bool _rails_on{false};
};

ModuleBase::Descriptor SveaPowerGate::desc{task_spawn, custom_command, print_usage};

int SveaPowerGate::write_gpio(const char *dev, bool high)
{
	int fd = open(dev, O_RDWR);

	if (fd < 0) {
		PX4_ERR("open %s failed (%d)", dev, errno);
		return -errno;
	}

	int ret = ioctl(fd, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);

	if (ret != 0) {
		const int err = errno;
		PX4_ERR("setpintype %s failed (%d)", dev, err);
		close(fd);
		return -err;
	}

	ret = ioctl(fd, GPIOC_WRITE, high ? 1 : 0);

	if (ret != 0) {
		const int err = errno;
		PX4_ERR("write %s=%d failed (%d)", dev, high ? 1 : 0, err);
		close(fd);
		return -err;
	}

	close(fd);
	return PX4_OK;
}

void SveaPowerGate::apply_power(bool on)
{
	int servo = PX4_ERROR;
	int esc = PX4_ERROR;

	if (on) {
		// Enable servo rail first, then ESC rail after a short delay.
		servo = write_gpio(kServoEnDev, true);

		if (servo == PX4_OK) {
			usleep(kRailStaggerUs);
			esc = write_gpio(kEscEnDev, true);
		}

	} else {
		// Disable ESC rail first, then servo rail after a short delay.
		esc = write_gpio(kEscEnDev, false);

		if (esc == PX4_OK) {
			usleep(kRailStaggerUs);
			servo = write_gpio(kServoEnDev, false);
		}
	}

	if (esc == PX4_OK && servo == PX4_OK) {
		_rails_on = on;
		PX4_INFO("power rails: ESC=%d ServoTPS=%d", on ? 1 : 0, on ? 1 : 0);
	}
}

void SveaPowerGate::Run()
{
	if (should_exit()) {
		ScheduleClear();
		apply_power(false);
		exit_and_cleanup(desc);
		return;
	}

	if (_vehicle_status_sub.updated()) {
		vehicle_status_s status{};

		if (_vehicle_status_sub.copy(&status)) {
			const bool armed = (status.arming_state == vehicle_status_s::ARMING_STATE_ARMED);

			if (armed != _armed) {
				_armed = armed;
				apply_power(_armed);
			}
		}
	}

	ScheduleDelayed(kPollIntervalUs);
}

int SveaPowerGate::task_spawn(int argc, char *argv[])
{
	SveaPowerGate *instance = instantiate(argc, argv);

	if (instance == nullptr) {
		return PX4_ERROR;
	}

	desc.object.store(instance);
	desc.task_id = task_id_is_work_queue;

	// Initialize to safe state until we observe arming.
	instance->apply_power(false);
	instance->ScheduleNow();
	return PX4_OK;
}

SveaPowerGate *SveaPowerGate::instantiate(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return new SveaPowerGate();
}

int SveaPowerGate::print_status()
{
	PX4_INFO("armed=%d rails_on=%d esc_dev=%s servo_dev=%s",
		 _armed ? 1 : 0, _rails_on ? 1 : 0, kEscEnDev, kServoEnDev);
	return PX4_OK;
}

int SveaPowerGate::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
SVEA power gate helper.
Toggles ESC and servo power rails from arming state:
- arm   -> enable /dev/gpio9 and /dev/gpio10
- disarm -> disable /dev/gpio9 and /dev/gpio10
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("svea_power_gate", "example");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return PX4_OK;
}

int svea_power_gate_main(int argc, char *argv[])
{
	return ModuleBase::main(SveaPowerGate::desc, argc, argv);
}
