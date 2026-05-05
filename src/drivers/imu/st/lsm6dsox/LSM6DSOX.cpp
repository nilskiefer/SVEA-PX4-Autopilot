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

#include "LSM6DSOX.hpp"

#include <inttypes.h>
#include <cstring>

using namespace time_literals;

static inline int16_t combine(uint8_t msb, uint8_t lsb)
{
	return static_cast<int16_t>(static_cast<uint16_t>(msb) << 8U | static_cast<uint16_t>(lsb));
}

LSM6DSOX::LSM6DSOX(const I2CSPIDriverConfig &config) :
	I2C(config),
	I2CSPIDriver(config),
	ModuleParams(nullptr),
	_px4_accel(get_device_id(), config.rotation),
	_px4_gyro(get_device_id(), config.rotation)
{
	_ctx.handle = this;
	_ctx.read_reg = read_reg;
	_ctx.write_reg = write_reg;

	_retries = 5;
}

LSM6DSOX::~LSM6DSOX()
{
	perf_free(_bad_register_perf);
	perf_free(_bad_transfer_perf);
	perf_free(_reset_perf);
}

int LSM6DSOX::init()
{
	const int ret = I2C::init();

	if (ret != PX4_OK) {
		DEVICE_DEBUG("I2C::init failed (%d)", ret);
		return ret;
	}

	ScheduleNow();
	return PX4_OK;
}

void LSM6DSOX::exit_and_cleanup()
{
	I2CSPIDriverBase::exit_and_cleanup();
}

void LSM6DSOX::print_status()
{
	I2CSPIDriverBase::print_status();
	perf_print_counter(_bad_register_perf);
	perf_print_counter(_bad_transfer_perf);
	perf_print_counter(_reset_perf);
}

int LSM6DSOX::probe()
{
	uint8_t who_am_i = 0;

	if (lsm6dsox_device_id_get(&_ctx, &who_am_i) != 0) {
		return PX4_ERROR;
	}

	if (who_am_i != LSM6DSOX_ID) {
		DEVICE_DEBUG("unexpected WHO_AM_I: 0x%02x", who_am_i);
		return PX4_ERROR;
	}

	return PX4_OK;
}

bool LSM6DSOX::Reset()
{
	if (lsm6dsox_reset_set(&_ctx, 1) != 0) {
		return false;
	}

	for (int i = 0; i < 20; i++) {
		px4_usleep(1000);

		uint8_t reset = 1;

		if (lsm6dsox_reset_get(&_ctx, &reset) != 0) {
			return false;
		}

		if (reset == 0) {
			perf_count(_reset_perf);
			return true;
		}
	}

	return false;
}

bool LSM6DSOX::Configure()
{
	if (!Reset()) {
		return false;
	}

	if (lsm6dsox_auto_increment_set(&_ctx, PROPERTY_ENABLE) != 0) {
		return false;
	}

	if (lsm6dsox_block_data_update_set(&_ctx, PROPERTY_ENABLE) != 0) {
		return false;
	}

	// Keep the part in pure I2C mode on shared buses.
	if (lsm6dsox_i3c_disable_set(&_ctx, LSM6DSOX_I3C_DISABLE) != 0) {
		return false;
	}

	if (!ConfigureFromParameters()) {
		return false;
	}

	if (lsm6dsox_xl_full_scale_set(&_ctx, _accel_fs) != 0) {
		return false;
	}

	if (lsm6dsox_gy_full_scale_set(&_ctx, _gyro_fs) != 0) {
		return false;
	}

	if (lsm6dsox_xl_data_rate_set(&_ctx, _accel_odr) != 0) {
		return false;
	}

	if (lsm6dsox_gy_data_rate_set(&_ctx, _gyro_odr) != 0) {
		return false;
	}

	_px4_accel.set_device_type(DRV_IMU_DEVTYPE_LSM6DSOX);
	_px4_gyro.set_device_type(DRV_IMU_DEVTYPE_LSM6DSOX);

	_px4_accel.set_range(_accel_range_g * CONSTANTS_ONE_G);
	_px4_gyro.set_range(math::radians(_gyro_range_dps));

	return true;
}

