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

#include "bq769x2.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#include <drivers/drv_hrt.h>
#include <px4_platform_common/defines.h>
#include <px4_platform_common/time.h>
#include <uORB/uORB.h>

extern "C" __EXPORT int bq769x2_main(int argc, char *argv[]);

BQ769x2::BQ769x2(const I2CSPIDriverConfig &config, int battery_index) :
	I2C(config),
	ModuleParams(nullptr),
	I2CSPIDriver(config),
	_protocol(*this, config.i2c_address, false)
{
	_battery_id = static_cast<uint8_t>(math::constrain(battery_index, 1, static_cast<int>(battery_status_s::MAX_INSTANCES)));

	_param_cells = param_find("BQ769X2_CELLS");
	_param_crc = param_find("BQ769X2_CRC");
	_param_configure = param_find("BQ769X2_CFG");
	_param_low_thr = param_find("BAT_LOW_THR");
	_param_crit_thr = param_find("BAT_CRIT_THR");
	_param_emerg_thr = param_find("BAT_EMERGEN_THR");
	_param_v_empty = param_find("BAT1_V_EMPTY");
	_param_v_charged = param_find("BAT1_V_CHARGED");
	_param_shunt_uohm = param_find("BQ769X2_SHUNT");
	_param_capacity = param_find("BAT1_CAPACITY");
	_param_cell_ov_v = param_find("BQ769X2_COV_V");
	_param_cell_ov_reset_v = param_find("BQ769X2_COV_RV");
	_param_cell_ov_delay_ms = param_find("BQ769X2_COV_DLY");
	_param_cell_uv_v = param_find("BQ769X2_CUV_V");
	_param_cell_uv_reset_v = param_find("BQ769X2_CUV_RV");
	_param_cell_uv_delay_ms = param_find("BQ769X2_CUV_DLY");
	_param_occ_a = param_find("BQ769X2_OCC_A");
	_param_occ_delay_ms = param_find("BQ769X2_OCC_DLY");
	_param_ocd_a = param_find("BQ769X2_OCD_A");
	_param_ocd_delay_ms = param_find("BQ769X2_OCD_DLY");
	_param_scd_a = param_find("BQ769X2_SCD_A");
	_param_scd_delay_us = param_find("BQ769X2_SCD_DLY");
	_param_chg_ot_c = param_find("BQ769X2_OTC_C");
	_param_chg_ut_c = param_find("BQ769X2_UTC_C");
	_param_dis_ot_c = param_find("BQ769X2_OTD_C");
	_param_dis_ut_c = param_find("BQ769X2_UTD_C");
	_param_temp_hyst_c = param_find("BQ769X2_T_HYST_C");
	_param_temp_prot_enable = param_find("BQ769X2_TPROT_EN");
	_param_conf_power = param_find("BQ769X2_PWR_CFG");
	_param_body_diode_ma = param_find("BQ769X2_DIODEMA");
	_param_fet_options = param_find("BQ769X2_FETOPT");
	_param_fets_auto = param_find("BQ769X2_FET_AUTO");
	_param_fets_on_mask = param_find("BQ769X2_FETMASK");
	_param_precharge_mask = param_find("BQ769X2_PCHGMASK");
	_param_precharge_ms = param_find("BQ769X2_PCHG_MS");
	_param_postcharge_ms = param_find("BQ769X2_POST_MS");
	_param_precharge_timeout_ms = param_find("BQ769X2_PTO_MS");
	_param_precharge_delta_mv = param_find("BQ769X2_PDV_MV");
	_param_fets_all_ok_gate = param_find("BQ769X2_ALL_OK");
	_param_openwire_check = param_find("BQ769X2_OW_CHK");
	_param_openwire_check_time_s = param_find("BQ769X2_OW_TIME");
	_param_openwire_tol_mv_per_cell = param_find("BQ769X2_OWTOL");
	_param_vcell_mode = param_find("BQ769X2_VCMODE");
}

BQ769x2::~BQ769x2()
{
	ScheduleClear();
	orb_unadvertise(_battery_status_topic);
	orb_unadvertise(_battery_info_topic);
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_collection_errors);
}

I2CSPIDriverBase *BQ769x2::instantiate(const I2CSPIDriverConfig &config, int runtime_instance)
{
	BQ769x2 *instance = new BQ769x2(config, config.custom1);

	if (instance == nullptr) {
		PX4_ERR("alloc failed");
		return nullptr;
	}

	if (instance->init() != PX4_OK) {
		delete instance;
		return nullptr;
	}

	return instance;
}

int BQ769x2::init()
{
	if (I2C::init() != PX4_OK) {
		PX4_ERR("i2c init failed");
		return PX4_ERROR;
	}

	updateParamsFromStore();

	const uint16_t vcell_mode = activeVcellModeMask();
	const int selected_channels = __builtin_popcount(vcell_mode);

	if (selected_channels < 1 || selected_channels > MAX_CELL_COUNT) {
		PX4_ERR("invalid vcell mode (mask=0x%04x, channels=%d)", vcell_mode, selected_channels);
		return PX4_ERROR;
	}

	if (probe() != PX4_OK) {
		PX4_ERR("probe failed");
		return PX4_ERROR;
	}

	if (_configure_on_startup && configure() != PX4_OK) {
		PX4_ERR("config failed");
		return PX4_ERROR;
	}

	if (ensureFetEnable() != PX4_OK) {
		PX4_ERR("FET_ENABLE failed");
		return PX4_ERROR;
	}

	ScheduleOnInterval(SAMPLE_INTERVAL_US);
	return PX4_OK;
}

