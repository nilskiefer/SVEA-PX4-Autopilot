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

#include <math.h>
#include <inttypes.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>
#include <drivers/drv_hrt.h>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/vehicle_command.h>

using namespace time_literals;

extern "C" __EXPORT int svea_rc_servo_latch_main(int argc, char *argv[]);

class SveaRcServoLatch : public ModuleBase, public px4::ScheduledWorkItem
{
public:
	static Descriptor desc;

	SveaRcServoLatch() : ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default) {}
	~SveaRcServoLatch() override = default;

	static int task_spawn(int argc, char *argv[]);
	static SveaRcServoLatch *instantiate(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]) { return print_usage("unknown command"); }
	static int print_usage(const char *reason = nullptr);

	bool init();
	void Run() override;
	int print_status() override;

private:
	void publish_actuator_set();

	uORB::Subscription _manual_control_setpoint_sub{ORB_ID(manual_control_setpoint)};
	uORB::Publication<vehicle_command_s> _vehicle_command_pub{ORB_ID(vehicle_command)};

	float _latched_value[2] {0.f, 0.f};
	int _active_index{0};
	bool _button_last_high{false};
	uint32_t _toggle_count{0};
};

ModuleBase::Descriptor SveaRcServoLatch::desc{task_spawn, custom_command, print_usage};

bool SveaRcServoLatch::init()
{
	ScheduleOnInterval(20_ms);
	return true;
}

void SveaRcServoLatch::publish_actuator_set()
{
	vehicle_command_s command{};
	command.timestamp = hrt_absolute_time();
	command.command = vehicle_command_s::VEHICLE_CMD_DO_SET_ACTUATOR;
	command.param1 = _latched_value[0];
	command.param2 = _latched_value[1];
	command.param3 = NAN;
	command.param4 = NAN;
	command.param5 = NAN;
	command.param6 = NAN;
	command.param7 = 0.f;
	_vehicle_command_pub.publish(command);
}

void SveaRcServoLatch::Run()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup(desc);
		return;
	}

	manual_control_setpoint_s manual_control_setpoint{};

	if (_manual_control_setpoint_sub.update(&manual_control_setpoint)) {
		if (!manual_control_setpoint.valid) {
			_button_last_high = false;
			return;
		}

		// RC behavior:
		// - AUX4 rising edge toggles active bank
		// - AUX5 writes active bank
		if (manual_control_setpoint.data_source == manual_control_setpoint_s::SOURCE_RC) {
			const bool button_high = PX4_ISFINITE(manual_control_setpoint.aux4) && (manual_control_setpoint.aux4 > 0.5f);

			if (button_high && !_button_last_high) {
				_active_index = 1 - _active_index;
				_toggle_count++;
			}

			_button_last_high = button_high;

			if (PX4_ISFINITE(manual_control_setpoint.aux5)) {
				_latched_value[_active_index] = manual_control_setpoint.aux5;
				publish_actuator_set();
			}

		} else if (manual_control_setpoint.data_source >= manual_control_setpoint_s::SOURCE_MAVLINK_0
			   && manual_control_setpoint.data_source <= manual_control_setpoint_s::SOURCE_MAVLINK_5) {
			// MAVLink behavior: direct passthrough (same external contract as prior RC_AUX4/5 mapping).
			bool updated = false;

			if (PX4_ISFINITE(manual_control_setpoint.aux4)) {
				_latched_value[0] = manual_control_setpoint.aux4;
				updated = true;
			}

			if (PX4_ISFINITE(manual_control_setpoint.aux5)) {
				_latched_value[1] = manual_control_setpoint.aux5;
				updated = true;
			}

			if (updated) {
				publish_actuator_set();
			}

			_button_last_high = false;
		}
	}
}

int SveaRcServoLatch::task_spawn(int argc, char *argv[])
{
	SveaRcServoLatch *instance = new SveaRcServoLatch();

	if (instance) {
		desc.object.store(instance);
		desc.task_id = task_id_is_work_queue;

		if (instance->init()) {
			return PX4_OK;
		}
	}

	delete instance;
	desc.object.store(nullptr);
	desc.task_id = -1;
	return PX4_ERROR;
}

SveaRcServoLatch *SveaRcServoLatch::instantiate(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return new SveaRcServoLatch();
}

int SveaRcServoLatch::print_status()
{
	PX4_INFO("active_index=%d latched0=%.3f latched1=%.3f toggles=%" PRIu32,
		 _active_index, (double)_latched_value[0], (double)_latched_value[1], _toggle_count);
	return PX4_OK;
}

int SveaRcServoLatch::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
SVEA RC latch/mux helper:
- CH4 (mapped to AUX4) toggles active servo bank on rising edge
- CH3 (mapped to AUX5) controls only the active bank
- Inactive bank keeps its last latched value
- MAVLink manual-control source directly drives AUX4->bank0 and AUX5->bank1
- Publishes VEHICLE_CMD_DO_SET_ACTUATOR index 0 (param1/param2)
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("svea_rc_servo_latch", "system");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_COMMAND("stop");
	PRINT_MODULE_USAGE_COMMAND("status");
	return 0;
}

extern "C" __EXPORT int svea_rc_servo_latch_main(int argc, char *argv[])
{
	return ModuleBase::main(SveaRcServoLatch::desc, argc, argv);
}
