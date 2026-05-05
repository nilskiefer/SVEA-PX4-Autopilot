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

/**
 * @file svea_ina3221.cpp
 *
 * Driver for the I2C attached INA3221 (SVEA wrapper)
 */

#include "svea_ina3221.h"

#include <cstring>

SVEA_INA3221::SVEA_INA3221(const I2CSPIDriverConfig &config) :
	I2C(config),
	I2CSPIDriver(config),
	_sample_perf(perf_alloc(PC_ELAPSED, "ina3221_read")),
	_comms_errors(perf_alloc(PC_COUNT, "ina3221_com_err")),
	_collection_errors(perf_alloc(PC_COUNT, "ina3221_collection_err"))
{
	if (config.custom1 > 0) {
		for (unsigned i = 0; i < INA3221_CHANNEL_COUNT; i++) {
			const float shunt_ohm = shunt_from_packed_milliohm(config.custom1, i);

			if (shunt_ohm > 0.f) {
				_rshunt_ohm[i] = shunt_ohm;
			}
		}
	}

	I2C::_retries = 5;
}

SVEA_INA3221::~SVEA_INA3221()
{
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_collection_errors);
}

float SVEA_INA3221::shunt_from_packed_milliohm(int packed, unsigned channel)
{
	if (channel >= INA3221_CHANNEL_COUNT) {
		return -1.f;
	}

	const int shift = static_cast<int>(channel) * 10;
	const int shunt_milliohm = (packed >> shift) & 0x3ff;

	if (shunt_milliohm <= 0) {
		return -1.f;
	}

	return static_cast<float>(shunt_milliohm) * 1e-3f;
}

int16_t SVEA_INA3221::clamp_to_i16(int32_t v)
{
	if (v > 32767) {
		return 32767;
	}

	if (v < -32768) {
		return -32768;
	}

	return static_cast<int16_t>(v);
}

int SVEA_INA3221::read_reg(uint8_t reg, uint16_t &value)
{
	uint16_t rx = 0;
	const int ret = transfer(&reg, 1, reinterpret_cast<uint8_t *>(&rx), sizeof(rx));

	if (ret == PX4_OK) {
		value = swap16(rx);

	} else {
		perf_count(_comms_errors);
	}

	return ret;
}

int SVEA_INA3221::write_reg(uint8_t reg, uint16_t value)
{
	uint8_t data[3] = {reg, static_cast<uint8_t>((value & 0xff00) >> 8), static_cast<uint8_t>(value & 0xff)};
	return transfer(data, sizeof(data), nullptr, 0);
}

int SVEA_INA3221::probe()
{
	uint16_t mfg_id = 0;
	uint16_t die_id = 0;

	if (read_reg(INA3221_REG_MANUFACTURER_ID, mfg_id) != PX4_OK || mfg_id != INA3221_MFG_ID_TI) {
		PX4_ERR("probe failed: mfg_id=0x%04x expected=0x%04x", (unsigned)mfg_id, (unsigned)INA3221_MFG_ID_TI);
		return PX4_ERROR;
	}

	if (read_reg(INA3221_REG_DIE_ID, die_id) != PX4_OK || die_id != INA3221_DIE_ID) {
		PX4_ERR("probe failed: die_id=0x%04x expected=0x%04x", (unsigned)die_id, (unsigned)INA3221_DIE_ID);
		return PX4_ERROR;
	}

	return PX4_OK;
}

int SVEA_INA3221::init()
{
	for (unsigned i = 0; i < INA3221_CHANNEL_COUNT; i++) {
		if (_rshunt_ohm[i] <= 0.f) {
			PX4_ERR("invalid shunt setup ch%u=%.6f Ohm", i + 1, (double)_rshunt_ohm[i]);
			return PX4_ERROR;
		}
	}

	if (I2C::init() != PX4_OK) {
		PX4_ERR("I2C init/probe failed (bus=%u addr=0x%02x)", get_device_bus(), get_device_address());
		return PX4_ERROR;
	}

	if (write_reg(INA3221_REG_CONFIG, INA3221_CONFIG_RST) != PX4_OK) {
		PX4_ERR("failed to reset (bus=%u addr=0x%02x)", get_device_bus(), get_device_address());
		return PX4_ERROR;
	}

	if (write_reg(INA3221_REG_CONFIG, _config) != PX4_OK) {
		PX4_ERR("failed to write config=0x%04x (bus=%u addr=0x%02x)", (unsigned)_config,
			get_device_bus(), get_device_address());
		return PX4_ERROR;
	}

	_initialized = true;
	start();
	return PX4_OK;
}

int SVEA_INA3221::force_init()
{
	const int ret = init();
	start();
	return ret;
}

