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

#pragma once

#include <drivers/drv_hrt.h>
#include <drivers/drv_sensor.h>
#include <lib/drivers/accelerometer/PX4Accelerometer.hpp>
#include <lib/drivers/device/i2c.h>
#include <lib/drivers/gyroscope/PX4Gyroscope.hpp>
#include <lib/mathlib/mathlib.h>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/i2c_spi_buses.h>
#include <px4_platform_common/module_params.h>

#include "vendor/lsm6dsox_reg.h"

class LSM6DSOX : public device::I2C, public I2CSPIDriver<LSM6DSOX>, public ModuleParams
{
public:
	static constexpr uint8_t I2C_ADDRESS_DEFAULT{0x6b};
	static constexpr int I2C_SPEED{400 * 1000};

	LSM6DSOX(const I2CSPIDriverConfig &config);
	~LSM6DSOX() override;

	static void print_usage();

	int init() override;
	void print_status() override;

	void RunImpl();

private:
	void exit_and_cleanup() override;

	int probe() override;

	bool Reset();
	bool Configure();
	bool ConfigureFromParameters();
	bool ReadAndPublish();
	bool UpdateTemperature();

	static int32_t write_reg(void *handle, uint8_t reg, uint8_t *buf, uint16_t len);
	static int32_t read_reg(void *handle, uint8_t reg, uint8_t *buf, uint16_t len);

	lsm6dsox_ctx_t _ctx {};

	lsm6dsox_fs_xl_t _accel_fs{LSM6DSOX_16g};
	lsm6dsox_fs_g_t _gyro_fs{LSM6DSOX_2000dps};
	lsm6dsox_odr_xl_t _accel_odr{LSM6DSOX_XL_ODR_417Hz};
	lsm6dsox_odr_g_t _gyro_odr{LSM6DSOX_GY_ODR_417Hz};
	float _accel_range_g{16.f};
	float _gyro_range_dps{2000.f};
	uint32_t _sample_interval_us{2500};

	PX4Accelerometer _px4_accel;
	PX4Gyroscope _px4_gyro;

	perf_counter_t _bad_register_perf{perf_alloc(PC_COUNT, MODULE_NAME": bad register")};
	perf_counter_t _bad_transfer_perf{perf_alloc(PC_COUNT, MODULE_NAME": bad transfer")};
	perf_counter_t _reset_perf{perf_alloc(PC_COUNT, MODULE_NAME": reset")};

	hrt_abstime _last_temperature_update{0};
	bool _configured{false};
	int _failure_count{0};

	DEFINE_PARAMETERS(
		(ParamInt<px4::params::LSM6DSOX_ACC_FS>) _param_lsm6dsox_acc_fs,
		(ParamInt<px4::params::LSM6DSOX_GYR_FS>) _param_lsm6dsox_gyr_fs,
		(ParamInt<px4::params::LSM6DSOX_ACC_ODR>) _param_lsm6dsox_acc_odr,
		(ParamInt<px4::params::LSM6DSOX_GYR_ODR>) _param_lsm6dsox_gyr_odr
	)
};