bool LSM6DSOX::ConfigureFromParameters()
{
	updateParams();

	const int32_t accel_fs = _param_lsm6dsox_acc_fs.get();

	switch (accel_fs) {
	case 2:
		_accel_fs = LSM6DSOX_2g;
		_accel_range_g = 2.f;
		break;

	case 4:
		_accel_fs = LSM6DSOX_4g;
		_accel_range_g = 4.f;
		break;

	case 8:
		_accel_fs = LSM6DSOX_8g;
		_accel_range_g = 8.f;
		break;

	case 16:
		_accel_fs = LSM6DSOX_16g;
		_accel_range_g = 16.f;
		break;

	default:
		PX4_ERR("invalid LSM6DSOX_ACC_FS: %" PRId32, accel_fs);
		return false;
	}

	const int32_t gyro_fs = _param_lsm6dsox_gyr_fs.get();

	switch (gyro_fs) {
	case 125:
		_gyro_fs = LSM6DSOX_125dps;
		_gyro_range_dps = 125.f;
		break;

	case 250:
		_gyro_fs = LSM6DSOX_250dps;
		_gyro_range_dps = 250.f;
		break;

	case 500:
		_gyro_fs = LSM6DSOX_500dps;
		_gyro_range_dps = 500.f;
		break;

	case 1000:
		_gyro_fs = LSM6DSOX_1000dps;
		_gyro_range_dps = 1000.f;
		break;

	case 2000:
		_gyro_fs = LSM6DSOX_2000dps;
		_gyro_range_dps = 2000.f;
		break;

	default:
		PX4_ERR("invalid LSM6DSOX_GYR_FS: %" PRId32, gyro_fs);
		return false;
	}

	int32_t accel_odr_hz = 0;
	const int32_t accel_odr = _param_lsm6dsox_acc_odr.get();

	switch (accel_odr) {
	case 52:
		_accel_odr = LSM6DSOX_XL_ODR_52Hz;
		accel_odr_hz = 52;
		break;

	case 104:
		_accel_odr = LSM6DSOX_XL_ODR_104Hz;
		accel_odr_hz = 104;
		break;

	case 208:
		_accel_odr = LSM6DSOX_XL_ODR_208Hz;
		accel_odr_hz = 208;
		break;

	case 417:
		_accel_odr = LSM6DSOX_XL_ODR_417Hz;
		accel_odr_hz = 417;
		break;

	case 833:
		_accel_odr = LSM6DSOX_XL_ODR_833Hz;
		accel_odr_hz = 833;
		break;

	default:
		PX4_ERR("invalid LSM6DSOX_ACC_ODR: %" PRId32, accel_odr);
		return false;
	}

	int32_t gyro_odr_hz = 0;
	const int32_t gyro_odr = _param_lsm6dsox_gyr_odr.get();

	switch (gyro_odr) {
	case 52:
		_gyro_odr = LSM6DSOX_GY_ODR_52Hz;
		gyro_odr_hz = 52;
		break;

	case 104:
		_gyro_odr = LSM6DSOX_GY_ODR_104Hz;
		gyro_odr_hz = 104;
		break;

	case 208:
		_gyro_odr = LSM6DSOX_GY_ODR_208Hz;
		gyro_odr_hz = 208;
		break;

	case 417:
		_gyro_odr = LSM6DSOX_GY_ODR_417Hz;
		gyro_odr_hz = 417;
		break;

	case 833:
		_gyro_odr = LSM6DSOX_GY_ODR_833Hz;
		gyro_odr_hz = 833;
		break;

	default:
		PX4_ERR("invalid LSM6DSOX_GYR_ODR: %" PRId32, gyro_odr);
		return false;
	}

	const int32_t max_odr_hz = math::max(accel_odr_hz, gyro_odr_hz);
	_sample_interval_us = 1000000U / static_cast<uint32_t>(max_odr_hz);

	return true;
}

