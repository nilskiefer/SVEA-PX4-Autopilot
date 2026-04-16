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

#include "bq769x2_protocol.h"
#include "bq769x2_registers.h"

#include <drivers/device/i2c.h>
#include <drivers/drv_hrt.h>
#include <lib/perf/perf_counter.h>
#include <mathlib/mathlib.h>
#include <px4_platform_common/i2c_spi_buses.h>
#include <px4_platform_common/module_params.h>
#include <px4_platform_common/param.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>
#include <uORB/SubscriptionInterval.hpp>
#include <uORB/topics/battery_info.h>
#include <uORB/topics/battery_status.h>
#include <uORB/topics/parameter_update.h>

using namespace time_literals;

class BQ769x2 : public device::I2C, public ModuleParams, public I2CSPIDriver<BQ769x2>, public BQ769x2ProtocolBus
{
public:
	BQ769x2(const I2CSPIDriverConfig &config, int battery_index);
	~BQ769x2() override;

	static I2CSPIDriverBase *instantiate(const I2CSPIDriverConfig &config, int runtime_instance);
	static void print_usage();

	int init() override;
	void RunImpl();
	void print_status() override;
	int busTransfer(const uint8_t *send, unsigned send_len, uint8_t *recv, unsigned recv_len) override
	{
		return transfer(send, send_len, recv, recv_len);
	}

protected:
	int probe() override;

private:
	static constexpr uint32_t SAMPLE_INTERVAL_US{100_ms};
	static constexpr uint8_t MAX_CELL_COUNT{14};

	int collectAndPublish();
	int configure();
	int configureProtections();
	int ensureFetEnable();
	int applyFetPolicy(bool bq_all_ok);
	int waitForPrechargeEqualization();
	uint8_t prechargeStageMask(uint8_t desired_mask) const;
	void publishDisconnected();
	void updateParamsFromStore();
	uint16_t activeVcellModeMask() const;
	uint8_t requestedFetMask() const;
	uint16_t mapFaults(uint8_t safety_a, uint8_t safety_b, uint8_t safety_c) const;
	uint8_t computeWarning(float remaining, uint16_t faults) const;
	float estimateRemaining(float avg_cell_voltage) const;

	BQ769x2Protocol _protocol;

	perf_counter_t _sample_perf{perf_alloc(PC_ELAPSED, "bq769x2_sample")};
	perf_counter_t _comms_errors{perf_alloc(PC_COUNT, "bq769x2_com_err")};
	perf_counter_t _collection_errors{perf_alloc(PC_COUNT, "bq769x2_collect_err")};

	uORB::SubscriptionInterval _parameter_update_sub{ORB_ID(parameter_update), 1_s};
	orb_advert_t _battery_status_topic{nullptr};
	orb_advert_t _battery_info_topic{nullptr};

	uint8_t _battery_id{1};
	uint8_t _cell_count{3};
	bool _configure_on_startup{true};
	bool _connected{false};
	hrt_abstime _last_integration_us{0};
	float _discharged_mah{0.f};
	float _discharged_wh{0.f};

	uint16_t _device_number{0};
	uint32_t _fw_version{0};
	uint32_t _hw_version{0};

