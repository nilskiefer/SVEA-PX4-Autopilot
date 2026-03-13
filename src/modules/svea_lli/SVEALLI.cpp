#include "SVEALLI.hpp"

#include <mathlib/mathlib.h>
#include <px4_platform_common/log.h>

#include <cmath>
#include <cstdlib>

using namespace time_literals;

namespace
{
constexpr uint16_t kServoNeutralUs{1500};

constexpr uint16_t kGearHighUs{1100};
constexpr uint16_t kGearLowUs{1900};

constexpr uint16_t kDiffEngagedFrontUs{1900};
constexpr uint16_t kDiffEngagedRearUs{1100};
constexpr uint16_t kDiffOpenFrontUs{1100};
constexpr uint16_t kDiffOpenRearUs{1900};

constexpr uint32_t kRosThrottleMaxAgeUs{100000};

constexpr float kThrottlePGain{0.02f};
constexpr float kThrottleMaxDeltaUs{15.f};
constexpr uint32_t kRemoteThrottleDeadbandUs{10};

constexpr uint16_t kOverrideRosMaxUs{1400};
constexpr uint16_t kOverrideMuteMaxUs{1600};

constexpr uint16_t kConnectedBandLowUs{1400};
constexpr uint16_t kConnectedBandHighUs{1600};
constexpr uint16_t kDiffToggleHighThresholdUs{1200};
constexpr hrt_abstime kRcMaxAgeUs{300_ms};

constexpr bool kForceLowGearOutput{true};
constexpr bool kForceDiffEngagedOutput{true};
const float kFloatNaN = NAN;

uint16_t clampPulseUs(int32_t pulse)
{
	return static_cast<uint16_t>(math::constrain(pulse, int32_t{1000}, int32_t{2000}));
}

uint16_t int8ToUs(int8_t value)
{
	const int32_t pulse = static_cast<int32_t>(kServoNeutralUs) + (static_cast<int32_t>(value) * 500) / 127;
	return clampPulseUs(pulse);
}

uint16_t int8ToThrottleUs(int8_t value)
{
	const int32_t pulse = static_cast<int32_t>(kServoNeutralUs) + (static_cast<int32_t>(value) * 500) / 127;
	return clampPulseUs(pulse);
}

int8_t pulseToInt8(int32_t pulse)
{
	const int32_t bounded = math::constrain(pulse, int32_t{1000}, int32_t{2000});
	const int32_t value = ((bounded - static_cast<int32_t>(kServoNeutralUs)) * 127) / 500;
	return static_cast<int8_t>(math::constrain(value, int32_t{-127}, int32_t{127}));
}

float pulseToNormalized(uint16_t pulse)
{
	return math::constrain((static_cast<float>(pulse) - static_cast<float>(kServoNeutralUs)) / 500.f, -1.f, 1.f);
}
}

ModuleBase::Descriptor SVEALLI::desc{task_spawn, custom_command, print_usage};

SVEALLI::SVEALLI() :
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default)
{
}

SVEALLI::~SVEALLI()
{
	perf_free(_loop_perf);
	perf_free(_loop_interval_perf);
}

bool SVEALLI::init()
{
	publishNeutralOutputs();
	ScheduleOnInterval(kLoopIntervalUs);

	return true;
}

void SVEALLI::updateSubscriptions()
{
	_input_rc_sub.update(&_input_rc);
	_failsafe_flags_sub.update(&_failsafe_flags);
}

void SVEALLI::updateRosControlFromRoverTopics()
{
	hrt_abstime newest_update{0};

	rover_steering_setpoint_s rover_steering_setpoint{};
	rover_throttle_setpoint_s rover_throttle_setpoint{};

	if (_rover_steering_setpoint_sub.update(&rover_steering_setpoint)) {
		newest_update = math::max(newest_update, rover_steering_setpoint.timestamp);

		if (PX4_ISFINITE(rover_steering_setpoint.normalized_steering_setpoint)) {
			const float steering = math::constrain(rover_steering_setpoint.normalized_steering_setpoint, -1.f, 1.f);
			_ros_ctrl.steering = static_cast<int8_t>(lroundf(steering * 127.f));
		}
	}

	if (_rover_throttle_setpoint_sub.update(&rover_throttle_setpoint)) {
		newest_update = math::max(newest_update, rover_throttle_setpoint.timestamp);

		if (PX4_ISFINITE(rover_throttle_setpoint.throttle_body_x)) {
			const float throttle = math::constrain(rover_throttle_setpoint.throttle_body_x, -1.f, 1.f);
			_ros_ctrl.throttle = static_cast<int8_t>(lroundf(throttle * 127.f));
		}
	}

	if (newest_update > 0) {
		_ros_ctrl.timestamp = newest_update;
	}
}