int BQ769x2::probe()
{
	// Some boards can ACK on I2C before subcommand transactions are ready
	// (for example right after wake from SHIP/sleep). Probe with bounded retries.
	// Also auto-detect CRC mode: try configured mode first, then opposite mode.
	static constexpr int kProbeAttempts = 20;

	const auto try_probe = [&](bool crc_enabled, int &ret_device, int &ret_fw, int &ret_hw) -> bool {
		_protocol.setCRCEnabled(crc_enabled);
		ret_device = PX4_ERROR;
		ret_fw = PX4_ERROR;
		ret_hw = PX4_ERROR;
		bool device_ok = false;

		for (int attempt = 0; attempt < kProbeAttempts; attempt++) {
			ret_device = _protocol.subcommandReadU2(BQ769X2_SUBCMD_DEVICE_NUMBER, _device_number);

			if (ret_device == PX4_OK) {
				device_ok = true;
				// Read metadata with dedicated retries; these reads are occasionally
				// transiently flaky right after wake/reset even when core comms works.
				static constexpr int kMetaAttempts = 10;

				for (int fw_attempt = 0; fw_attempt < kMetaAttempts; fw_attempt++) {
					ret_fw = _protocol.subcommandReadU4(BQ769X2_SUBCMD_FW_VERSION, _fw_version);

					if (ret_fw == PX4_OK) {
						break;
					}

					uint16_t battery_status = 0;
					(void)_protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, battery_status);
					px4_usleep(2'000);
				}

				if (ret_fw == PX4_OK) {
					for (int hw_attempt = 0; hw_attempt < kMetaAttempts; hw_attempt++) {
						ret_hw = _protocol.subcommandReadU4(BQ769X2_SUBCMD_HW_VERSION, _hw_version);

						if (ret_hw == PX4_OK) {
							return true;
						}

						uint16_t battery_status = 0;
						(void)_protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, battery_status);
						px4_usleep(2'000);
					}
				}
			}

			// Wake ping via direct command path before next subcommand retry.
			uint16_t battery_status = 0;
			(void)_protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, battery_status);
			px4_usleep(10'000);
		}

		// Accept partial probe if device number is readable. Some parts can
		// transiently fail FW/HW metadata reads at boot while core commands work.
		if (device_ok) {
			if (ret_fw != PX4_OK) {
				_fw_version = 0;
			}

			if (ret_hw != PX4_OK) {
				_hw_version = 0;
			}

			PX4_WARN("probe partial (dev ok, fw:%d hw:%d)", ret_fw, ret_hw);
			return true;
		}

		return false;
	};

	int ret_device = PX4_ERROR;
	int ret_fw = PX4_ERROR;
	int ret_hw = PX4_ERROR;

	if (!try_probe(_crc_param_enabled, ret_device, ret_fw, ret_hw)) {
		if (!try_probe(!_crc_param_enabled, ret_device, ret_fw, ret_hw)) {
			PX4_ERR("probe failed dev:%d fw:%d hw:%d", ret_device, ret_fw, ret_hw);
			return PX4_ERROR;
		}

		PX4_WARN("CRC auto-detected: using %d (param BQ769X2_CRC=%d)", !_crc_param_enabled ? 1 : 0,
			 _crc_param_enabled ? 1 : 0);
	}

	_connected = true;
	return PX4_OK;
}

int BQ769x2::configure()
{
	const int ret_enter = _protocol.setConfigUpdateMode(true);

	if (ret_enter != PX4_OK) {
		return ret_enter;
	}

	int ret = PX4_OK;

	if (_shunt_uohm < 1.f) {
		ret = PX4_ERROR;
	} else {
		// LibreSolar convention: calibration gain derived from shunt resistance in uOhm.
		ret |= _protocol.datamemWriteF4(BQ769X2_CAL_CURR_CC_GAIN, 7568.4f / _shunt_uohm);
	}

	// Match LibreSolar convention: CC2 current in 10 mA and stack/pack voltage in 10 mV.
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_CONF_DA, 0x06);

	ret |= _protocol.datamemWriteU2(BQ769X2_SET_CONF_POWER, _conf_power);

	ret |= _protocol.datamemWriteU2(BQ769X2_SET_CONF_VCELL_MODE, activeVcellModeMask());
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_FET_OPTIONS, _fet_options);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_FET_PDSG_TIMEOUT, 0);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_OPEN_WIRE_CHECK_TIME, _openwire_check_time_s);

	ret |= _protocol.datamemWriteU2(BQ769X2_SET_PROT_BODY_DIODE_TH, static_cast<uint16_t>(math::constrain(lroundf(_body_diode_th_ma),
								 0l, 32767l)));

	ret |= configureProtections();

	const int ret_exit = _protocol.setConfigUpdateMode(false);
	ret |= ret_exit;

	return ret;
}