	float _bat_low_thr{0.3f};
	float _bat_crit_thr{0.2f};
	float _bat_emerg_thr{0.1f};
	float _cell_voltage_empty{3.2f};
	float _cell_voltage_charged{4.2f};
	float _shunt_uohm{1000.f};
	float _cell_ov_limit_v{4.25f};
	float _cell_ov_reset_v{4.10f};
	float _cell_ov_delay_ms{1000.f};
	float _cell_uv_limit_v{2.80f};
	float _cell_uv_reset_v{3.00f};
	float _cell_uv_delay_ms{1000.f};
	float _occ_limit_a{120.f};
	float _occ_delay_ms{100.f};
	float _ocd_limit_a{130.f};
	float _ocd_delay_ms{20.f};
	float _scd_limit_a{220.f};
	float _scd_delay_us{60.f};
	float _chg_ot_limit_c{45.f};
	float _chg_ut_limit_c{0.f};
	float _dis_ot_limit_c{60.f};
	float _dis_ut_limit_c{-20.f};
	float _temp_hyst_c{5.f};
	float _capacity_mah{0.f};
	float _body_diode_th_ma{500.f};
	uint16_t _conf_power{0x2882};
	uint8_t _fet_options{0x1D};
	uint8_t _fets_on_mask{static_cast<uint8_t>(BQ769X2_FET_CHG | BQ769X2_FET_DSG)};
	uint8_t _precharge_mask{BQ769X2_FET_PDSG};
	uint16_t _precharge_ms{50};
	uint16_t _postcharge_ms{10};
	uint16_t _precharge_timeout_ms{500};
	float _precharge_delta_pct{5.f};    ///< |Vstack-Vpack| <= pct/100 * Vpack to pass equalization
	bool _fets_auto{true};
	bool _fets_all_ok_gate{true};
	bool _openwire_check{true};
	bool _temp_prot_enable{false};
	bool _crc_param_enabled{false};
	bool _fets_initialized{false};
	uint8_t _openwire_check_time_s{10};
	float _openwire_tol_mv_per_cell{50.f};
	uint16_t _vcell_mode_mask{0};

	param_t _param_cells{PARAM_INVALID};
	param_t _param_crc{PARAM_INVALID};
	param_t _param_configure{PARAM_INVALID};
	param_t _param_low_thr{PARAM_INVALID};
	param_t _param_crit_thr{PARAM_INVALID};
	param_t _param_emerg_thr{PARAM_INVALID};
	param_t _param_v_empty{PARAM_INVALID};
	param_t _param_v_charged{PARAM_INVALID};
	param_t _param_shunt_uohm{PARAM_INVALID};
	param_t _param_capacity{PARAM_INVALID};
	param_t _param_cell_ov_v{PARAM_INVALID};
	param_t _param_cell_ov_reset_v{PARAM_INVALID};
	param_t _param_cell_ov_delay_ms{PARAM_INVALID};
	param_t _param_cell_uv_v{PARAM_INVALID};
	param_t _param_cell_uv_reset_v{PARAM_INVALID};
	param_t _param_cell_uv_delay_ms{PARAM_INVALID};
	param_t _param_occ_a{PARAM_INVALID};
	param_t _param_occ_delay_ms{PARAM_INVALID};
	param_t _param_ocd_a{PARAM_INVALID};
	param_t _param_ocd_delay_ms{PARAM_INVALID};
	param_t _param_scd_a{PARAM_INVALID};
	param_t _param_scd_delay_us{PARAM_INVALID};
	param_t _param_chg_ot_c{PARAM_INVALID};
	param_t _param_chg_ut_c{PARAM_INVALID};
	param_t _param_dis_ot_c{PARAM_INVALID};
	param_t _param_dis_ut_c{PARAM_INVALID};
	param_t _param_temp_hyst_c{PARAM_INVALID};
	param_t _param_temp_prot_enable{PARAM_INVALID};
	param_t _param_conf_power{PARAM_INVALID};
	param_t _param_body_diode_ma{PARAM_INVALID};
	param_t _param_fet_options{PARAM_INVALID};
	param_t _param_fets_auto{PARAM_INVALID};
	param_t _param_fets_on_mask{PARAM_INVALID};
	param_t _param_precharge_mask{PARAM_INVALID};
	param_t _param_precharge_ms{PARAM_INVALID};
	param_t _param_postcharge_ms{PARAM_INVALID};
	param_t _param_precharge_timeout_ms{PARAM_INVALID};
	param_t _param_precharge_delta_pct{PARAM_INVALID};
	param_t _param_fets_all_ok_gate{PARAM_INVALID};
	param_t _param_openwire_check{PARAM_INVALID};
	param_t _param_openwire_check_time_s{PARAM_INVALID};
	param_t _param_openwire_tol_mv_per_cell{PARAM_INVALID};
	param_t _param_vcell_mode{PARAM_INVALID};
};
