/****************************************************************************
 *
 *   Copyright (c) 2019-2020 PX4 Development Team. All rights reserved.
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

/**
 * @file svea_ina226.cpp
 * @author David Sidrane <david_s5@usa.net>
 *
 * Driver for the I2C attached INA226 (SVEA wrapper)
 */

#include "svea_ina226.h"

#include <cstring>


SVEA_INA226::SVEA_INA226(const I2CSPIDriverConfig &config) :
	I2C(config),
	ModuleParams(nullptr),
	I2CSPIDriver(config),
	_sample_perf(perf_alloc(PC_ELAPSED, "ina226_read")),
	_comms_errors(perf_alloc(PC_COUNT, "ina226_com_err")),
	_collection_errors(perf_alloc(PC_COUNT, "ina226_collection_err")),
	_measure_errors(perf_alloc(PC_COUNT, "ina226_measurement_err"))
{
	float fvalue = MAX_CURRENT;
	_max_current = fvalue;
	param_t ph = param_find("INA226_CURRENT");

	if (ph != PARAM_INVALID && param_get(ph, &fvalue) == PX4_OK) {
		_max_current = fvalue;
	}

	fvalue = INA226_SHUNT;
	_rshunt = fvalue;
	ph = param_find("INA226_SHUNT");

	if (ph != PARAM_INVALID && param_get(ph, &fvalue) == PX4_OK) {
		_rshunt = fvalue;
	}

	ph = param_find("INA226_CONFIG");
	int32_t value = INA226_CONFIG;
	_config = (uint16_t)value;

	if (ph != PARAM_INVALID && param_get(ph, &value) == PX4_OK) {
		_config = (uint16_t)value;
	}

	// Optional per-instance shunt override from CLI (-r), encoded as nano-ohms in custom2.
	if (config.custom2 > 0) {
		_rshunt = static_cast<float>(config.custom2) * 1e-9f;
	}

	_mode_triggered = ((_config & INA226_MODE_MASK) >> INA226_MODE_SHIFTS) <=
			  ((INA226_MODE_SHUNT_BUS_TRIG & INA226_MODE_MASK) >>
			   INA226_MODE_SHIFTS);

	_current_lsb = _max_current / DN_MAX;
	_power_lsb = 25 * _current_lsb;

	I2C::_retries = 5;
}

SVEA_INA226::~SVEA_INA226()
{
	/* free perf counters */
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_collection_errors);
	perf_free(_measure_errors);
}

int SVEA_INA226::read(uint8_t address, int16_t &data)
{
	// read desired little-endian value via I2C
	uint16_t received_bytes;
	int ret = PX4_ERROR;

	for (size_t i = 0; i < 3; i++) {
		ret = transfer(&address, 1, (uint8_t *)&received_bytes, sizeof(received_bytes));

		if (ret == PX4_OK) {
			data = swap16(received_bytes);
			break;

		} else {
			perf_count(_comms_errors);
			PX4_DEBUG("i2c::transfer returned %d", ret);
		}
	}

	return ret;
}

int SVEA_INA226::write(uint8_t address, uint16_t value)
{
	uint8_t data[3] = {address, ((uint8_t)((value & 0xff00) >> 8)), (uint8_t)(value & 0xff)};
	return transfer(data, sizeof(data), nullptr, 0);
}

int
SVEA_INA226::init()
{
	int ret = PX4_ERROR;

	if (_max_current <= 0.f || _rshunt <= 0.f) {
		PX4_ERR("invalid calibration setup (max_current=%.3fA shunt=%.6fOhm)",
			(double)_max_current, (double)_rshunt);
		return ret;
	}

	/* do I2C init (and probe) first */
	if (I2C::init() != PX4_OK) {
		PX4_ERR("I2C init/probe failed (bus=%u addr=0x%02x)", get_device_bus(), get_device_address());
		return ret;
	}

	if (write(INA226_REG_CONFIGURATION, INA226_RST) < 0) {
		PX4_ERR("failed to write reset (bus=%u addr=0x%02x)", get_device_bus(), get_device_address());
		return ret;
	}

	_cal = INA226_CONST / (_current_lsb * _rshunt);

	if (write(INA226_REG_CALIBRATION, _cal) < 0) {
		PX4_ERR("failed to write calibration (bus=%u addr=0x%02x cal=%d)", get_device_bus(), get_device_address(),
			(int)_cal);
		return -3;
	}

	// If we run in continuous mode then start it here

	if (!_mode_triggered) {
		ret = write(INA226_REG_CONFIGURATION, _config);

	} else {
		ret = PX4_OK;
	}

	if (ret != PX4_OK) {
		PX4_ERR("failed to write config (bus=%u addr=0x%02x cfg=0x%04x)", get_device_bus(), get_device_address(),
			(unsigned)_config);
	}

	start();
	_sensor_ok = true;

	_initialized = ret == PX4_OK;
	return ret;
}