uint16_t SVEALLI::rcChannelUs(unsigned channel_1_based, uint16_t fallback) const
{
	if (channel_1_based == 0) {
		return fallback;
	}

	const unsigned index = channel_1_based - 1;

	if (index < _input_rc.channel_count) {
		const uint16_t pulse = _input_rc.values[index];

		if (pulse >= 750 && pulse <= 2250) {
			return pulse;
		}
	}

	return fallback;
}

bool SVEALLI::remoteConnected() const
{
	if (_input_rc.timestamp_last_signal == 0) {
		return false;
	}

	if (_input_rc.rc_lost || _failsafe_flags.manual_control_signal_lost) {
		return false;
	}

	if (hrt_elapsed_time(&_input_rc.timestamp_last_signal) > kRcMaxAgeUs) {
		return false;
	}

	const uint16_t ch4 = rcChannelUs(4, kServoNeutralUs);
	return (ch4 < kConnectedBandLowUs || ch4 > kConnectedBandHighUs);
}

SVEALLI::OverrideMode SVEALLI::getOverrideMode() const
{
	const uint16_t override_us = rcChannelUs(5, kServoNeutralUs);

	if (override_us < kOverrideRosMaxUs) {
		return OverrideMode::Ros;
	}

	if (override_us <= kOverrideMuteMaxUs) {
		return OverrideMode::Mute;
	}

	return OverrideMode::Remote;
}

bool SVEALLI::consumeDiffToggleEvent()
{
	const bool high = (rcChannelUs(4, kServoNeutralUs) > kDiffToggleHighThresholdUs);
	const bool event = high && !_last_ch4_high;
	_last_ch4_high = high;
	return event;
}

void SVEALLI::publishNeutralOutputs()
{
	actuator_servos_s actuator_servos{};
	for (float &value : actuator_servos.control) {
		value = kFloatNaN;
	}
	actuator_servos.control[0] = 0.f;
	actuator_servos.control[1] = pulseToNormalized(kGearLowUs);
	actuator_servos.control[2] = pulseToNormalized(kDiffEngagedFrontUs);
	actuator_servos.control[3] = pulseToNormalized(kDiffEngagedRearUs);
	actuator_servos.timestamp = hrt_absolute_time();
	_actuator_servos_pub.publish(actuator_servos);

	actuator_motors_s actuator_motors{};
	for (float &value : actuator_motors.control) {
		value = kFloatNaN;
	}
	actuator_motors.reversible_flags = 1u;
	actuator_motors.control[0] = 0.f;
	actuator_motors.timestamp = hrt_absolute_time();
	_actuator_motors_pub.publish(actuator_motors);

	_actuated_throttle = kServoNeutralUs;
}

void SVEALLI::publishOutputs(uint16_t steering_us, uint16_t throttle_us, uint16_t gear_us, uint16_t diff_front_us, uint16_t diff_rear_us)
{
	actuator_servos_s actuator_servos{};
	for (float &value : actuator_servos.control) {
		value = kFloatNaN;
	}
	actuator_servos.control[0] = pulseToNormalized(steering_us);
	actuator_servos.control[1] = pulseToNormalized(gear_us);
	actuator_servos.control[2] = pulseToNormalized(diff_front_us);
	actuator_servos.control[3] = pulseToNormalized(diff_rear_us);
	actuator_servos.timestamp = hrt_absolute_time();
	_actuator_servos_pub.publish(actuator_servos);

	actuator_motors_s actuator_motors{};
	for (float &value : actuator_motors.control) {
		value = kFloatNaN;
	}
	actuator_motors.reversible_flags = 1u;
	actuator_motors.control[0] = pulseToNormalized(throttle_us);
	actuator_motors.timestamp = actuator_servos.timestamp;
	_actuator_motors_pub.publish(actuator_motors);
}

