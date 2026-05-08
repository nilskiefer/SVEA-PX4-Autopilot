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
#include <uORB/SubscriptionCallback.hpp>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/failsafe_flags.h>
#include <unistd.h>

extern "C" __EXPORT int svea_power_gate_main(int argc, char *argv[]);

class SveaPowerGate : public ModuleBase<SveaPowerGate>, public px4::ScheduledWorkItem
{
public:
	// Keep this off hp_default: RC and manual control also run there, and this
	// module intentionally sleeps while sequencing rails.
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
	static constexpr useconds_t kRailStaggerUsON = 500000;
	static constexpr useconds_t kRailStaggerUsOFF = 50000;

	int write_gpio(const char *dev, bool high);
	bool init();
	void apply_power(bool on);

	uORB::SubscriptionCallbackWorkItem _actuator_armed_sub{this, ORB_ID(actuator_armed)};
	uORB::SubscriptionCallbackWorkItem _failsafe_flags_sub{this, ORB_ID(failsafe_flags)};
	bool _requested_on{false};
	bool _rails_on{false};
	bool _manual_control_signal_lost{false};
};

int SveaPowerGate::write_gpio(const char *dev, bool high)
{
	PX4_DEBUG("gpio write begin: dev=%s target=%d", dev, high ? 1 : 0);
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
	PX4_DEBUG("gpio write ok: dev=%s value=%d", dev, high ? 1 : 0);
	return PX4_OK;
}

void SveaPowerGate::apply_power(bool on)
{
	PX4_DEBUG("apply_power begin: request=%d prev_requested=%d rails_on=%d rc_lost=%d",
		  on ? 1 : 0, _requested_on ? 1 : 0, _rails_on ? 1 : 0, _manual_control_signal_lost ? 1 : 0);
	_requested_on = on;

	int servo = PX4_ERROR;
	int esc = PX4_ERROR;

	if (on) {
		// Enable servo rail first, then ESC rail after a short delay.
		servo = write_gpio(kServoEnDev, true);
		usleep(kRailStaggerUsON);
		esc = write_gpio(kEscEnDev, true);

	} else {
		// Keep ESC enabled on manual kill; disable ESC only on manual control loss.
		if (_manual_control_signal_lost) {
			esc = write_gpio(kEscEnDev, false);

		} else {
			esc = PX4_OK;
		}

		usleep(kRailStaggerUsOFF);
		servo = write_gpio(kServoEnDev, false);
	}

	if (esc == PX4_OK && servo == PX4_OK) {
		_rails_on = on;
		PX4_DEBUG("power rails: ESC=%d ServoTPS=%d", on ? 1 : 0, on ? 1 : 0);

	} else {
		PX4_WARN("apply_power incomplete: request=%d esc_ret=%d servo_ret=%d",
			 on ? 1 : 0, esc, servo);
	}
}

bool SveaPowerGate::init()
{
	if (!_actuator_armed_sub.registerCallback()) {
		PX4_ERR("actuator_armed callback registration failed");
		return false;
	}

	if (!_failsafe_flags_sub.registerCallback()) {
		PX4_ERR("failsafe_flags callback registration failed");
		_actuator_armed_sub.unregisterCallback();
		return false;
	}

	return true;
}

void SveaPowerGate::Run()
{
	if (should_exit()) {
		PX4_DEBUG("Run: should_exit=1, forcing rails off");
		ScheduleClear();
		_actuator_armed_sub.unregisterCallback();
		_failsafe_flags_sub.unregisterCallback();
		apply_power(false);
		exit_and_cleanup();
		return;
	}

	if (_failsafe_flags_sub.updated()) {
		failsafe_flags_s failsafe_flags{};

		if (_failsafe_flags_sub.copy(&failsafe_flags)) {
			_manual_control_signal_lost = failsafe_flags.manual_control_signal_lost;
		}
	}

	if (_actuator_armed_sub.updated()) {
		actuator_armed_s actuator_armed{};

		if (_actuator_armed_sub.copy(&actuator_armed)) {
			// Keep original gating behavior; kill still requests power-off path.
			const bool should_enable = actuator_armed.armed && !actuator_armed.lockdown
						   && !actuator_armed.manual_lockdown && !actuator_armed.force_failsafe;
			PX4_DEBUG("armed update: armed=%d prearmed=%d ready=%d lockdown=%d manual_lockdown=%d in_esc_cal=%d force_failsafe=%d rc_lost=%d -> should_enable=%d",
				  actuator_armed.armed ? 1 : 0,
				  actuator_armed.prearmed ? 1 : 0,
				  actuator_armed.ready_to_arm ? 1 : 0,
				  actuator_armed.lockdown ? 1 : 0,
				  actuator_armed.manual_lockdown ? 1 : 0,
				  actuator_armed.in_esc_calibration_mode ? 1 : 0,
				  actuator_armed.force_failsafe ? 1 : 0,
				  _manual_control_signal_lost ? 1 : 0,
				  should_enable ? 1 : 0);

			if (should_enable != _requested_on) {
				PX4_DEBUG("state change: requested_on %d -> %d", _requested_on ? 1 : 0, should_enable ? 1 : 0);
				_requested_on = should_enable;
				apply_power(should_enable);
			}
		}
	}
}

int SveaPowerGate::task_spawn(int argc, char *argv[])
{
	PX4_DEBUG("task_spawn: argc=%d", argc);
	SveaPowerGate *instance = instantiate(argc, argv);

	if (instance == nullptr) {
		return PX4_ERROR;
	}

	_object.store(instance);
	_task_id = task_id_is_work_queue;

	if (!instance->init()) {
		delete instance;
		_object.store(nullptr);
		_task_id = -1;
		return PX4_ERROR;
	}

	// Initialize to safe state until we observe arming.
	PX4_DEBUG("task_spawn: initialize safe state (rails off), schedule now");
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
	PX4_INFO("requested_on=%d rails_on=%d rc_lost=%d esc_dev=%s servo_dev=%s",
		 _requested_on ? 1 : 0, _rails_on ? 1 : 0, _manual_control_signal_lost ? 1 : 0, kEscEnDev, kServoEnDev);
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

	PRINT_MODULE_USAGE_NAME("svea_power_gate", "system");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return PX4_OK;
}

int svea_power_gate_main(int argc, char *argv[])
{
	return SveaPowerGate::main(argc, argv);
}