int
SVEA_INA226::force_init()
{
	int ret = init();

	start();

	return ret;
}

int
SVEA_INA226::probe()
{
	int16_t value{0};
	const int mfg_ret = read(INA226_MFG_ID, value);
	const int16_t mfg_id = value;

	if (mfg_ret != PX4_OK || mfg_id != INA226_MFG_ID_TI) {
		PX4_ERR("probe failed: mfg_id=0x%04x (ret=%d, expected=0x%04x)", (unsigned)mfg_id, mfg_ret,
			(unsigned)INA226_MFG_ID_TI);
		return -1;
	}

	const int die_ret = read(INA226_MFG_DIEID, value);
	const int16_t die_id = value;

	if (die_ret != PX4_OK || die_id != INA226_MFG_DIE) {
		PX4_ERR("probe failed: die_id=0x%04x (ret=%d, expected=0x%04x)", (unsigned)die_id, die_ret,
			(unsigned)INA226_MFG_DIE);
		return -1;
	}

	return PX4_OK;
}

int
SVEA_INA226::measure()
{
	int ret = PX4_OK;

	if (_mode_triggered) {
		ret = write(INA226_REG_CONFIGURATION, _config);

		if (ret < 0) {
			perf_count(_comms_errors);
			PX4_DEBUG("i2c::transfer returned %d", ret);
		}
	}

	return ret;
}

int
SVEA_INA226::collect()
{
	perf_begin(_sample_perf);

	if (_parameter_update_sub.updated()) {
		parameter_update_s parameter_update{};
		_parameter_update_sub.copy(&parameter_update);
		updateParams();
	}

	// read from the sensor
	// Note: If the sensor is connected backwards, then _current can be negative but valid.
	int16_t mask_enable{0};
	int16_t alert_limit{0};
	bool success{true};
	success = success && (read(INA226_REG_BUSVOLTAGE, _bus_voltage) == PX4_OK);
	success = success && (read(INA226_REG_POWER, _power) == PX4_OK);
	success = success && (read(INA226_REG_CURRENT, _current) == PX4_OK);
	success = success && (read(INA226_REG_SHUNTVOLTAGE, _shunt) == PX4_OK);
	success = success && (read(INA226_REG_MASKENABLE, mask_enable) == PX4_OK);
	success = success && (read(INA226_REG_ALERTLIMIT, alert_limit) == PX4_OK);

	if (!success) {
		_bus_voltage = 0;
		_power = 0;
		_current = 0;
		_shunt = 0;
		mask_enable = 0;
		alert_limit = 0;
	}

	const float v_bus_minus = static_cast<float>(_bus_voltage) * INA226_VSCALE;
	const float v_shunt = static_cast<float>(_shunt) * INA226_VSHUNT_SCALE;
	const float v_bus_plus = v_bus_minus + v_shunt;
	const float i_bus = static_cast<float>(_current) * _current_lsb;
	const float p_bus = static_cast<float>(_power) * _power_lsb;
	const hrt_abstime now = hrt_absolute_time();

	memset(&_pm_status, 0, sizeof(_pm_status));
	_pm_status.timestamp = now;
	_pm_status.voltage_v = success ? v_bus_plus : 0.f;
	_pm_status.current_a = success ? i_bus : -1.f;
	_pm_status.power_w = success ? p_bus : -1.f;
	_pm_status.rconf = static_cast<int16_t>(_config);
	_pm_status.rsv = _shunt;
	_pm_status.rbv = _bus_voltage;
	_pm_status.rp = _power;
	_pm_status.rc = _current;
	_pm_status.rcal = _cal;
	_pm_status.me = mask_enable;
	_pm_status.al = alert_limit;
	_pm_pub_topic.publish(_pm_status);

	perf_end(_sample_perf);

	if (success) {
		return PX4_OK;

	} else {
		return PX4_ERROR;
	}
}


void
SVEA_INA226::start()
{
	ScheduleClear();

	/* reset the report ring and state machine */
	_collect_phase = false;

	_measure_interval = INA226_CONVERSION_INTERVAL;

	/* schedule a cycle to start things */
	ScheduleDelayed(5);
}