int BQ769x2::configureProtections()
{
	const auto validateRange = [](float value, float min, float max, const char *name) {
		if (value < min || value > max || !PX4_ISFINITE(value)) {
			PX4_ERR("%s out of range: %.3f (allowed %.3f..%.3f)", name, (double)value, (double)min, (double)max);
			return false;
		}

		return true;
	};

	int ret = PX4_OK;

	// COV: threshold/hysteresis/delay (50.6 mV units, 3.3 ms units).
	if (!validateRange(_cell_ov_limit_v, 1.5f, 5.6f, "COV_V")
	    || !validateRange(_cell_ov_delay_ms, 3.3f, 6755.f, "COV_DLY")
	    || !validateRange(_cell_ov_limit_v - _cell_ov_reset_v, 0.1012f, 1.012f, "COV_HYST")) {
		return PX4_ERROR;
	}

	const long cov_threshold_raw = lroundf(_cell_ov_limit_v * 1000.f / 50.6f);
	const long cov_hyst_raw = lroundf((_cell_ov_limit_v - _cell_ov_reset_v) * 1000.f / 50.6f);
	const long cov_delay_raw = lroundf(_cell_ov_delay_ms / 3.3f);

	if (cov_threshold_raw < 20 || cov_threshold_raw > 110
	    || cov_hyst_raw < 2 || cov_hyst_raw > 20
	    || cov_delay_raw < 1 || cov_delay_raw > 2047) {
		PX4_ERR("COV config not representable");
		return PX4_ERROR;
	}

	const uint8_t cov_threshold = static_cast<uint8_t>(cov_threshold_raw);
	const uint8_t cov_hyst = static_cast<uint8_t>(cov_hyst_raw);
	const uint16_t cov_delay = static_cast<uint16_t>(cov_delay_raw);

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_COV_THRESHOLD, cov_threshold);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_COV_RECOV_HYST, cov_hyst);
	ret |= _protocol.datamemWriteU2(BQ769X2_PROT_COV_DELAY, cov_delay);

	// CUV: threshold/hysteresis/delay (50.6 mV units, 3.3 ms units).
	if (!validateRange(_cell_uv_limit_v, 1.5f, 4.6f, "CUV_V")
	    || !validateRange(_cell_uv_delay_ms, 3.3f, 6755.f, "CUV_DLY")
	    || !validateRange(_cell_uv_reset_v - _cell_uv_limit_v, 0.1012f, 1.012f, "CUV_HYST")) {
		return PX4_ERROR;
	}

	const long cuv_threshold_raw = lroundf(_cell_uv_limit_v * 1000.f / 50.6f);
	const long cuv_hyst_raw = lroundf((_cell_uv_reset_v - _cell_uv_limit_v) * 1000.f / 50.6f);
	const long cuv_delay_raw = lroundf(_cell_uv_delay_ms / 3.3f);

	if (cuv_threshold_raw < 20 || cuv_threshold_raw > 90
	    || cuv_hyst_raw < 2 || cuv_hyst_raw > 20
	    || cuv_delay_raw < 1 || cuv_delay_raw > 2047) {
		PX4_ERR("CUV config not representable");
		return PX4_ERROR;
	}

	const uint8_t cuv_threshold = static_cast<uint8_t>(cuv_threshold_raw);
	const uint8_t cuv_hyst = static_cast<uint8_t>(cuv_hyst_raw);
	const uint16_t cuv_delay = static_cast<uint16_t>(cuv_delay_raw);

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_CUV_THRESHOLD, cuv_threshold);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_CUV_RECOV_HYST, cuv_hyst);
	ret |= _protocol.datamemWriteU2(BQ769X2_PROT_CUV_DELAY, cuv_delay);

	// OCC/OCD thresholds and delays (2 mV units, 3.3 ms base + 6.6 ms offset).
	if (!validateRange(_occ_limit_a, 1.f, 500.f, "OCC_A")
	    || !validateRange(_occ_delay_ms, 6.6f, 425.f, "OCC_DLY")
	    || !validateRange(_ocd_limit_a, 1.f, 500.f, "OCD_A")
	    || !validateRange(_ocd_delay_ms, 6.6f, 425.f, "OCD_DLY")
	    || !validateRange(_scd_limit_a, 10.f, 1000.f, "SCD_A")
	    || !validateRange(_scd_delay_us, 0.f, 450.f, "SCD_DLY")) {
		return PX4_ERROR;
	}

	const long occ_threshold_raw = lroundf(_occ_limit_a * _shunt_uohm / 2000.f);
	const long occ_delay_raw = lroundf((_occ_delay_ms - 6.6f) / 3.3f);

	if (occ_threshold_raw < 2 || occ_threshold_raw > 62 || occ_delay_raw < 1 || occ_delay_raw > 127) {
		PX4_ERR("OCC config not representable for shunt %.1f uohm", (double)_shunt_uohm);
		return PX4_ERROR;
	}

	const uint8_t occ_threshold = static_cast<uint8_t>(occ_threshold_raw);
	const uint8_t occ_delay = static_cast<uint8_t>(occ_delay_raw);

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCC_THRESHOLD, occ_threshold);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCC_DELAY, occ_delay);

	const long ocd_threshold_raw = lroundf(_ocd_limit_a * _shunt_uohm / 2000.f);
	const long ocd_delay_raw = lroundf((_ocd_delay_ms - 6.6f) / 3.3f);

	if (ocd_threshold_raw < 2 || ocd_threshold_raw > 100 || ocd_delay_raw < 1 || ocd_delay_raw > 127) {
		PX4_ERR("OCD config not representable for shunt %.1f uohm", (double)_shunt_uohm);
		return PX4_ERROR;
	}

	const uint8_t ocd_threshold = static_cast<uint8_t>(ocd_threshold_raw);
	const uint8_t ocd_delay = static_cast<uint8_t>(ocd_delay_raw);

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCD1_THRESHOLD, ocd_threshold);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCD1_DELAY, ocd_delay);

	// SCD threshold uses discrete table (mV across shunt), delay in 15 us steps.
	static constexpr uint16_t scd_thresholds_mv[] {10, 20, 40, 60, 80, 100, 125, 150, 175, 200, 250, 300, 350, 400, 450, 500};
	const float scd_shunt_mv = _scd_limit_a * _shunt_uohm / 1000.f;
	const float min_scd_a = static_cast<float>(scd_thresholds_mv[0]) * 1000.f / _shunt_uohm;
	const float max_scd_a = static_cast<float>(scd_thresholds_mv[(sizeof(scd_thresholds_mv) / sizeof(scd_thresholds_mv[0])) - 1]) * 1000.f / _shunt_uohm;

	if (_scd_limit_a < min_scd_a || _scd_limit_a > max_scd_a) {
		PX4_ERR("SCD_A not representable for shunt %.1f uohm (%.1f..%.1f A)", (double)_shunt_uohm, (double)min_scd_a,
			(double)max_scd_a);
		return PX4_ERROR;
	}

	uint8_t scd_threshold_idx = 0;

	for (int i = static_cast<int>(sizeof(scd_thresholds_mv) / sizeof(scd_thresholds_mv[0])) - 1; i >= 0; i--) {
		if (scd_shunt_mv >= static_cast<float>(scd_thresholds_mv[i])) {
			scd_threshold_idx = static_cast<uint8_t>(i);
			break;
		}
	}

	const long scd_delay_raw = lroundf(_scd_delay_us / 15.f + 1.f);

	if (scd_delay_raw < 1 || scd_delay_raw > 31) {
		PX4_ERR("SCD_DLY not representable");
		return PX4_ERROR;
	}

	const uint8_t scd_delay = static_cast<uint8_t>(scd_delay_raw);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_SCD_THRESHOLD, scd_threshold_idx);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_SCD_DELAY, scd_delay);

	// Temperature protections (degC, signed 8-bit values with hysteresis).
	if (_temp_prot_enable) {
		if (_dis_ot_limit_c < 0.f || _chg_ot_limit_c < 0.f
		    || _dis_ot_limit_c < (_dis_ut_limit_c + 20.f)
		    || _chg_ot_limit_c < (_chg_ut_limit_c + 20.f)
		    || !validateRange(_temp_hyst_c, 1.f, 20.f, "T_HYST")) {
			PX4_ERR("invalid temp limits");
			return PX4_ERROR;
		}

		const int8_t otc_threshold = static_cast<int8_t>(math::constrain(lroundf(_chg_ot_limit_c), -40l, 120l));
		const int8_t otd_threshold = static_cast<int8_t>(math::constrain(lroundf(_dis_ot_limit_c), -40l, 120l));
		const int8_t utc_threshold = static_cast<int8_t>(math::constrain(lroundf(_chg_ut_limit_c), -40l, 120l));
		const int8_t utd_threshold = static_cast<int8_t>(math::constrain(lroundf(_dis_ut_limit_c), -40l, 120l));
		const int8_t hyst = static_cast<int8_t>(math::constrain(lroundf(_temp_hyst_c), 1l, 20l));
		const int8_t otc_recovery = static_cast<int8_t>(math::constrain(static_cast<long>(otc_threshold - hyst), -40l, 120l));
		const int8_t otd_recovery = static_cast<int8_t>(math::constrain(static_cast<long>(otd_threshold - hyst), -40l, 120l));
		const int8_t utc_recovery = static_cast<int8_t>(math::constrain(static_cast<long>(utc_threshold + hyst), -40l, 120l));
		const int8_t utd_recovery = static_cast<int8_t>(math::constrain(static_cast<long>(utd_threshold + hyst), -40l, 120l));

		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OTC_THRESHOLD, static_cast<uint8_t>(otc_threshold));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OTC_RECOVERY, static_cast<uint8_t>(otc_recovery));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OTD_THRESHOLD, static_cast<uint8_t>(otd_threshold));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OTD_RECOVERY, static_cast<uint8_t>(otd_recovery));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_UTC_THRESHOLD, static_cast<uint8_t>(utc_threshold));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_UTC_RECOVERY, static_cast<uint8_t>(utc_recovery));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_UTD_THRESHOLD, static_cast<uint8_t>(utd_threshold));
		ret |= _protocol.datamemWriteU1(BQ769X2_PROT_UTD_RECOVERY, static_cast<uint8_t>(utd_recovery));
	}

	// Ensure these protections are enabled in PROT_ENABLED_A.
	uint8_t prot_enabled_a{0};
	ret |= _protocol.datamemReadU1(BQ769X2_SET_PROT_ENABLED_A, prot_enabled_a);
	prot_enabled_a |= static_cast<uint8_t>(BQ769X2_PROT_EN_A_CUV | BQ769X2_PROT_EN_A_COV | BQ769X2_PROT_EN_A_OCC
					       | BQ769X2_PROT_EN_A_OCD1 | BQ769X2_PROT_EN_A_SCD);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_PROT_ENABLED_A, prot_enabled_a);

	uint8_t prot_enabled_b{0};
	ret |= _protocol.datamemReadU1(BQ769X2_SET_PROT_ENABLED_B, prot_enabled_b);
	const uint8_t temp_bits = static_cast<uint8_t>(BQ769X2_PROT_EN_B_UTC | BQ769X2_PROT_EN_B_UTD | BQ769X2_PROT_EN_B_OTC
				| BQ769X2_PROT_EN_B_OTD);

	if (_temp_prot_enable) {
		prot_enabled_b |= temp_bits;

	} else {
		prot_enabled_b &= ~temp_bits;
	}

	ret |= _protocol.datamemWriteU1(BQ769X2_SET_PROT_ENABLED_B, prot_enabled_b);

	return ret;
}

