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

#include "ISM330DLC.hpp"

#include <cstring>

using namespace time_literals;

ISM330DLC::ISM330DLC(const I2CSPIDriverConfig &config) :
	I2C(config),
	I2CSPIDriver(config),
	_px4_accel(get_device_id(), config.rotation),
	_px4_gyro(get_device_id(), config.rotation)
{
	_ctx.handle = this;
	_ctx.read_reg = read_reg;
	_ctx.write_reg = write_reg;

	_retries = 3;
}

ISM330DLC::~ISM330DLC()
{
	perf_free(_bad_register_perf);
	perf_free(_bad_transfer_perf);
	perf_free(_reset_perf);
}

int ISM330DLC::init()
{
	const int ret = I2C::init();

	if (ret != PX4_OK) {
		DEVICE_DEBUG("I2C::init failed (%d)", ret);
		return ret;
	}

	ScheduleNow();
	return PX4_OK;
}

void ISM330DLC::exit_and_cleanup()
{
	I2CSPIDriverBase::exit_and_cleanup();
}

void ISM330DLC::print_status()
{
	I2CSPIDriverBase::print_status();
	perf_print_counter(_bad_register_perf);
	perf_print_counter(_bad_transfer_perf);
	perf_print_counter(_reset_perf);
}

int ISM330DLC::probe()
{
	uint8_t who_am_i = 0;

	if (ism330dlc_device_id_get(&_ctx, &who_am_i) != 0) {
		return PX4_ERROR;
	}

	if (who_am_i != ISM330DLC_ID) {
		DEVICE_DEBUG("unexpected WHO_AM_I: 0x%02x", who_am_i);
		return PX4_ERROR;
	}

	return PX4_OK;
}

bool ISM330DLC::Reset()
{
	uint8_t reset = 1;

	if (ism330dlc_reset_set(&_ctx, reset) != 0) {
		return false;
	}

	for (int i = 0; i < 20; i++) {
		px4_usleep(1000);

		if (ism330dlc_reset_get(&_ctx, &reset) != 0) {
			return false;
		}

		if (reset == 0) {
			perf_count(_reset_perf);
			return true;
		}
	}

	return false;
}

bool ISM330DLC::Configure()
{
	if (!Reset()) {
		return false;
	}

	if (ism330dlc_auto_increment_set(&_ctx, PROPERTY_ENABLE) != 0) {
		return false;
	}

	if (ism330dlc_block_data_update_set(&_ctx, PROPERTY_ENABLE) != 0) {
		return false;
	}

	if (ism330dlc_xl_full_scale_set(&_ctx, ISM330DLC_16g) != 0) {
		return false;
	}

	if (ism330dlc_gy_full_scale_set(&_ctx, ISM330DLC_2000dps) != 0) {
		return false;
	}

	if (ism330dlc_xl_data_rate_set(&_ctx, ISM330DLC_XL_ODR_416Hz) != 0) {
		return false;
	}

	if (ism330dlc_gy_data_rate_set(&_ctx, ISM330DLC_GY_ODR_416Hz) != 0) {
		return false;
	}

	_px4_accel.set_device_type(DRV_IMU_DEVTYPE_ISM330DLC);
	_px4_gyro.set_device_type(DRV_IMU_DEVTYPE_ISM330DLC);

	_px4_accel.set_range(16.f * CONSTANTS_ONE_G);
	_px4_gyro.set_range(math::radians(2000.f));

	return true;
}

bool ISM330DLC::ReadAndPublish()
{
	int16_t accel_raw[3] {};
	int16_t gyro_raw[3] {};

	if (ism330dlc_acceleration_raw_get(&_ctx, accel_raw) != 0) {
		return false;
	}

	if (ism330dlc_angular_rate_raw_get(&_ctx, gyro_raw) != 0) {
		return false;
	}

	const hrt_abstime now = hrt_absolute_time();

	const float accel_x = (ism330dlc_from_fs16g_to_mg(accel_raw[0]) / 1000.f) * CONSTANTS_ONE_G;
	const float accel_y = (ism330dlc_from_fs16g_to_mg(accel_raw[1]) / 1000.f) * CONSTANTS_ONE_G;
	const float accel_z = (ism330dlc_from_fs16g_to_mg(accel_raw[2]) / 1000.f) * CONSTANTS_ONE_G;

	const float gyro_x = math::radians(ism330dlc_from_fs2000dps_to_mdps(gyro_raw[0]) / 1000.f);
	const float gyro_y = math::radians(ism330dlc_from_fs2000dps_to_mdps(gyro_raw[1]) / 1000.f);
	const float gyro_z = math::radians(ism330dlc_from_fs2000dps_to_mdps(gyro_raw[2]) / 1000.f);

	_px4_accel.update(now, accel_x, accel_y, accel_z);
	_px4_gyro.update(now, gyro_x, gyro_y, gyro_z);

	if (hrt_elapsed_time(&_last_temperature_update) > 1_s) {
		UpdateTemperature();
		_last_temperature_update = now;
	}

	return true;
}

bool ISM330DLC::UpdateTemperature()
{
	int16_t raw = 0;

	if (ism330dlc_temperature_raw_get(&_ctx, &raw) != 0) {
		return false;
	}

	const float temperature_c = ism330dlc_from_lsb_to_celsius(raw);
	_px4_accel.set_temperature(temperature_c);
	_px4_gyro.set_temperature(temperature_c);

	return true;
}

void ISM330DLC::RunImpl()
{
	if (should_exit()) {
		exit_and_cleanup();
		return;
	}

	if (!_configured) {
		if (!Configure()) {
			perf_count(_bad_register_perf);
			ScheduleDelayed(100_ms);
			return;
		}

		_configured = true;
		_failure_count = 0;
		ScheduleOnInterval(2500_us);
		return;
	}

	if (!ReadAndPublish()) {
		perf_count(_bad_transfer_perf);
		_failure_count++;

		if (_failure_count > 10) {
			_configured = false;
			_failure_count = 0;
			ScheduleDelayed(20_ms);
			return;
		}

	} else {
		_failure_count = 0;
	}
}

int32_t ISM330DLC::write_reg(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len)
{
	auto *self = static_cast<ISM330DLC *>(handle);

	if ((self == nullptr) || (buf == nullptr) || (len == 0)) {
		return -1;
	}

	uint8_t tx[17] {};

	if (len > (sizeof(tx) - 1)) {
		return -1;
	}

	tx[0] = reg;
	memcpy(&tx[1], buf, len);

	return (self->transfer(tx, len + 1, nullptr, 0) == PX4_OK) ? 0 : -1;
}

int32_t ISM330DLC::read_reg(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
	auto *self = static_cast<ISM330DLC *>(handle);

	if ((self == nullptr) || (buf == nullptr) || (len == 0)) {
		return -1;
	}

	return (self->transfer(&reg, 1, buf, len) == PX4_OK) ? 0 : -1;
}
