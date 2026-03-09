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

#include <lib/drivers/accelerometer/PX4Accelerometer.hpp>
#include <lib/drivers/gyroscope/PX4Gyroscope.hpp>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/i2c_spi_buses.h>

// Register map shared by ISM330DLC/LSM6 family.
static constexpr uint8_t ISM330DLC_ADDR_WHO_AM_I = 0x0F;
static constexpr uint8_t ISM330DLC_ADDR_CTRL1_XL = 0x10;
static constexpr uint8_t ISM330DLC_ADDR_CTRL2_G = 0x11;
static constexpr uint8_t ISM330DLC_ADDR_CTRL3_C = 0x12;
static constexpr uint8_t ISM330DLC_ADDR_STATUS = 0x1E;
static constexpr uint8_t ISM330DLC_ADDR_OUT_TEMP_L = 0x20;

static constexpr uint8_t ISM330DLC_WHO_AM_I = 0x6A;

// CTRL3_C bits
static constexpr uint8_t CTRL3_C_BDU = (1 << 6);
static constexpr uint8_t CTRL3_C_IF_INC = (1 << 2);

// STATUS_REG bits
static constexpr uint8_t STATUS_XLDA = (1 << 0);
static constexpr uint8_t STATUS_GDA = (1 << 1);

// CTRL1_XL config fields
static constexpr uint8_t CTRL1_XL_ODR_104HZ = (0x4 << 4);
static constexpr uint8_t CTRL1_XL_ODR_208HZ = (0x5 << 4);
static constexpr uint8_t CTRL1_XL_ODR_416HZ = (0x6 << 4);
static constexpr uint8_t CTRL1_XL_FS_4G = (0x2 << 2);
static constexpr uint8_t CTRL1_XL_FS_8G = (0x3 << 2);
static constexpr uint8_t CTRL1_XL_FS_16G = (0x1 << 2);

// CTRL2_G config fields
static constexpr uint8_t CTRL2_G_ODR_104HZ = (0x4 << 4);
static constexpr uint8_t CTRL2_G_ODR_208HZ = (0x5 << 4);
static constexpr uint8_t CTRL2_G_ODR_416HZ = (0x6 << 4);
static constexpr uint8_t CTRL2_G_FS_500DPS = (0x1 << 2);
static constexpr uint8_t CTRL2_G_FS_1000DPS = (0x2 << 2);
static constexpr uint8_t CTRL2_G_FS_2000DPS = (0x3 << 2);

// Default PX4 config for this board. Change these in one place if needed.
static constexpr uint8_t ISM330DLC_CTRL1_XL_DEFAULT = CTRL1_XL_ODR_416HZ | CTRL1_XL_FS_16G;
static constexpr uint8_t ISM330DLC_CTRL2_G_DEFAULT = CTRL2_G_ODR_416HZ | CTRL2_G_FS_2000DPS;

extern device::Device *ISM330DLC_I2C_interface(const I2CSPIDriverConfig &config);

class ISM330DLC : public I2CSPIDriver<ISM330DLC>
{
public:
	ISM330DLC(device::Device *interface, const I2CSPIDriverConfig &config);
	~ISM330DLC() override;

	static I2CSPIDriverBase *instantiate(const I2CSPIDriverConfig &config, int runtime_instance);
	static void print_usage();

	int init();
	void print_status() override;
	void RunImpl();

private:
	struct MeasurementData {
		uint8_t temp_l;
		uint8_t temp_h;
		uint8_t gx_l;
		uint8_t gx_h;
		uint8_t gy_l;
		uint8_t gy_h;
		uint8_t gz_l;
		uint8_t gz_h;
		uint8_t ax_l;
		uint8_t ax_h;
		uint8_t ay_l;
		uint8_t ay_h;
		uint8_t az_l;
		uint8_t az_h;
	};

	uint8_t read_register(uint8_t reg);
	int read_measurement(MeasurementData *data);
	void write_register(uint8_t reg, uint8_t value);

	device::Device *_interface;
	PX4Accelerometer _px4_accel;
	PX4Gyroscope _px4_gyro;

	perf_counter_t _sample_perf;
	perf_counter_t _comms_error_perf;
};