void BQ769x2::updateParamsFromStore()
{
	int32_t val_i32{0};
	float val_f{0.f};

	if (_param_cells != PARAM_INVALID && param_get(_param_cells, &val_i32) == PX4_OK) {
		_cell_count = static_cast<uint8_t>(math::constrain(val_i32, static_cast<int32_t>(1), static_cast<int32_t>(MAX_CELL_COUNT)));
	}

	if (_param_crc != PARAM_INVALID && param_get(_param_crc, &val_i32) == PX4_OK) {
		_crc_param_enabled = val_i32 != 0;
		_protocol.setCRCEnabled(_crc_param_enabled);
	}

	if (_param_configure != PARAM_INVALID && param_get(_param_configure, &val_i32) == PX4_OK) {
		_configure_on_startup = val_i32 != 0;
	}

	if (_param_low_thr != PARAM_INVALID && param_get(_param_low_thr, &val_f) == PX4_OK) {
		_bat_low_thr = val_f;
	}

	if (_param_crit_thr != PARAM_INVALID && param_get(_param_crit_thr, &val_f) == PX4_OK) {
		_bat_crit_thr = val_f;
	}

	if (_param_emerg_thr != PARAM_INVALID && param_get(_param_emerg_thr, &val_f) == PX4_OK) {
		_bat_emerg_thr = val_f;
	}

	if (_param_v_empty != PARAM_INVALID && param_get(_param_v_empty, &val_f) == PX4_OK) {
		_cell_voltage_empty = val_f;
	}

	if (_param_v_charged != PARAM_INVALID && param_get(_param_v_charged, &val_f) == PX4_OK) {
		_cell_voltage_charged = val_f;
	}

	if (_param_shunt_uohm != PARAM_INVALID && param_get(_param_shunt_uohm, &val_f) == PX4_OK) {
		_shunt_uohm = math::max(val_f, 1.f);
	}

	if (_param_capacity != PARAM_INVALID && param_get(_param_capacity, &val_f) == PX4_OK) {
		_capacity_mah = math::max(val_f, 0.f);
	}

	if (_param_cell_ov_v != PARAM_INVALID && param_get(_param_cell_ov_v, &val_f) == PX4_OK) {
		_cell_ov_limit_v = val_f;
	}

	if (_param_cell_ov_reset_v != PARAM_INVALID && param_get(_param_cell_ov_reset_v, &val_f) == PX4_OK) {
		_cell_ov_reset_v = val_f;
	}

	if (_param_cell_ov_delay_ms != PARAM_INVALID && param_get(_param_cell_ov_delay_ms, &val_f) == PX4_OK) {
		_cell_ov_delay_ms = val_f;
	}

	if (_param_cell_uv_v != PARAM_INVALID && param_get(_param_cell_uv_v, &val_f) == PX4_OK) {
		_cell_uv_limit_v = val_f;
	}

	if (_param_cell_uv_reset_v != PARAM_INVALID && param_get(_param_cell_uv_reset_v, &val_f) == PX4_OK) {
		_cell_uv_reset_v = val_f;
	}

	if (_param_cell_uv_delay_ms != PARAM_INVALID && param_get(_param_cell_uv_delay_ms, &val_f) == PX4_OK) {
		_cell_uv_delay_ms = val_f;
	}

	if (_param_occ_a != PARAM_INVALID && param_get(_param_occ_a, &val_f) == PX4_OK) {
		_occ_limit_a = val_f;
	}

	if (_param_occ_delay_ms != PARAM_INVALID && param_get(_param_occ_delay_ms, &val_f) == PX4_OK) {
		_occ_delay_ms = val_f;
	}

	if (_param_ocd_a != PARAM_INVALID && param_get(_param_ocd_a, &val_f) == PX4_OK) {
		_ocd_limit_a = val_f;
	}

	if (_param_ocd_delay_ms != PARAM_INVALID && param_get(_param_ocd_delay_ms, &val_f) == PX4_OK) {
		_ocd_delay_ms = val_f;
	}

	if (_param_scd_a != PARAM_INVALID && param_get(_param_scd_a, &val_f) == PX4_OK) {
		_scd_limit_a = val_f;
	}

	if (_param_scd_delay_us != PARAM_INVALID && param_get(_param_scd_delay_us, &val_f) == PX4_OK) {
		_scd_delay_us = val_f;
	}

	if (_param_chg_ot_c != PARAM_INVALID && param_get(_param_chg_ot_c, &val_f) == PX4_OK) {
		_chg_ot_limit_c = val_f;
	}

	if (_param_chg_ut_c != PARAM_INVALID && param_get(_param_chg_ut_c, &val_f) == PX4_OK) {
		_chg_ut_limit_c = val_f;
	}

	if (_param_dis_ot_c != PARAM_INVALID && param_get(_param_dis_ot_c, &val_f) == PX4_OK) {
		_dis_ot_limit_c = val_f;
	}

	if (_param_dis_ut_c != PARAM_INVALID && param_get(_param_dis_ut_c, &val_f) == PX4_OK) {
		_dis_ut_limit_c = val_f;
	}

	if (_param_temp_hyst_c != PARAM_INVALID && param_get(_param_temp_hyst_c, &val_f) == PX4_OK) {
		_temp_hyst_c = val_f;
	}

	if (_param_temp_prot_enable != PARAM_INVALID && param_get(_param_temp_prot_enable, &val_i32) == PX4_OK) {
		_temp_prot_enable = val_i32 != 0;
	}

	if (_param_conf_power != PARAM_INVALID && param_get(_param_conf_power, &val_i32) == PX4_OK) {
		_conf_power = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(0xFFFF)));
	}

	if (_param_body_diode_ma != PARAM_INVALID && param_get(_param_body_diode_ma, &val_f) == PX4_OK) {
		_body_diode_th_ma = val_f;
	}

	if (_param_fet_options != PARAM_INVALID && param_get(_param_fet_options, &val_i32) == PX4_OK) {
		_fet_options = static_cast<uint8_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(255)));
	}

	if (_param_fets_auto != PARAM_INVALID && param_get(_param_fets_auto, &val_i32) == PX4_OK) {
		_fets_auto = val_i32 != 0;
	}

	if (_param_fets_on_mask != PARAM_INVALID && param_get(_param_fets_on_mask, &val_i32) == PX4_OK) {
		_fets_on_mask = static_cast<uint8_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(15)));
	}

	if (_param_precharge_mask != PARAM_INVALID && param_get(_param_precharge_mask, &val_i32) == PX4_OK) {
		_precharge_mask = static_cast<uint8_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(15)));
	}

	if (_param_precharge_ms != PARAM_INVALID && param_get(_param_precharge_ms, &val_i32) == PX4_OK) {
		_precharge_ms = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(2000)));
	}

	if (_param_postcharge_ms != PARAM_INVALID && param_get(_param_postcharge_ms, &val_i32) == PX4_OK) {
		_postcharge_ms = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(2000)));
	}

	if (_param_precharge_timeout_ms != PARAM_INVALID && param_get(_param_precharge_timeout_ms, &val_i32) == PX4_OK) {
		_precharge_timeout_ms = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(1), static_cast<int32_t>(5000)));
	}

	if (_param_precharge_delta_mv != PARAM_INVALID && param_get(_param_precharge_delta_mv, &val_f) == PX4_OK) {
		_precharge_delta_mv = math::constrain(val_f, 1.f, 5000.f);
	}

	if (_param_fets_all_ok_gate != PARAM_INVALID && param_get(_param_fets_all_ok_gate, &val_i32) == PX4_OK) {
		_fets_all_ok_gate = val_i32 != 0;
	}

	if (_param_openwire_check != PARAM_INVALID && param_get(_param_openwire_check, &val_i32) == PX4_OK) {
		_openwire_check = val_i32 != 0;
	}

	if (_param_openwire_check_time_s != PARAM_INVALID && param_get(_param_openwire_check_time_s, &val_i32) == PX4_OK) {
		_openwire_check_time_s = static_cast<uint8_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(255)));
	}

	if (_param_openwire_tol_mv_per_cell != PARAM_INVALID && param_get(_param_openwire_tol_mv_per_cell, &val_f) == PX4_OK) {
		_openwire_tol_mv_per_cell = math::max(val_f, 1.f);
	}

	if (_param_vcell_mode != PARAM_INVALID && param_get(_param_vcell_mode, &val_i32) == PX4_OK) {
		_vcell_mode_mask = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(0xFFFF)));
	}
}