void SVEALLI::Run()
{
	if (should_exit()) {
		ScheduleClear();
		publishNeutralOutputs();
		exit_and_cleanup(desc);
		return;
	}

	perf_begin(_loop_perf);
	perf_count(_loop_interval_perf);

	updateSubscriptions();
	updateRosControlFromRoverTopics();

	const bool connected = remoteConnected();
	const OverrideMode override_mode = getOverrideMode();

	uint16_t steer_us = kServoNeutralUs;
	uint16_t thr_target_us = kServoNeutralUs;
	bool high_gear = false;
	bool diff_engaged = _diff_state;

	if (connected && override_mode != OverrideMode::Mute) {
		switch (override_mode) {
		case OverrideMode::Remote: {
			steer_us = rcChannelUs(1, kServoNeutralUs);

			const uint16_t rc_throttle_us = clampPulseUs(rcChannelUs(2, kServoNeutralUs));
			const uint16_t inverted_throttle_us = clampPulseUs(3000 - static_cast<int32_t>(rc_throttle_us));
			thr_target_us = inverted_throttle_us;

			if (std::abs(static_cast<int32_t>(thr_target_us) - static_cast<int32_t>(kServoNeutralUs)) < static_cast<int32_t>(kRemoteThrottleDeadbandUs)) {
				thr_target_us = kServoNeutralUs;
			}

			high_gear = (rcChannelUs(6, kServoNeutralUs) > kServoNeutralUs);

			_ros_ctrl.steering = pulseToInt8(steer_us);

			const int32_t thr_delta = static_cast<int32_t>(kServoNeutralUs) - static_cast<int32_t>(thr_target_us);
			const int32_t thr_value = math::constrain((thr_delta * 127) / 500, int32_t{-127}, int32_t{127});
			_ros_ctrl.throttle = static_cast<int8_t>(thr_value);
			_ros_ctrl.high_gear = high_gear;

			if (consumeDiffToggleEvent()) {
				_diff_state = !_diff_state;
			}

			_ros_ctrl.diff = _diff_state;
			diff_engaged = _diff_state;
			break;
		}

		case OverrideMode::Ros: {
			steer_us = int8ToUs(_ros_ctrl.steering);
			thr_target_us = kServoNeutralUs;

			if (_ros_ctrl.timestamp != 0 && hrt_elapsed_time(&_ros_ctrl.timestamp) < kRosThrottleMaxAgeUs) {
				thr_target_us = int8ToThrottleUs(_ros_ctrl.throttle);
			}

			high_gear = _ros_ctrl.high_gear;
			_diff_state = _ros_ctrl.diff;

			if (consumeDiffToggleEvent()) {
				_diff_state = !_diff_state;
			}

			diff_engaged = _diff_state;
			break;
		}

		case OverrideMode::Mute:
		default:
			break;
		}

		const uint16_t gear_us = high_gear ? kGearHighUs : kGearLowUs;
		const uint16_t diff_front_us = diff_engaged ? kDiffEngagedFrontUs : kDiffOpenFrontUs;
		const uint16_t diff_rear_us = diff_engaged ? kDiffEngagedRearUs : kDiffOpenRearUs;

		const float error = static_cast<float>(thr_target_us) - static_cast<float>(_actuated_throttle);
		float step = kThrottlePGain * error;
		step = math::constrain(step, -kThrottleMaxDeltaUs, kThrottleMaxDeltaUs);

		_actuated_throttle = static_cast<uint16_t>(math::constrain(static_cast<int>(lroundf(static_cast<float>(_actuated_throttle) + step)), 1001, 1999));

		const uint16_t applied_gear_us = kForceLowGearOutput ? kGearLowUs : gear_us;
		const uint16_t applied_diff_front_us = kForceDiffEngagedOutput ? kDiffEngagedFrontUs : diff_front_us;
		const uint16_t applied_diff_rear_us = kForceDiffEngagedOutput ? kDiffEngagedRearUs : diff_rear_us;
		publishOutputs(steer_us, _actuated_throttle, applied_gear_us, applied_diff_front_us, applied_diff_rear_us);

	} else {
		publishNeutralOutputs();
	}

	perf_end(_loop_perf);
}

int SVEALLI::task_spawn(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	SVEALLI *instance = new SVEALLI();

	if (instance) {
		desc.object.store(instance);
		desc.task_id = task_id_is_work_queue;

		if (instance->init()) {
			return PX4_OK;
		}

	} else {
		PX4_ERR("alloc failed");
	}

	delete instance;
	desc.object.store(nullptr);
	desc.task_id = -1;

	return PX4_ERROR;
}

int SVEALLI::print_status()
{
	perf_print_counter(_loop_perf);
	perf_print_counter(_loop_interval_perf);
	PX4_INFO("publishing to actuator pipeline, override channel=5, diff channel=4, gear channel=6");
	return 0;
}

int SVEALLI::custom_command(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return print_usage("unknown command");
}

int SVEALLI::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
SVEA low-level interface module.

The module runs a 100 Hz control loop on a PX4 work queue, consumes PX4 RC/failsafe
status, and publishes actuator setpoints into the PX4 actuator pipeline.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("svea_lli", "module");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

extern "C" __EXPORT int svea_lli_main(int argc, char *argv[])
{
	return ModuleBase::main(SVEALLI::desc, argc, argv);
}
