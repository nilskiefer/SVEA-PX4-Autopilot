/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 ****************************************************************************/

#include <drivers/device/Device.hpp>
#include <drivers/drv_sensor.h>
#include <drivers/drv_hrt.h>
#include <inttypes.h>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/defines.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>
#include <uORB/PublicationMulti.hpp>
#include <uORB/topics/sensor_baro.h>

using namespace time_literals;

extern "C" __EXPORT int svea_baro_sim_main(int argc, char *argv[]);

class SveaBaroSim : public ModuleBase<SveaBaroSim>, public px4::ScheduledWorkItem
{
public:
	SveaBaroSim() :
		ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default)
	{
		device::Device::DeviceId device_id{};
		device_id.devid_s.bus_type = device::Device::DeviceBusType_SIMULATION;
		device_id.devid_s.bus = 1;
		device_id.devid_s.address = 4;
		device_id.devid_s.devtype = DRV_BARO_DEVTYPE_BAROSIM;
		_sensor_baro.device_id = device_id.devid;
	}

	~SveaBaroSim() override
	{
		ScheduleClear();
		perf_free(_cycle_perf);
	}

	static int task_spawn(int argc, char *argv[])
	{
		SveaBaroSim *instance = new SveaBaroSim();

		if (instance) {
			_object.store(instance);
			_task_id = task_id_is_work_queue;

			if (instance->init()) {
				return PX4_OK;
			}

		} else {
			PX4_ERR("alloc failed");
		}

		delete instance;
		_object.store(nullptr);
		_task_id = -1;
		return PX4_ERROR;
	}

	static int custom_command(int argc, char *argv[]) { return print_usage("unknown command"); }

	static int print_usage(const char *reason = nullptr)
	{
		if (reason) {
			PX4_WARN("%s", reason);
		}

		PRINT_MODULE_DESCRIPTION(
			R"DESCR_STR(
### Description
Publishes the standard sensor_baro uORB topic for PX4_UORB_TUNNEL validation.
)DESCR_STR");

		PRINT_MODULE_USAGE_NAME("svea_baro_sim", "system");
		PRINT_MODULE_USAGE_COMMAND("start");
		PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
		return PX4_OK;
	}

	bool init()
	{
		_sensor_baro_pub.advertise();
		ScheduleOnInterval(kSampleIntervalUs);
		return true;
	}

	void Run() override
	{
		if (should_exit()) {
			ScheduleClear();
			exit_and_cleanup();
			return;
		}

		perf_begin(_cycle_perf);

		_rng_state = (_rng_state * 1664525u) + 1013904223u;
		const float unit = static_cast<float>((_rng_state >> 16) & 0xffffu) * (1.f / 65535.f);
		const hrt_abstime now = hrt_absolute_time();

		_sensor_baro.timestamp_sample = now;
		_sensor_baro.timestamp = now;
		_sensor_baro.pressure = 101250.f + (unit * 150.f);
		_sensor_baro.temperature = 20.f + (unit * 4.f);
		_sensor_baro.error_count = 0;
		_sensor_baro_pub.publish(_sensor_baro);
		_sequence++;

		perf_end(_cycle_perf);
	}

	int print_status() override
	{
		perf_print_counter(_cycle_perf);
		PX4_INFO("publishing sensor_baro at 4 Hz, sequence=%" PRIu32 ", pressure=%.2f Pa, temperature=%.3f C",
			 _sequence, static_cast<double>(_sensor_baro.pressure), static_cast<double>(_sensor_baro.temperature));
		return PX4_OK;
	}

private:
	static constexpr hrt_abstime kSampleIntervalUs = 250_ms;

	uORB::PublicationMulti<sensor_baro_s> _sensor_baro_pub{ORB_ID(sensor_baro)};
	sensor_baro_s _sensor_baro{};
	uint32_t _rng_state{0x5e8a201u};
	uint32_t _sequence{0};
	perf_counter_t _cycle_perf{perf_alloc(PC_ELAPSED, MODULE_NAME": cycle")};
};

int svea_baro_sim_main(int argc, char *argv[])
{
	return SveaBaroSim::main(argc, argv);
}
