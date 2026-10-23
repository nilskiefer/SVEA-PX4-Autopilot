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
 * @file svea_ina3221.h
 */

#pragma once

#include <drivers/device/i2c.h>
#include <drivers/drv_hrt.h>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/getopt.h>
#include <px4_platform_common/i2c_spi_buses.h>
#include <px4_platform_common/px4_config.h>
#include <uORB/PublicationMulti.hpp>
#include <uORB/topics/power_monitor.h>

#define INA3221_BASEADDR 0x40
#define INA3221_INIT_RETRY_INTERVAL_US 500000

#define INA3221_REG_CONFIG 0x00
#define INA3221_REG_CH1_SHUNT_VOLTAGE 0x01
#define INA3221_REG_CH1_BUS_VOLTAGE 0x02
#define INA3221_REG_CH2_SHUNT_VOLTAGE 0x03
#define INA3221_REG_CH2_BUS_VOLTAGE 0x04
#define INA3221_REG_CH3_SHUNT_VOLTAGE 0x05
#define INA3221_REG_CH3_BUS_VOLTAGE 0x06
#define INA3221_REG_MASKENABLE 0x0F
#define INA3221_REG_MANUFACTURER_ID 0xFE
#define INA3221_REG_DIE_ID 0xFF

#define INA3221_CONFIG_MODE_BOTH_CONT 0x7
#define INA3221_CONFIG_VSHUNTCT_1100_US (0x4 << 3)
#define INA3221_CONFIG_VBUSCT_1100_US (0x4 << 6)
#define INA3221_CONFIG_AVG_NSAMPLES_64 (0x3 << 9)
#define INA3221_CONFIG_CH1_EN (1 << 14)
#define INA3221_CONFIG_CH2_EN (1 << 13)
#define INA3221_CONFIG_CH3_EN (1 << 12)
#define INA3221_CONFIG_RST (1 << 15)

#define INA3221_MFG_ID_TI 0x5449
#define INA3221_DIE_ID 0x3220

#define INA3221_CONFIG_DEFAULT (INA3221_CONFIG_CH1_EN | INA3221_CONFIG_CH2_EN | INA3221_CONFIG_CH3_EN | \
				INA3221_CONFIG_AVG_NSAMPLES_64 | INA3221_CONFIG_VBUSCT_1100_US | \
				INA3221_CONFIG_VSHUNTCT_1100_US | INA3221_CONFIG_MODE_BOTH_CONT)

#define INA3221_CHANNEL_COUNT 3
#define INA3221_SAMPLE_FREQUENCY_HZ 4
#define INA3221_SAMPLE_INTERVAL_US (1000000 / INA3221_SAMPLE_FREQUENCY_HZ)
#define INA3221_BUS_VOLTAGE_LSB_V 0.008f
#define INA3221_SHUNT_VOLTAGE_LSB_V 0.000040f
#define INA3221_DEFAULT_SHUNT_OHM 0.016f

#define swap16(w) __builtin_bswap16((w))

class SVEA_INA3221 : public device::I2C, public I2CSPIDriver<SVEA_INA3221>
{
public:
	SVEA_INA3221(const I2CSPIDriverConfig &config);
	~SVEA_INA3221() override;

	static I2CSPIDriverBase *instantiate(const I2CSPIDriverConfig &config, int runtime_instance);
	static void print_usage();

	void RunImpl();
	int init() override;
	int force_init();
	void print_status() override;

protected:
	int probe() override;

private:
	void start();
	int collect();

	int read_reg(uint8_t reg, uint16_t &value);
	int write_reg(uint8_t reg, uint16_t value);
	void publish_channel(unsigned channel, int16_t bus_raw, int16_t shunt_raw, int16_t mask_enable, bool valid);

	static float shunt_from_packed_milliohm(int packed, unsigned channel);
	static int16_t clamp_to_i16(int32_t v);

	bool _initialized{false};
	uint16_t _config{INA3221_CONFIG_DEFAULT};
	float _rshunt_ohm[INA3221_CHANNEL_COUNT] {INA3221_DEFAULT_SHUNT_OHM, INA3221_DEFAULT_SHUNT_OHM, INA3221_DEFAULT_SHUNT_OHM};
	struct ChannelSnapshot {
		bool valid{false};
		hrt_abstime timestamp{0};
		int16_t bus_raw{0};
		int16_t shunt_raw{0};
		float voltage_v{0.f};
		float current_a{0.f};
		float power_w{0.f};
	};
	ChannelSnapshot _channel_snapshot[INA3221_CHANNEL_COUNT] {};

	perf_counter_t _sample_perf;
	perf_counter_t _comms_errors;
	perf_counter_t _collection_errors;

	uORB::PublicationMulti<power_monitor_s> _pm_pub_topic[INA3221_CHANNEL_COUNT] {
		{ORB_ID(power_monitor)},
		{ORB_ID(power_monitor)},
		{ORB_ID(power_monitor)}
	};
	power_monitor_s _pm_status[INA3221_CHANNEL_COUNT] {};
};