void BQ769x2::RunImpl()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}

	if (_parameter_update_sub.updated()) {
		parameter_update_s parameter_update{};
		_parameter_update_sub.copy(&parameter_update);
		updateParamsFromStore();
	}

	if (collectAndPublish() != PX4_OK) {
		perf_count(_collection_errors);
		publishDisconnected();
	}
}

int BQ769x2::collectAndPublish()
{
	perf_begin(_sample_perf);

	battery_status_s report{};
	report.timestamp = hrt_absolute_time();
	report.id = _battery_id;
	report.priority = static_cast<uint8_t>(_battery_id - 1);
	report.connected = true;
	report.source = battery_status_s::SOURCE_EXTERNAL;
	report.scale = 1.f;
	report.current_average_a = -1.f;
	report.discharged_mah = _discharged_mah;
	report.remaining = -1.f;
	report.time_remaining_s = NAN;
	report.temperature = NAN;
	report.cell_count = 0;
	report.capacity = static_cast<uint16_t>(math::constrain(lroundf(_capacity_mah), 0l, 65535l));
	report.interface_error = static_cast<uint16_t>(perf_event_count(_comms_errors));

	int16_t raw_i16{0};

	if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_PACK, raw_i16) != PX4_OK) {
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return PX4_ERROR;
	}

	report.voltage_v = static_cast<float>(raw_i16) * 1e-2f;

	if (_protocol.directReadI2(BQ769X2_CMD_CURRENT_CC2, raw_i16) != PX4_OK) {
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return PX4_ERROR;
	}

	report.current_a = static_cast<float>(raw_i16) * 1e-2f;
	report.current_average_a = report.current_a;

	// Coulomb counter integration (LibreSolar-style): integrate signed current
	// over elapsed time and track consumed capacity/energy.
	if (_last_integration_us != 0 && report.timestamp > _last_integration_us) {
		const float dt_s = (report.timestamp - _last_integration_us) * 1e-6f;

		// Ignore long gaps to avoid large jumps after resets/restarts.
		if (dt_s > 0.f && dt_s < 1.f) {
			const float signed_current_a = (fabsf(report.current_a) > 0.05f) ? report.current_a : 0.f;

			_discharged_mah += (signed_current_a * 1000.f) * (dt_s / 3600.f);
			_discharged_mah = math::max(_discharged_mah, 0.f);

			if (_capacity_mah > 0.f) {
				_discharged_mah = math::min(_discharged_mah, _capacity_mah);
			}

			if (PX4_ISFINITE(report.voltage_v) && report.voltage_v > 0.f) {
				_discharged_wh += (signed_current_a * report.voltage_v) * (dt_s / 3600.f);
				_discharged_wh = math::max(_discharged_wh, 0.f);
			}
		}
	}

	_last_integration_us = report.timestamp;
	report.discharged_mah = _discharged_mah;

	float stack_voltage_v = NAN;

	if (_openwire_check) {
		if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_STACK, raw_i16) != PX4_OK) {
			perf_count(_comms_errors);
			perf_end(_sample_perf);
			return PX4_ERROR;
		}

		stack_voltage_v = static_cast<float>(raw_i16) * 1e-2f;
	}

	if (_protocol.directReadI2(BQ769X2_CMD_TEMP_INT, raw_i16) == PX4_OK) {
		report.temperature = (static_cast<float>(raw_i16) * 0.1f) - 273.15f;
	}

	float min_cell_v = FLT_MAX;
	float max_cell_v = -FLT_MAX;
	float sum_cell_v = 0.f;
	uint8_t valid_cells = 0;

	uint8_t out_cell_index = 0;
	const uint16_t vcell_mode = activeVcellModeMask();

	for (uint8_t channel = 0; channel < 16 && out_cell_index < sizeof(report.voltage_cell_v) / sizeof(report.voltage_cell_v[0]); channel++) {
		if ((vcell_mode & (1u << channel)) == 0) {
			continue;
		}

		if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_CELL_1 + (2 * channel), raw_i16) != PX4_OK) {
			perf_count(_comms_errors);
			perf_end(_sample_perf);
			return PX4_ERROR;
		}

		const float cell_v = static_cast<float>(raw_i16) * 1e-3f;
		report.voltage_cell_v[out_cell_index++] = cell_v;

		if (cell_v > 0.5f) {
			min_cell_v = math::min(min_cell_v, cell_v);
			max_cell_v = math::max(max_cell_v, cell_v);
			sum_cell_v += cell_v;
			valid_cells++;
		}
	}

	if (valid_cells > 0) {
		report.max_cell_voltage_delta = max_cell_v - min_cell_v;
		const float remaining_voltage_based = estimateRemaining(sum_cell_v / valid_cells);
		report.remaining = remaining_voltage_based;

		if (_capacity_mah > 0.f && PX4_ISFINITE(_discharged_mah) && _discharged_mah >= 0.f) {
			const float remaining_coulomb_based = math::constrain(1.f - (_discharged_mah / _capacity_mah), 0.f, 1.f);

			if (PX4_ISFINITE(remaining_voltage_based) && remaining_voltage_based >= 0.f) {
				report.remaining = math::min(remaining_voltage_based, remaining_coulomb_based);

			} else {
				report.remaining = remaining_coulomb_based;
			}
		}
	}

	report.cell_count = out_cell_index;

	if (_capacity_mah > 0.f) {
		const float nominal_voltage = static_cast<float>(report.cell_count) * 3.7f;

		if (nominal_voltage > 0.f) {
			report.nominal_voltage = nominal_voltage;
			report.full_charge_capacity_wh = (_capacity_mah * nominal_voltage) * 1e-3f;

			if (PX4_ISFINITE(_discharged_mah) && _discharged_mah >= 0.f) {
				const float remaining_capacity_mah = math::max(_capacity_mah - _discharged_mah, 0.f);
				report.remaining_capacity_wh = (remaining_capacity_mah * nominal_voltage) * 1e-3f;

			} else if (PX4_ISFINITE(report.remaining) && report.remaining >= 0.f) {
				report.remaining_capacity_wh = report.full_charge_capacity_wh * report.remaining;
			}
		}

		if (PX4_ISFINITE(report.remaining) && report.remaining >= 0.f && report.current_a > 0.1f && report.voltage_v > 0.f) {
			const float current_based_time = (_capacity_mah * report.remaining) / (report.current_a * 1000.f) * 3600.f;
			report.time_remaining_s = math::max(current_based_time, 0.f);
		}
	}

	bool open_wire_detected = false;

	if (_openwire_check && valid_cells > 0 && PX4_ISFINITE(stack_voltage_v)) {
		const float tolerance_v = (static_cast<float>(valid_cells) * _openwire_tol_mv_per_cell) * 1e-3f;
		const float deviation_v = fabsf(sum_cell_v - stack_voltage_v);
		open_wire_detected = deviation_v > tolerance_v;
	}

	uint8_t safety_a{0};
	uint8_t safety_b{0};
	uint8_t safety_c{0};

	if (_protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_A, safety_a) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_B, safety_b) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_C, safety_c) != PX4_OK) {
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return PX4_ERROR;
	}

	report.faults = mapFaults(safety_a, safety_b, safety_c);

	if (open_wire_detected) {
		report.faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_CELL_FAIL);
	}

	report.warning = computeWarning(report.remaining, report.faults);

	if (_fets_auto && applyFetPolicy(report.faults == 0) != PX4_OK) {
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return PX4_ERROR;
	}

	int instance = 0;
	orb_publish_auto(ORB_ID(battery_status), &_battery_status_topic, &report, &instance);

	battery_info_s info{};
	info.timestamp = report.timestamp;
	info.id = report.id;
	snprintf(info.serial_number, sizeof(info.serial_number), "BQ769x2:%u", _device_number);
	orb_publish_auto(ORB_ID(battery_info), &_battery_info_topic, &info, &instance);

	_connected = true;
	perf_end(_sample_perf);
	return PX4_OK;
}