bool LSM6DSOX::ReadAndPublish()
{
	uint8_t accel_raw[6] {};
	uint8_t gyro_raw[6] {};

	if (lsm6dsox_acceleration_raw_get(&_ctx, accel_raw) != 0) {
		return false;
	}

	if (lsm6dsox_angular_rate_raw_get(&_ctx, gyro_raw) != 0) {
		return false;
	}

	const int16_t accel_x_raw = combine(accel_raw[1], accel_raw[0]);
	const int16_t accel_y_raw = combine(accel_raw[3], accel_raw[2]);
	const int16_t accel_z_raw = combine(accel_raw[5], accel_raw[4]);

	const int16_t gyro_x_raw = combine(gyro_raw[1], gyro_raw[0]);
	const int16_t gyro_y_raw = combine(gyro_raw[3], gyro_raw[2]);
	const int16_t gyro_z_raw = combine(gyro_raw[5], gyro_raw[4]);

	const hrt_abstime now = hrt_absolute_time();

	auto accel_to_si = [this](int16_t raw) -> float {
		switch (_accel_fs)
		{
		case LSM6DSOX_2g:
			return (lsm6dsox_from_fs2_to_mg(raw) / 1000.f) * CONSTANTS_ONE_G;

		case LSM6DSOX_4g:
			return (lsm6dsox_from_fs4_to_mg(raw) / 1000.f) * CONSTANTS_ONE_G;

		case LSM6DSOX_8g:
			return (lsm6dsox_from_fs8_to_mg(raw) / 1000.f) * CONSTANTS_ONE_G;

		case LSM6DSOX_16g:
			return (lsm6dsox_from_fs16_to_mg(raw) / 1000.f) * CONSTANTS_ONE_G;
		}

		return NAN;
	};

	auto gyro_to_si = [this](int16_t raw) -> float {
		switch (_gyro_fs)
		{
		case LSM6DSOX_125dps:
			return math::radians(lsm6dsox_from_fs125_to_mdps(raw) / 1000.f);

		case LSM6DSOX_250dps:
			return math::radians(lsm6dsox_from_fs250_to_mdps(raw) / 1000.f);

		case LSM6DSOX_500dps:
			return math::radians(lsm6dsox_from_fs500_to_mdps(raw) / 1000.f);

		case LSM6DSOX_1000dps:
			return math::radians(lsm6dsox_from_fs1000_to_mdps(raw) / 1000.f);

		case LSM6DSOX_2000dps:
			return math::radians(lsm6dsox_from_fs2000_to_mdps(raw) / 1000.f);
		}

		return NAN;
	};

	const float accel_x = accel_to_si(accel_x_raw);
	const float accel_y = accel_to_si(accel_y_raw);
	const float accel_z = accel_to_si(accel_z_raw);

	const float gyro_x = gyro_to_si(gyro_x_raw);
	const float gyro_y = gyro_to_si(gyro_y_raw);
	const float gyro_z = gyro_to_si(gyro_z_raw);

	if (!PX4_ISFINITE(accel_x) || !PX4_ISFINITE(accel_y) || !PX4_ISFINITE(accel_z)
	    || !PX4_ISFINITE(gyro_x) || !PX4_ISFINITE(gyro_y) || !PX4_ISFINITE(gyro_z)) {
		return false;
	}

	_px4_accel.update(now, accel_x, accel_y, accel_z);
	_px4_gyro.update(now, gyro_x, gyro_y, gyro_z);

	if (hrt_elapsed_time(&_last_temperature_update) > 1_s) {
		UpdateTemperature();
		_last_temperature_update = now;
	}

	return true;
}

bool LSM6DSOX::UpdateTemperature()
{
	uint8_t raw[2] {};

	if (lsm6dsox_temperature_raw_get(&_ctx, raw) != 0) {
		return false;
	}

	const float temperature_c = lsm6dsox_from_lsb_to_celsius(combine(raw[1], raw[0]));
	_px4_accel.set_temperature(temperature_c);
	_px4_gyro.set_temperature(temperature_c);

	return true;
}

void LSM6DSOX::RunImpl()
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
		ScheduleOnInterval(_sample_interval_us);
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

int32_t LSM6DSOX::write_reg(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
	auto *self = static_cast<LSM6DSOX *>(handle);

	if ((self == nullptr) || (buf == nullptr) || (len == 0)) {
		return -1;
	}

	uint8_t tx[33] {};

	if (len > (sizeof(tx) - 1U)) {
		return -1;
	}

	tx[0] = reg;
	memcpy(&tx[1], buf, len);

	return (self->transfer(tx, len + 1U, nullptr, 0) == PX4_OK) ? 0 : -1;
}

int32_t LSM6DSOX::read_reg(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
	auto *self = static_cast<LSM6DSOX *>(handle);

	if ((self == nullptr) || (buf == nullptr) || (len == 0)) {
		return -1;
	}

	return (self->transfer(&reg, 1, buf, len) == PX4_OK) ? 0 : -1;
}