void SVEA_INA3221::publish_channel(unsigned channel, int16_t bus_raw, int16_t shunt_raw, int16_t mask_enable, bool valid)
{
	const int16_t bus_code = static_cast<int16_t>(bus_raw) >> 3;
	const int16_t shunt_code = static_cast<int16_t>(shunt_raw) >> 3;

	const float v_bus = static_cast<float>(bus_code) * INA3221_BUS_VOLTAGE_LSB_V;
	const float v_shunt = static_cast<float>(shunt_code) * INA3221_SHUNT_VOLTAGE_LSB_V;
	// INA3221 bus voltage register measures VIN- with 8 mV LSB (datasheet 7.6.2.3).
	const float voltage_v = v_bus;
	const float current_a = (_rshunt_ohm[channel] > 0.f) ? (v_shunt / _rshunt_ohm[channel]) : 0.f;
	const float power_w = voltage_v * current_a;
	const hrt_abstime now = hrt_absolute_time();

	_channel_snapshot[channel].valid = valid;
	_channel_snapshot[channel].timestamp = now;
	_channel_snapshot[channel].bus_raw = bus_raw;
	_channel_snapshot[channel].shunt_raw = shunt_raw;
	_channel_snapshot[channel].voltage_v = valid ? voltage_v : 0.f;
	_channel_snapshot[channel].current_a = valid ? current_a : -1.f;
	_channel_snapshot[channel].power_w = valid ? power_w : -1.f;

	power_monitor_s &pm = _pm_status[channel];
	memset(&pm, 0, sizeof(pm));
	pm.timestamp = now;
	pm.voltage_v = valid ? voltage_v : 0.f;
	pm.current_a = valid ? current_a : -1.f;
	pm.power_w = valid ? power_w : -1.f;
	pm.rconf = static_cast<int16_t>(_config);
	pm.rsv = shunt_raw;
	pm.rbv = bus_raw;
	pm.rc = valid ? clamp_to_i16(static_cast<int32_t>(current_a * 1000.f)) : 0;
	pm.rp = valid ? clamp_to_i16(static_cast<int32_t>(power_w * 1000.f)) : 0;
	pm.rcal = clamp_to_i16(static_cast<int32_t>(_rshunt_ohm[channel] * 1e6f));
	pm.me = mask_enable;
	pm.al = static_cast<int16_t>(channel + 1);

	_pm_pub_topic[channel].publish(pm);
}

int SVEA_INA3221::collect()
{
	perf_begin(_sample_perf);

	uint16_t mask_enable_u16 = 0;
	const bool mask_ok = (read_reg(INA3221_REG_MASKENABLE, mask_enable_u16) == PX4_OK);
	const int16_t mask_enable = mask_ok ? static_cast<int16_t>(mask_enable_u16) : 0;

	bool any_valid = false;

	for (unsigned ch = 0; ch < INA3221_CHANNEL_COUNT; ch++) {
		uint16_t bus_raw_u16 = 0;
		uint16_t shunt_raw_u16 = 0;

		const uint8_t bus_reg = INA3221_REG_CH1_BUS_VOLTAGE + static_cast<uint8_t>(2 * ch);
		const uint8_t shunt_reg = INA3221_REG_CH1_SHUNT_VOLTAGE + static_cast<uint8_t>(2 * ch);

		const bool bus_ok = (read_reg(bus_reg, bus_raw_u16) == PX4_OK);
		const bool shunt_ok = (read_reg(shunt_reg, shunt_raw_u16) == PX4_OK);
		const bool valid = bus_ok && shunt_ok;

		publish_channel(ch,
				static_cast<int16_t>(bus_raw_u16),
				static_cast<int16_t>(shunt_raw_u16),
				mask_enable,
				valid);

		any_valid = any_valid || valid;
	}

	perf_end(_sample_perf);
	return any_valid ? PX4_OK : PX4_ERROR;
}

void SVEA_INA3221::start()
{
	ScheduleClear();
	ScheduleDelayed(1000);
}

void SVEA_INA3221::RunImpl()
{
	if (_initialized) {
		if (collect() != PX4_OK) {
			perf_count(_collection_errors);
		}

		ScheduleDelayed(INA3221_SAMPLE_INTERVAL_US);

	} else {
		if (init() != PX4_OK) {
			ScheduleDelayed(INA3221_INIT_RETRY_INTERVAL_US);
		}
	}
}

void SVEA_INA3221::print_status()
{
	I2CSPIDriverBase::print_status();

	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_collection_errors);

	PX4_INFO("config=0x%04x", (unsigned)_config);
	PX4_INFO("shunts: ch1=%.6f ch2=%.6f ch3=%.6f Ohm",
		 (double)_rshunt_ohm[0], (double)_rshunt_ohm[1], (double)_rshunt_ohm[2]);

	uint16_t mask_enable = 0;

	if (read_reg(INA3221_REG_MASKENABLE, mask_enable) == PX4_OK) {
		PX4_INFO("mask_enable=0x%04x", (unsigned)mask_enable);
	} else {
		PX4_WARN("failed to read mask_enable");
	}

	for (unsigned ch = 0; ch < INA3221_CHANNEL_COUNT; ch++) {
		const ChannelSnapshot &snap = _channel_snapshot[ch];
		PX4_INFO("ch%u valid=%d raw_bus=%d raw_shunt=%d V=%.5f I=%.5f P=%.5f age=%.3fs",
			 ch + 1,
			 snap.valid ? 1 : 0,
			 static_cast<int>(snap.bus_raw),
			 static_cast<int>(snap.shunt_raw),
			 (double)snap.voltage_v,
			 (double)snap.current_a,
			 (double)snap.power_w,
			 snap.timestamp > 0 ? (double)((hrt_absolute_time() - snap.timestamp) * 1e-6) : -1.0);
	}
}