void BQ769x2::publishDisconnected()
{
	if (_fets_auto) {
		(void)applyFetPolicy(false);
	}

	battery_status_s report{};
	report.timestamp = hrt_absolute_time();
	report.id = _battery_id;
	report.priority = static_cast<uint8_t>(_battery_id - 1);
	report.source = battery_status_s::SOURCE_EXTERNAL;
	report.connected = false;
	report.warning = battery_status_s::WARNING_FAILED;
	report.interface_error = static_cast<uint16_t>(perf_event_count(_comms_errors));

	int instance = 0;
	orb_publish_auto(ORB_ID(battery_status), &_battery_status_topic, &report, &instance);
	_connected = false;
}

int BQ769x2::ensureFetEnable()
{
	if (_fets_initialized) {
		return PX4_OK;
	}

	uint16_t mfg_status{0};

	if (_protocol.subcommandReadU2(BQ769X2_SUBCMD_MFG_STATUS, mfg_status) != PX4_OK) {
		return PX4_ERROR;
	}

	if ((mfg_status & BQ769X2_MFG_STATUS_FET_EN_MASK) == 0) {
		if (_protocol.subcommand(BQ769X2_SUBCMD_FET_ENABLE) != PX4_OK) {
			return PX4_ERROR;
		}
	}

	_fets_initialized = true;
	return PX4_OK;
}

