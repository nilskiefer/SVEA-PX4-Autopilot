#pragma once

#include <px4_platform_common/defines.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <drivers/drv_hrt.h>
#include <lib/perf/perf_counter.h>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/failsafe_flags.h>
#include <uORB/topics/input_rc.h>
#include <uORB/topics/rover_steering_setpoint.h>
#include <uORB/topics/rover_throttle_setpoint.h>
#include <uORB/topics/actuator_motors.h>
#include <uORB/topics/actuator_servos.h>

class SVEALLI : public ModuleBase, public px4::ScheduledWorkItem
{
public:
	static Descriptor desc;

	SVEALLI();
	~SVEALLI() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);

	bool init();
	int print_status() override;

private:
	enum class OverrideMode {
		Ros = 0,
		Mute,
		Remote,
	};

	struct RosControl {
		int8_t steering{0};
		int8_t throttle{0};
		bool high_gear{false};
		bool diff{true};
		hrt_abstime timestamp{0};
	};

	static constexpr uint32_t kControlRateHz{100};
	static constexpr uint32_t kLoopIntervalUs{1000000 / kControlRateHz};

	void Run() override;

	void updateSubscriptions();
	void updateRosControlFromRoverTopics();

	uint16_t rcChannelUs(unsigned channel_1_based, uint16_t fallback = 1500) const;
	bool remoteConnected() const;
	OverrideMode getOverrideMode() const;
	bool consumeDiffToggleEvent();

	void publishNeutralOutputs();
	void publishOutputs(uint16_t steering_us, uint16_t throttle_us, uint16_t gear_us, uint16_t diff_front_us, uint16_t diff_rear_us);

	uORB::Subscription _input_rc_sub{ORB_ID(input_rc)};
	uORB::Subscription _failsafe_flags_sub{ORB_ID(failsafe_flags)};
	uORB::Subscription _rover_steering_setpoint_sub{ORB_ID(rover_steering_setpoint)};
	uORB::Subscription _rover_throttle_setpoint_sub{ORB_ID(rover_throttle_setpoint)};

	input_rc_s _input_rc{};
	failsafe_flags_s _failsafe_flags{};

	RosControl _ros_ctrl{};

	uORB::Publication<actuator_motors_s> _actuator_motors_pub{ORB_ID(actuator_motors)};
	uORB::Publication<actuator_servos_s> _actuator_servos_pub{ORB_ID(actuator_servos)};

	uint16_t _actuated_throttle{1500};
	bool _diff_state{true};
	bool _last_ch4_high{false};

	perf_counter_t _loop_perf{perf_alloc(PC_ELAPSED, MODULE_NAME ": cycle")};
	perf_counter_t _loop_interval_perf{perf_alloc(PC_INTERVAL, MODULE_NAME ": interval")};
};