void
SVEA_INA226::RunImpl()
{
	if (_initialized) {
		if (_collect_phase) {
			/* perform collection */
			if (collect() != PX4_OK) {
				perf_count(_collection_errors);
				/* if error restart the measurement state machine */
				start();
				return;
			}

			/* next phase is measurement */
			_collect_phase = !_mode_triggered;

			if (_measure_interval > INA226_CONVERSION_INTERVAL) {
				/* schedule a fresh cycle call when we are ready to measure again */
				ScheduleDelayed(_measure_interval - INA226_CONVERSION_INTERVAL);
				return;
			}
		}

		/* Measurement  phase */

		/* Perform measurement */
		if (measure() != PX4_OK) {
			perf_count(_measure_errors);
		}

		/* next phase is collection */
		_collect_phase = true;

		/* schedule a fresh cycle call when the measurement is done */
		ScheduleDelayed(INA226_CONVERSION_INTERVAL);

	} else {
		if (init() != PX4_OK) {
			ScheduleDelayed(INA226_INIT_RETRY_INTERVAL_US);
		}
	}
}

void
SVEA_INA226::print_status()
{
	I2CSPIDriverBase::print_status();

	if (_initialized) {
		perf_print_counter(_sample_perf);
		perf_print_counter(_comms_errors);

		printf("poll interval:  %u \n", _measure_interval);
		printf("max current: %.3f A, shunt: %.6f Ohm\n", (double)_max_current, (double)_rshunt);

		int16_t config{0};
		int16_t calibration{0};
		int16_t mask_enable{0};
		int16_t bus_raw{0};
		int16_t shunt_raw{0};
		int16_t current_raw{0};
		int16_t power_raw{0};

		const bool reg_ok =
			(read(INA226_REG_CONFIGURATION, config) == PX4_OK) &&
			(read(INA226_REG_CALIBRATION, calibration) == PX4_OK) &&
			(read(INA226_REG_MASKENABLE, mask_enable) == PX4_OK) &&
			(read(INA226_REG_BUSVOLTAGE, bus_raw) == PX4_OK) &&
			(read(INA226_REG_SHUNTVOLTAGE, shunt_raw) == PX4_OK) &&
			(read(INA226_REG_CURRENT, current_raw) == PX4_OK) &&
			(read(INA226_REG_POWER, power_raw) == PX4_OK);

		if (reg_ok) {
			const float v_bus_minus = static_cast<float>(bus_raw) * INA226_VSCALE;
			const float v_shunt = static_cast<float>(shunt_raw) * INA226_VSHUNT_SCALE;
			const float v_bus_plus = v_bus_minus + v_shunt;
			const float i_bus = static_cast<float>(current_raw) * _current_lsb;
			const float p_bus = static_cast<float>(power_raw) * _power_lsb;

			PX4_INFO("regs: CFG=0x%04x CAL=0x%04x MASK=0x%04x BUS=0x%04x SHUNT=0x%04x CUR=0x%04x PWR=0x%04x",
				 (unsigned)(uint16_t)config, (unsigned)(uint16_t)calibration, (unsigned)(uint16_t)mask_enable,
				 (unsigned)(uint16_t)bus_raw, (unsigned)(uint16_t)shunt_raw, (unsigned)(uint16_t)current_raw,
				 (unsigned)(uint16_t)power_raw);
			PX4_INFO("meas: Vbus+=%.6f Vbus-=%.6f Vshunt=%.6f I=%.6fA P=%.6fW",
				 (double)v_bus_plus, (double)v_bus_minus, (double)v_shunt, (double)i_bus, (double)p_bus);
			PX4_INFO("flags: CVRF=%u OVF=%u AFF=%u LEN=%u APOL=%u",
				 (mask_enable & INA226_CVRF) ? 1u : 0u, (mask_enable & INA226_OVF) ? 1u : 0u,
				 (mask_enable & INA226_AFF) ? 1u : 0u, (mask_enable & INA226_LEN) ? 1u : 0u,
				 (mask_enable & INA226_APOL) ? 1u : 0u);

		} else {
			PX4_WARN("failed to read one or more SVEA_INA226 status registers");
		}

	} else {
		PX4_INFO("Device not initialized. Retrying every %d ms until battery is plugged in.",
			 INA226_INIT_RETRY_INTERVAL_US / 1000);
	}
}