int BQ769x2::applyFetPolicy(bool bq_all_ok)
{
	if (ensureFetEnable() != PX4_OK) {
		return PX4_ERROR;
	}

	const bool all_ok = bq_all_ok && _fets_all_ok_gate;
	const uint8_t desired_mask = all_ok ? requestedFetMask() : 0;
	uint8_t fet_status{0};

	if (_protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status) != PX4_OK) {
		return PX4_ERROR;
	}

	const uint8_t current_mask = static_cast<uint8_t>(fet_status & 0x0F);

	if (current_mask == desired_mask) {
		return PX4_OK;
	}

	if (!all_ok) {
		int ret = _protocol.subcommand(BQ769X2_SUBCMD_DSG_PDSG_OFF);
		ret |= _protocol.subcommand(BQ769X2_SUBCMD_CHG_PCHG_OFF);
		ret |= _protocol.subcommand(BQ769X2_SUBCMD_ALL_FETS_OFF);
		return ret == PX4_OK ? PX4_OK : PX4_ERROR;
	}

	int ret = PX4_OK;

	const uint8_t stage_mask = prechargeStageMask(desired_mask);

	if (stage_mask != desired_mask && current_mask == 0) {
		ret |= _protocol.subcommandWriteU1(BQ769X2_SUBCMD_FET_CONTROL, stage_mask);
		ret |= _protocol.subcommand(BQ769X2_SUBCMD_ALL_FETS_ON);

		if (ret != PX4_OK) {
			return PX4_ERROR;
		}

		ret = waitForPrechargeEqualization();

		if (ret != PX4_OK) {
			PX4_ERR("precharge equalization timeout");
			return PX4_ERROR;
		}

		const uint8_t overlap_mask = static_cast<uint8_t>(desired_mask | (stage_mask & (BQ769X2_FET_PCHG | BQ769X2_FET_PDSG)));

		if (overlap_mask != desired_mask && _postcharge_ms > 0) {
			ret = _protocol.subcommandWriteU1(BQ769X2_SUBCMD_FET_CONTROL, overlap_mask);
			ret |= _protocol.subcommand(BQ769X2_SUBCMD_ALL_FETS_ON);

			if (ret != PX4_OK) {
				return PX4_ERROR;
			}

			px4_usleep(static_cast<uint32_t>(_postcharge_ms) * 1000);
		}
	}

	ret |= _protocol.subcommandWriteU1(BQ769X2_SUBCMD_FET_CONTROL, desired_mask);
	ret |= _protocol.subcommand(BQ769X2_SUBCMD_ALL_FETS_ON);
	return ret == PX4_OK ? PX4_OK : PX4_ERROR;
}

int BQ769x2::waitForPrechargeEqualization()
{
	const hrt_abstime start = hrt_absolute_time();
	const uint32_t timeout_us = static_cast<uint32_t>(_precharge_timeout_ms) * 1000u;
	const uint32_t min_precharge_us = static_cast<uint32_t>(_precharge_ms) * 1000u;
	const float delta_threshold_v = _precharge_delta_mv * 1e-3f;

	while (hrt_elapsed_time(&start) < timeout_us) {
		int16_t raw_pack_mv{0};
		int16_t raw_stack_mv{0};

		if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_PACK, raw_pack_mv) != PX4_OK
		    || _protocol.directReadI2(BQ769X2_CMD_VOLTAGE_STACK, raw_stack_mv) != PX4_OK) {
			return PX4_ERROR;
		}

		const float v_pack = static_cast<float>(raw_pack_mv) * 1e-2f;
		const float v_stack = static_cast<float>(raw_stack_mv) * 1e-2f;

		const bool min_time_elapsed = hrt_elapsed_time(&start) >= min_precharge_us;

		if (min_time_elapsed && fabsf(v_stack - v_pack) <= delta_threshold_v) {
			return PX4_OK;
		}

		px4_usleep(10'000);
	}

	return PX4_ERROR;
}

