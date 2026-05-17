#include <px4_platform_common/module.h>
#include <px4_platform_common/module_params.h>
#include <px4_platform_common/log.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <lib/mathlib/mathlib.h>
#include <lib/matrix/matrix/math.hpp>
#include <lib/perf/perf_counter.h>

#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/manual_control_switches.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/wheel_distance.h>

using namespace time_literals;

class SveaSITL : public ModuleBase, public ModuleParams, public px4::ScheduledWorkItem
{
public:
	static Descriptor desc;

	SveaSITL() :
		ModuleParams(nullptr),
		ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default)
	{
	}

	~SveaSITL() override
	{
		perf_free(_loop_perf);
	}

	bool init()
	{
		ScheduleOnInterval(20_ms);
		return true;
	}

	static int task_spawn(int argc, char *argv[])
	{
		SveaSITL *instance = new SveaSITL();

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

	static int custom_command(int argc, char *argv[])
	{
		return print_usage("unknown command");
	}

	static int print_usage(const char *reason = nullptr)
	{
		if (reason) {
			PX4_WARN("%s", reason);
		}

		PRINT_MODULE_DESCRIPTION(
			R"DESCR_STR(
### Description
SVEA SITL helper:
- publishes neutral RC input/switch state so manual control falls back cleanly when MAVLink input disappears
- republishes local_position from groundtruth with ENU/NED conversion for SVEA rover visualization
)DESCR_STR");

		PRINT_MODULE_USAGE_NAME("svea_sitl", "system");
		PRINT_MODULE_USAGE_COMMAND("start");
		PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
		return 0;
	}

	void Run() override
	{
		if (should_exit()) {
			ScheduleClear();
			exit_and_cleanup(desc);
			return;
		}

		perf_begin(_loop_perf);

		const hrt_abstime now = hrt_absolute_time();
		bridge_local_position(now);
		publish_rc_fallback_if_mavlink_lost(now);
		publish_wheel_distance(now);

		perf_end(_loop_perf);
	}

private:
	void publish_rc_fallback_if_mavlink_lost(const hrt_abstime now)
	{
		manual_control_setpoint_s mavlink_manual{};

		if (_manual_control_mavlink_sub.copy(&mavlink_manual)
		    && mavlink_manual.valid
		    && (mavlink_manual.data_source >= manual_control_setpoint_s::SOURCE_MAVLINK_0)
		    && (mavlink_manual.data_source <= manual_control_setpoint_s::SOURCE_MAVLINK_5)) {
			_last_mavlink_manual_input = mavlink_manual.timestamp;
		}

		if (_last_mavlink_manual_input != 0
		    && (now - _last_mavlink_manual_input) <= _mavlink_manual_timeout) {
			// MAVLink manual stream is alive: do not override it with fallback RC.
			return;
		}

		manual_control_switches_s sw{};
		sw.timestamp = now;
		sw.timestamp_sample = now;
		sw.mode_slot = manual_control_switches_s::MODE_SLOT_1;
		sw.arm_switch = manual_control_switches_s::SWITCH_POS_OFF;
		sw.return_switch = manual_control_switches_s::SWITCH_POS_OFF;
		sw.loiter_switch = manual_control_switches_s::SWITCH_POS_OFF;
		sw.offboard_switch = manual_control_switches_s::SWITCH_POS_OFF;
		sw.kill_switch = manual_control_switches_s::SWITCH_POS_OFF;
		sw.gear_switch = manual_control_switches_s::SWITCH_POS_NONE;
		sw.transition_switch = manual_control_switches_s::SWITCH_POS_NONE;
		sw.photo_switch = manual_control_switches_s::SWITCH_POS_NONE;
		sw.video_switch = manual_control_switches_s::SWITCH_POS_NONE;
		sw.payload_power_switch = manual_control_switches_s::SWITCH_POS_NONE;
		sw.engage_main_motor_switch = manual_control_switches_s::SWITCH_POS_NONE;
		sw.termination_switch = manual_control_switches_s::SWITCH_POS_OFF;
		_manual_control_switches_pub.publish(sw);

		manual_control_setpoint_s rc{};
		rc.timestamp = now;
		rc.timestamp_sample = now;
		rc.valid = true;
		rc.data_source = manual_control_setpoint_s::SOURCE_RC;
		rc.sticks_moving = false;
		rc.roll = 0.f;
		rc.pitch = 0.f;
		rc.yaw = 0.f;
		rc.throttle = 0.f;
		rc.flaps = 0.f;
		rc.aux1 = -1.f;
		rc.aux2 = -1.f;
		rc.aux3 = -1.f;
		rc.aux4 = 0.f;
		rc.aux5 = 0.f;
		rc.aux6 = 0.f;
		_manual_control_input_pub.publish(rc);
	}

	void bridge_local_position(const hrt_abstime now)
	{
		vehicle_local_position_s lpos_gt{};

		if (!_lpos_gt_sub.update(&lpos_gt)) {
			if ((now - _last_groundtruth_warn) > 1_s) {
				PX4_WARN("vehicle_local_position_groundtruth missing; no SVEA SITL pose bridge");
				_last_groundtruth_warn = now;
			}

			return;
		}

		vehicle_local_position_s lpos = lpos_gt;

		// ENU -> NED for rover tf consistency with ROS base_link convention.
		lpos.x = lpos_gt.y;
		lpos.y = lpos_gt.x;
		lpos.z = -lpos_gt.z;
		lpos.vx = lpos_gt.vy;
		lpos.vy = lpos_gt.vx;
		lpos.vz = -lpos_gt.vz;
		lpos.ax = lpos_gt.ay;
		lpos.ay = lpos_gt.ax;
		lpos.az = -lpos_gt.az;

		lpos.heading = matrix::wrap_pi(M_PI_F / 2.f - lpos_gt.heading);
		lpos.unaided_heading = lpos.heading;
		lpos.delta_heading = 0.f;

		lpos.xy_valid = true;
		lpos.z_valid = true;
		lpos.v_xy_valid = true;
		lpos.v_z_valid = true;
		lpos.heading_good_for_control = true;
		lpos.dead_reckoning = false;
		lpos.timestamp = now;

		_lpos_pub.publish(lpos);
	}

	void publish_wheel_distance(const hrt_abstime now)
	{
		if (_last_wheel_pub == 0) {
			_last_wheel_pub = now;
			return;
		}

		const float dt = (now - _last_wheel_pub) * 1e-6f;
		_last_wheel_pub = now;

		// Use EKF local-position speed as encoder proxy; do not override estimator outputs.
		vehicle_local_position_s lpos{};

		if (!_lpos_sub.copy(&lpos)) {
			if ((now - _last_groundtruth_warn) > 1_s) {
				PX4_WARN("vehicle_local_position unavailable; skipping wheel encoder update");
				_last_groundtruth_warn = now;
			}
			return;
		}

		if (!PX4_ISFINITE(lpos.vx) || !lpos.v_xy_valid) {
			return;
		}

		const float forward_speed_m_s = lpos.vx;
		_wheel_distance_left_m += forward_speed_m_s * dt;
		_wheel_distance_right_m += forward_speed_m_s * dt;

		wheel_distance_s msg{};
		msg.timestamp = now;
		msg.timestamp_sample = lpos.timestamp_sample;
		msg.sequence = _wheel_sequence++;
		msg.left_distance_m = _wheel_distance_left_m;
		msg.right_distance_m = _wheel_distance_right_m;
		_wheel_distance_pub.publish(msg);
	}

	uORB::Subscription _lpos_gt_sub{ORB_ID(vehicle_local_position_groundtruth)};
	uORB::Subscription _manual_control_mavlink_sub{ORB_ID(manual_control_input), 1};
	uORB::Subscription _lpos_sub{ORB_ID(vehicle_local_position)};

	uORB::Publication<manual_control_switches_s> _manual_control_switches_pub{ORB_ID(manual_control_switches)};
	uORB::Publication<manual_control_setpoint_s> _manual_control_input_pub{ORB_ID(manual_control_input)};
	uORB::Publication<vehicle_local_position_s> _lpos_pub{ORB_ID(vehicle_local_position)};
	uORB::Publication<wheel_distance_s> _wheel_distance_pub{ORB_ID(wheel_distance)};

	hrt_abstime _last_groundtruth_warn{0};
	hrt_abstime _last_mavlink_manual_input{0};
	static constexpr hrt_abstime _mavlink_manual_timeout{300_ms};
	hrt_abstime _last_wheel_pub{0};
	uint32_t _wheel_sequence{0};
	float _wheel_distance_left_m{0.f};
	float _wheel_distance_right_m{0.f};
	perf_counter_t _loop_perf{perf_alloc(PC_ELAPSED, MODULE_NAME ": cycle")};
};

ModuleBase::Descriptor SveaSITL::desc{task_spawn, custom_command, print_usage};

extern "C" __EXPORT int svea_sitl_main(int argc, char *argv[])
{
	return ModuleBase::main(SveaSITL::desc, argc, argv);
}
