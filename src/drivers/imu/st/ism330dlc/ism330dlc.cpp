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

#include "ism330dlc.hpp"

#include <drivers/drv_hrt.h>
#include <lib/geo/geo.h>

using namespace time_literals;

static inline int16_t combine(uint8_t lsb, uint8_t msb)
{
	return static_cast<int16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
}

ISM330DLC::ISM330DLC(device::Device *interface, const I2CSPIDriverConfig &config) :
	I2CSPIDriver(config),
	_interface(interface),
	_px4_accel(interface->get_device_id(), config.rotation),
	_px4_gyro(interface->get_device_id(), config.rotation),
	_sample_perf(perf_alloc(PC_COUNT, MODULE_NAME": sample")),
	_comms_error_perf(perf_alloc(PC_COUNT, MODULE_NAME": comms errors"))
{
}

ISM330DLC::~ISM330DLC()
{
	perf_free(_sample_perf);
	perf_free(_comms_error_perf);
	delete _interface;
}

int ISM330DLC::init()
{
	// Block update and auto register increment for burst reads.
	write_register(ISM330DLC_ADDR_CTRL3_C, CTRL3_C_BDU | CTRL3_C_IF_INC);
	// Default accel/gyro config is centralized in ism330dlc.hpp.
	// Example alternatives:
	//   write_register(ISM330DLC_ADDR_CTRL1_XL, CTRL1_XL_ODR_416HZ | CTRL1_XL_FS_8G);
	//   write_register(ISM330DLC_ADDR_CTRL2_G, CTRL2_G_ODR_416HZ | CTRL2_G_FS_1000DPS);
	write_register(ISM330DLC_ADDR_CTRL1_XL, ISM330DLC_CTRL1_XL_DEFAULT);
	write_register(ISM330DLC_ADDR_CTRL2_G, ISM330DLC_CTRL2_G_DEFAULT);

	// Device sensitivities at configured full-scale.
	_px4_accel.set_scale(0.488e-3f * CONSTANTS_ONE_G);   // 0.488 mg/LSB
	_px4_accel.set_range(16.f * CONSTANTS_ONE_G);
	_px4_gyro.set_scale(math::radians(70.f / 1000.f));   // 70 mdps/LSB
	_px4_gyro.set_range(math::radians(2000.f));

	ScheduleDelayed(10_ms);
	return PX4_OK;
}

void ISM330DLC::RunImpl()
{
	const uint8_t status = read_register(ISM330DLC_ADDR_STATUS);

	if ((status & (STATUS_XLDA | STATUS_GDA)) != (STATUS_XLDA | STATUS_GDA)) {
		ScheduleDelayed(5_ms);
		return;
	}

	MeasurementData data {};

	if (read_measurement(&data) != PX4_OK) {
		perf_count(_comms_error_perf);
		ScheduleDelayed(10_ms);
		return;
	}

	const hrt_abstime timestamp_sample = hrt_absolute_time();

	const int16_t gx = combine(data.gx_l, data.gx_h);
	const int16_t gy = combine(data.gy_l, data.gy_h);
	const int16_t gz = combine(data.gz_l, data.gz_h);

	const int16_t ax = combine(data.ax_l, data.ax_h);
	const int16_t ay = combine(data.ay_l, data.ay_h);
	const int16_t az = combine(data.az_l, data.az_h);

	// Datasheet conversion: 25 C + raw/16.
	const int16_t temp_raw = combine(data.temp_l, data.temp_h);
	const float temp_c = 25.f + static_cast<float>(temp_raw) / 16.f;
	_px4_accel.set_temperature(temp_c);
	_px4_gyro.set_temperature(temp_c);

	_px4_gyro.update(timestamp_sample, gx, gy, gz);
	_px4_accel.update(timestamp_sample, ax, ay, az);

	const uint64_t error_count = perf_event_count(_comms_error_perf);
	_px4_gyro.set_error_count(error_count);
	_px4_accel.set_error_count(error_count);

	perf_count(_sample_perf);
	ScheduleDelayed(5_ms);
}

uint8_t ISM330DLC::read_register(uint8_t reg)
{
	uint8_t value = 0;

	if (_interface->read(reg, &value, sizeof(value)) != PX4_OK) {
		perf_count(_comms_error_perf);
	}

	return value;
}

int ISM330DLC::read_measurement(MeasurementData *data)
{
	return _interface->read(ISM330DLC_ADDR_OUT_TEMP_L, data, sizeof(*data));
}

void ISM330DLC::write_register(uint8_t reg, uint8_t value)
{
	if (_interface->write(reg, &value, sizeof(value)) != PX4_OK) {
		perf_count(_comms_error_perf);
	}
}

void ISM330DLC::print_status()
{
	I2CSPIDriverBase::print_status();
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_error_perf);
}