uint8_t BQ769x2::prechargeStageMask(uint8_t desired_mask) const
{
	uint8_t stage = desired_mask;

	if ((desired_mask & BQ769X2_FET_DSG) && (_precharge_mask & BQ769X2_FET_PDSG)) {
		stage = static_cast<uint8_t>((stage & ~BQ769X2_FET_DSG) | BQ769X2_FET_PDSG);
	}

	if ((desired_mask & BQ769X2_FET_CHG) && (_precharge_mask & BQ769X2_FET_PCHG)) {
		stage = static_cast<uint8_t>((stage & ~BQ769X2_FET_CHG) | BQ769X2_FET_PCHG);
	}

	return static_cast<uint8_t>(stage &
				     (BQ769X2_FET_CHG | BQ769X2_FET_PCHG | BQ769X2_FET_DSG | BQ769X2_FET_PDSG));
}

uint8_t BQ769x2::requestedFetMask() const
{
	return static_cast<uint8_t>(_fets_on_mask &
				     (BQ769X2_FET_CHG | BQ769X2_FET_PCHG | BQ769X2_FET_DSG | BQ769X2_FET_PDSG));
}

uint16_t BQ769x2::mapFaults(uint8_t safety_a, uint8_t safety_b, uint8_t safety_c) const
{
	uint16_t faults{0};

	if (safety_a & (BQ769X2_SAFETY_A_CUV | BQ769X2_SAFETY_A_COV)) {
		faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_CELL_FAIL);
	}

	if (safety_a & (BQ769X2_SAFETY_A_OCC | BQ769X2_SAFETY_A_OCD1 | BQ769X2_SAFETY_A_OCD2 | BQ769X2_SAFETY_A_SCD)) {
		faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_OVER_CURRENT);
	}

	if (safety_b & (BQ769X2_SAFETY_B_OTC | BQ769X2_SAFETY_B_OTD | BQ769X2_SAFETY_B_OTINT | BQ769X2_SAFETY_B_OTF)) {
		faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_OVER_TEMPERATURE);
	}

	if (safety_b & (BQ769X2_SAFETY_B_UTC | BQ769X2_SAFETY_B_UTD | BQ769X2_SAFETY_B_UTINT)) {
		faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_UNDER_TEMPERATURE);
	}

	if (safety_c & (BQ769X2_SAFETY_C_HWDF | BQ769X2_SAFETY_C_PTO | BQ769X2_SAFETY_C_COVL
			| BQ769X2_SAFETY_C_OCDL | BQ769X2_SAFETY_C_SCDL | BQ769X2_SAFETY_C_OCD3)) {
		faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_HARDWARE_FAILURE);
	}

	return faults;
}

uint8_t BQ769x2::computeWarning(float remaining, uint16_t faults) const
{
	if (faults != 0) {
		return battery_status_s::WARNING_FAILED;
	}

	if (!PX4_ISFINITE(remaining) || remaining < 0.f) {
		return battery_status_s::WARNING_NONE;
	}

	if (remaining > _bat_low_thr) {
		return battery_status_s::WARNING_NONE;
	}

	if (remaining > _bat_crit_thr) {
		return battery_status_s::WARNING_LOW;
	}

	if (remaining > _bat_emerg_thr) {
		return battery_status_s::WARNING_CRITICAL;
	}

	return battery_status_s::WARNING_EMERGENCY;
}

float BQ769x2::estimateRemaining(float avg_cell_voltage) const
{
	if (_cell_voltage_charged <= _cell_voltage_empty + 0.01f) {
		return -1.f;
	}

	return math::constrain((avg_cell_voltage - _cell_voltage_empty) / (_cell_voltage_charged - _cell_voltage_empty), 0.f, 1.f);
}

void BQ769x2::print_status()
{
	I2CSPIDriverBase::print_status();
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_collection_errors);
	PX4_INFO("connected: %s", _connected ? "yes" : "no");
	PX4_INFO("fets auto: %s, all_ok_gate: %s, on_mask: 0x%02x", _fets_auto ? "yes" : "no",
		 _fets_all_ok_gate ? "on" : "off", requestedFetMask());
	PX4_INFO("precharge: min %u ms + %u ms overlap, mask: 0x%02x", _precharge_ms, _postcharge_ms, _precharge_mask);
	PX4_INFO("precharge equalization: timeout %u ms, delta %.1f mV", _precharge_timeout_ms,
		 (double)_precharge_delta_mv);
	PX4_INFO("openwire check: %s, hw_period: %u s, tolerance: %.1f mV/cell", _openwire_check ? "on" : "off",
		 _openwire_check_time_s, (double)_openwire_tol_mv_per_cell);
	PX4_INFO("current prot: OCC %.1fA/%.1fms, OCD %.1fA/%.1fms, SCD %.1fA/%.1fus",
		 (double)_occ_limit_a, (double)_occ_delay_ms, (double)_ocd_limit_a, (double)_ocd_delay_ms,
		 (double)_scd_limit_a, (double)_scd_delay_us);
	PX4_INFO("temp prot: %s, OTC %.1fC, UTC %.1fC, OTD %.1fC, UTD %.1fC, hyst %.1fC",
		 _temp_prot_enable ? "on" : "off", (double)_chg_ot_limit_c, (double)_chg_ut_limit_c,
		 (double)_dis_ot_limit_c, (double)_dis_ut_limit_c, (double)_temp_hyst_c);
	PX4_INFO("capacity: %.0f mAh", (double)_capacity_mah);
	PX4_INFO("consumed: %.1f mAh, %.3f Wh", (double)_discharged_mah, (double)_discharged_wh);
	PX4_INFO("battery id: %u, cells: %u, vcell_mode: 0x%04x", _battery_id, __builtin_popcount(activeVcellModeMask()), activeVcellModeMask());
	PX4_INFO("device: %u, fw: 0x%08lx, hw: 0x%08lx", _device_number,
		 static_cast<unsigned long>(_fw_version), static_cast<unsigned long>(_hw_version));
}

uint16_t BQ769x2::activeVcellModeMask() const
{
	if (_vcell_mode_mask != 0) {
		return _vcell_mode_mask;
	}

	if (_cell_count >= 16) {
		return 0xFFFF;
	}

	return static_cast<uint16_t>((1u << _cell_count) - 1u);
}
