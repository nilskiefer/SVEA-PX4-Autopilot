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
	_param_openwire_check = param_find("BQ769X2_OW_CHK");
	_param_openwire_check_time_s = param_find("BQ769X2_OW_TIME");
	_param_openwire_tol_mv_per_cell = param_find("BQ769X2_OWTOL");
	_param_vcell_mode = param_find("BQ769X2_VCMODE");
	_param_pdsg_timeout_ms = param_find("BQ769X2_PDSG_TO");
	_param_pdsg_stop_dv_mv = param_find("BQ769X2_PDSG_DV");
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

	if (_configure_on_startup) {
		if (configMatchesDesired()) {
			PX4_INFO("config already matches target, skip cfgupdate");

		} else if (configure() != PX4_OK) {
			PX4_ERR("config failed");
			return PX4_ERROR;
		}
	}

	if (ensureFetEnable() != PX4_OK) {
		PX4_ERR("FET_ENABLE failed");
		return PX4_ERROR;
	}

	// libresolar pattern: turn CHG + DSG on once at startup. Chip protections + auto-PDSG
	// handle precharge sequencing and fault response autonomously from here on.
	// Firmware never drives PCHG/PDSG - all bus precharge is hardware-owned.
	if (enableMainFets() != PX4_OK) {
		PX4_ERR("initial enableMainFets failed");
		return PX4_ERROR;
	}
	_switches_initialized = true;

	// Immediate post-init snapshot for cases where power/FETs oscillate before first scheduled sample.
	uint8_t fet_status{0}, safety_a{0}, safety_b{0}, safety_c{0};
	uint8_t pf_a{0}, pf_b{0}, pf_c{0}, pf_d{0};
	uint16_t mfg_status{0};
	uint16_t battery_status{0};
	bool battery_status_valid{false};
	int16_t raw_pack_mv{0}, raw_stack_mv{0}, raw_current_cc2{0};
	float vpack = NAN, vstack = NAN, current_a = NAN;
	bool pf_valid = false;
	bool mfg_valid = false;

	if (_protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_A, safety_a) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_B, safety_b) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_C, safety_c) == PX4_OK) {

		if (_protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, battery_status) == PX4_OK) {
			battery_status_valid = true;
		}

		if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_PACK, raw_pack_mv) == PX4_OK) {
			vpack = static_cast<float>(raw_pack_mv) * 1e-2f;
		}

		if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_STACK, raw_stack_mv) == PX4_OK) {
			vstack = static_cast<float>(raw_stack_mv) * 1e-2f;
		}

		if (_protocol.directReadI2(BQ769X2_CMD_CURRENT_CC2, raw_current_cc2) == PX4_OK) {
			current_a = static_cast<float>(raw_current_cc2) * 1e-2f;
		}

		pf_valid = (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_A, pf_a) == PX4_OK)
			   && (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_B, pf_b) == PX4_OK)
			   && (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_C, pf_c) == PX4_OK)
			   && (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_D, pf_d) == PX4_OK);

			mfg_valid = (_protocol.subcommandReadU2(BQ769X2_SUBCMD_MFG_STATUS, mfg_status) == PX4_OK);

			PX4_INFO("init snapshot: FET_STATUS=0x%02x (mask=0x%02x) SAFETY_A/B/C=0x%02x/0x%02x/0x%02x PF_A/B/C/D=%s0x%02x/0x%02x/0x%02x/0x%02x BAT_STATUS=%s0x%04x MFG_STATUS=%s0x%04x Vstack=%.3fV Vpack=%.3fV I=%.2fA",
				 fet_status, static_cast<unsigned>(fet_status & 0x0F),
				 safety_a, safety_b, safety_c,
				 pf_valid ? "" : "n/a:", pf_a, pf_b, pf_c, pf_d,
				 battery_status_valid ? "" : "n/a:", static_cast<unsigned>(battery_status),
				 mfg_valid ? "" : "n/a:", static_cast<unsigned>(mfg_status),
				 (double)vstack, (double)vpack, (double)current_a);
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

bool BQ769x2::configMatchesDesired()
{
	const uint16_t vcell_mode = activeVcellModeMask();
	const uint8_t pdsg_timeout_raw = static_cast<uint8_t>(math::constrain(lroundf(_pdsg_timeout_ms * 0.1f), 0l, 255l));
	const uint8_t pdsg_stop_dv_raw = static_cast<uint8_t>(math::constrain(lroundf(_pdsg_stop_dv_mv * 0.1f), 0l, 255l));

	uint16_t conf_power{0};
	uint8_t conf_alert{0};
	uint8_t conf_da{0};
	uint16_t vcell_mode_read{0};
	uint16_t body_diode_th_ma{0};
	uint8_t fet_options{0};
	uint8_t pdsg_timeout{0};
	uint8_t pdsg_stop_dv{0};
	uint8_t prot_enabled_a{0};
	uint8_t prot_enabled_b{0};
	uint8_t sf_alert_mask_a{0};
	uint8_t sf_alert_mask_b{0};
	uint8_t pf_alert_mask_a{0};
	uint8_t pf_alert_mask_b{0};
	uint8_t pf_alert_mask_c{0};
	uint8_t pf_alert_mask_d{0};
	uint16_t alarm_default_mask{0};

	if (_protocol.datamemReadU2(BQ769X2_SET_CONF_POWER, conf_power) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_CONF_ALERT, conf_alert) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_CONF_DA, conf_da) != PX4_OK
	    || _protocol.datamemReadU2(BQ769X2_SET_CONF_VCELL_MODE, vcell_mode_read) != PX4_OK
	    || _protocol.datamemReadU2(BQ769X2_SET_PROT_BODY_DIODE_TH, body_diode_th_ma) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_FET_OPTIONS, fet_options) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_FET_PDSG_TIMEOUT, pdsg_timeout) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_FET_PDSG_STOP_DV, pdsg_stop_dv) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_PROT_ENABLED_A, prot_enabled_a) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_PROT_ENABLED_B, prot_enabled_b) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_ALARM_SF_ALERT_MASK_A, sf_alert_mask_a) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_ALARM_SF_ALERT_MASK_B, sf_alert_mask_b) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_A, pf_alert_mask_a) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_B, pf_alert_mask_b) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_C, pf_alert_mask_c) != PX4_OK
	    || _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_D, pf_alert_mask_d) != PX4_OK
	    || _protocol.datamemReadU2(BQ769X2_SET_ALARM_DEFAULT_MASK, alarm_default_mask) != PX4_OK) {
		return false;
	}

	uint8_t expected_prot_enabled_a = static_cast<uint8_t>(BQ769X2_PROT_EN_A_CUV | BQ769X2_PROT_EN_A_COV
					       | BQ769X2_PROT_EN_A_OCC | BQ769X2_PROT_EN_A_OCD1
					       | BQ769X2_PROT_EN_A_SCD);
	const uint8_t temp_bits = static_cast<uint8_t>(BQ769X2_PROT_EN_B_UTC | BQ769X2_PROT_EN_B_UTD
				| BQ769X2_PROT_EN_B_OTC | BQ769X2_PROT_EN_B_OTD);
	uint8_t expected_prot_enabled_b = _temp_prot_enable ? temp_bits : 0;

	const bool matches = (conf_power == 0x2882)
			     && (conf_alert == 0x00)
			     && (conf_da == 0x06)
			     && (vcell_mode_read == vcell_mode)
			     && (body_diode_th_ma == 500u)
			     && (fet_options == 0x1D)
			     && (pdsg_timeout == pdsg_timeout_raw)
			     && (pdsg_stop_dv == pdsg_stop_dv_raw)
			     && ((prot_enabled_a & expected_prot_enabled_a) == expected_prot_enabled_a)
			     && ((prot_enabled_b & temp_bits) == expected_prot_enabled_b)
			     && (sf_alert_mask_a == prot_enabled_a)
			     && (sf_alert_mask_b == prot_enabled_b)
			     && (pf_alert_mask_a == 0xFF)
			     && (pf_alert_mask_b == 0xFF)
			     && (pf_alert_mask_c == 0xFF)
			     && (pf_alert_mask_d == 0xFF)
			     && ((alarm_default_mask & 0x1000u) != 0);

	return matches;
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

	// Disable sleep mode so chip doesn't autonomously drop CHG (libresolar default 0x2882).
	ret |= _protocol.datamemWriteU2(BQ769X2_SET_CONF_POWER, 0x2882);

	// Keep ALERT pin in its dedicated alarm function mode.
	// (Libresolar default pin config writes ALERT=0x00.)
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_CONF_ALERT, 0x00);

	ret |= _protocol.datamemWriteU2(BQ769X2_SET_CONF_VCELL_MODE, activeVcellModeMask());

	// Body-diode threshold for ideal-diode control (libresolar default 500 mA).
	ret |= _protocol.datamemWriteU2(BQ769X2_SET_PROT_BODY_DIODE_TH, 500);

	// Auto-PDSG / hardware precharge of the bus through the predischarge resistor.
	// FET_OPTIONS = 0x1D enables the chip's automatic pre-discharge before DSG turn-on.
	// PDSG timeout and stop-delta are parameterized in user units (ms/mV), converted
	// to the chip's 10 ms / 10 mV register encoding.
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_FET_OPTIONS, 0x1D);
	const uint8_t pdsg_timeout_raw = static_cast<uint8_t>(math::constrain(lroundf(_pdsg_timeout_ms * 0.1f), 0l, 255l));
	const uint8_t pdsg_stop_dv_raw = static_cast<uint8_t>(math::constrain(lroundf(_pdsg_stop_dv_mv * 0.1f), 0l, 255l));
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_FET_PDSG_TIMEOUT, pdsg_timeout_raw);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_FET_PDSG_STOP_DV, pdsg_stop_dv_raw);

	ret |= _protocol.datamemWriteU1(BQ769X2_SET_OPEN_WIRE_CHECK_TIME, _openwire_check_time_s);

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

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_COV_THRESHOLD, static_cast<uint8_t>(cov_threshold_raw));
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_COV_RECOV_HYST, static_cast<uint8_t>(cov_hyst_raw));
	ret |= _protocol.datamemWriteU2(BQ769X2_PROT_COV_DELAY, static_cast<uint16_t>(cov_delay_raw));

	// CUV: threshold/hysteresis/delay.
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

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_CUV_THRESHOLD, static_cast<uint8_t>(cuv_threshold_raw));
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_CUV_RECOV_HYST, static_cast<uint8_t>(cuv_hyst_raw));
	ret |= _protocol.datamemWriteU2(BQ769X2_PROT_CUV_DELAY, static_cast<uint16_t>(cuv_delay_raw));

	// OCC/OCD thresholds and delays (2 mV units across shunt, 3.3 ms base + 6.6 ms offset).
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

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCC_THRESHOLD, static_cast<uint8_t>(occ_threshold_raw));
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCC_DELAY, static_cast<uint8_t>(occ_delay_raw));

	const long ocd_threshold_raw = lroundf(_ocd_limit_a * _shunt_uohm / 2000.f);
	const long ocd_delay_raw = lroundf((_ocd_delay_ms - 6.6f) / 3.3f);

	if (ocd_threshold_raw < 2 || ocd_threshold_raw > 100 || ocd_delay_raw < 1 || ocd_delay_raw > 127) {
		PX4_ERR("OCD config not representable for shunt %.1f uohm", (double)_shunt_uohm);
		return PX4_ERROR;
	}

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCD1_THRESHOLD, static_cast<uint8_t>(ocd_threshold_raw));
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_OCD1_DELAY, static_cast<uint8_t>(ocd_delay_raw));

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

	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_SCD_THRESHOLD, scd_threshold_idx);
	ret |= _protocol.datamemWriteU1(BQ769X2_PROT_SCD_DELAY, static_cast<uint8_t>(scd_delay_raw));

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

	// Ensure the cell/current protections are enabled in PROT_ENABLED_A. The chip
	// drives the FETs off autonomously when any enabled protection trips.
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

	// Route configured safety faults to ALERT pin (open-drain) so external LED/logic
	// sees hardware-level protection events without relying on host polling.
	// Mirrors libresolar approach: SF mask + ALARM_DEFAULT_MASK bit 12.
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_ALARM_SF_ALERT_MASK_A, prot_enabled_a);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_ALARM_SF_ALERT_MASK_B, prot_enabled_b);
	// PF events are also routed to ALERT so hardware fault latches are externally visible.
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_A, 0xFF);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_B, 0xFF);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_C, 0xFF);
	ret |= _protocol.datamemWriteU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_D, 0xFF);
	ret |= _protocol.datamemWriteU2(BQ769X2_SET_ALARM_DEFAULT_MASK, 0x1000);

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

	if (_param_pdsg_timeout_ms != PARAM_INVALID && param_get(_param_pdsg_timeout_ms, &val_i32) == PX4_OK) {
		_pdsg_timeout_ms = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(10), static_cast<int32_t>(2550)));
	}

	if (_param_pdsg_stop_dv_mv != PARAM_INVALID && param_get(_param_pdsg_stop_dv_mv, &val_i32) == PX4_OK) {
		_pdsg_stop_dv_mv = static_cast<uint16_t>(math::constrain(val_i32, static_cast<int32_t>(0), static_cast<int32_t>(2550)));
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
	uint8_t fet_status{0};
	uint16_t battery_status{0};
	bool battery_status_valid{false};

	if (_protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_A, safety_a) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_B, safety_b) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_C, safety_c) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status) != PX4_OK) {
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return PX4_ERROR;
	}

	report.faults = mapFaults(safety_a, safety_b, safety_c);

	if (open_wire_detected) {
		report.faults |= static_cast<uint16_t>(1u << battery_status_s::FAULT_CELL_FAIL);
	}

	report.warning = computeWarning(report.remaining, report.faults);

	// Telemetry only: log if chip raised a new safety bit (so we know why FETs went off).
	if (_protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, battery_status) == PX4_OK) {
		battery_status_valid = true;
	}

	logFaultChange(safety_a, safety_b, safety_c, fet_status,
		       battery_status, battery_status_valid,
		       stack_voltage_v, report.voltage_v, report.current_a);

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
		// FET_ENABLE subcommand sets FET_EN bit in MFG_STATUS (mirrors libresolar init).
		if (_protocol.subcommand(BQ769X2_SUBCMD_FET_ENABLE) != PX4_OK) {
			return PX4_ERROR;
		}

		// FET_ENABLE can take a short time to latch. Verify explicitly so later
		// FET_CONTROL writes don't silently no-op.
		bool fet_en_latched = false;

		for (int attempt = 0; attempt < 20; attempt++) {
			if (_protocol.subcommandReadU2(BQ769X2_SUBCMD_MFG_STATUS, mfg_status) == PX4_OK
			    && ((mfg_status & BQ769X2_MFG_STATUS_FET_EN_MASK) != 0)) {
				fet_en_latched = true;
				break;
			}

			px4_usleep(2'000);
		}

		if (!fet_en_latched) {
			uint8_t fet_status{0}, safety_a{0}, safety_b{0}, safety_c{0};
			uint16_t bat_status{0};
			(void)_protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status);
			(void)_protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_A, safety_a);
			(void)_protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_B, safety_b);
			(void)_protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_C, safety_c);
			(void)_protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, bat_status);

			PX4_ERR("FET_ENABLE did not latch: MFG_STATUS=0x%04x FET_STATUS=0x%02x BAT_STATUS=0x%04x SAFETY_A/B/C=0x%02x/0x%02x/0x%02x",
				static_cast<unsigned>(mfg_status),
				fet_status,
				static_cast<unsigned>(bat_status),
				safety_a, safety_b, safety_c);
			return PX4_ERROR;
		}
	}

	_fets_initialized = true;
	return PX4_OK;
}

int BQ769x2::enableMainFets()
{
	// Hardware-only FET strategy:
	//   - Bus precharge is done in chip HW via auto-PDSG (FET_OPTIONS=0x1D,
	//     see configure()). The chip gates DSG based on parameterized
	//     PDSG timeout / PDSG stop-delta conditions.
	//   - Firmware never drives PCHG or PDSG directly. This function only
	//     enables the main CHG + DSG FETs once at init. Chip protections
	//     (PROT_ENABLED_A/B) drop the FETs autonomously on faults.
	//
	// Mirror of libresolar's bms_ic_bq769x2_set_switches, narrowed to CHG+DSG:
	//   1) Read current FET_STATUS, mask to lower nibble.
	//   2) OR in the main-FET bits.
	//   3) Write merged bits via FET_CONTROL subcommand.
	//   4) Send ALL_FETS_ON to actually engage the requested combination.
	uint8_t fet_status_before{0};

	if (_protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status_before) != PX4_OK) {
		return PX4_ERROR;
	}

	const uint8_t before_mask = static_cast<uint8_t>(fet_status_before & 0x0F);
	const uint8_t target = static_cast<uint8_t>(before_mask | DEFAULT_FET_MASK);

	// Do not poke FET control on MCU reset when CHG+DSG are already on.
	// This avoids re-triggering chip-side auto-PDSG sequencing just because
	// the host rebooted and re-ran init.
	if ((before_mask & DEFAULT_FET_MASK) == DEFAULT_FET_MASK) {
		PX4_INFO("enableMainFets: skip (already on, FET_STATUS=0x%02x)", fet_status_before);
		return PX4_OK;
	}

	const int ret_fet_control = _protocol.subcommandWriteU1(BQ769X2_SUBCMD_FET_CONTROL, target);
	const int ret_all_fets_on = _protocol.subcommand(BQ769X2_SUBCMD_ALL_FETS_ON);
	const int ret = ret_fet_control | ret_all_fets_on;

	if (ret != PX4_OK) {
		PX4_ERR("enableMainFets: command error (control=%d all_on=%d)", ret_fet_control, ret_all_fets_on);
		return PX4_ERROR;
	}

	// Libresolar-style behavior: command requested switch state once and let the chip run
	// autonomous auto-PDSG/DSG sequencing. Do not hard-fail on intermediate states (e.g. 0x09),
	// only hard-fail if the chip is already reporting active safety/PF faults.
	uint8_t fet_status_after{0};
	uint8_t safety_a{0}, safety_b{0}, safety_c{0};
	uint8_t pf_a{0}, pf_b{0}, pf_c{0}, pf_d{0};
	uint16_t bat_status{0};
	uint16_t mfg_status{0};

	if (_protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status_after) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_A, safety_a) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_B, safety_b) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_C, safety_c) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_A, pf_a) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_B, pf_b) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_C, pf_c) != PX4_OK
	    || _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_D, pf_d) != PX4_OK
	    || _protocol.directReadU2(BQ769X2_CMD_BATTERY_STATUS, bat_status) != PX4_OK
	    || _protocol.subcommandReadU2(BQ769X2_SUBCMD_MFG_STATUS, mfg_status) != PX4_OK) {
		PX4_ERR("enableMainFets: readback failed (pre=0x%02x target=0x%02x control=%d all_on=%d)",
			fet_status_before, target, ret_fet_control, ret_all_fets_on);
		return PX4_ERROR;
	}

	const uint8_t after_mask = static_cast<uint8_t>(fet_status_after & 0x0F);
	const bool has_safety = (safety_a | safety_b | safety_c) != 0;
	const bool has_pf = (pf_a | pf_b | pf_c | pf_d) != 0;

	if (has_safety || has_pf) {
		PX4_ERR("enableMainFets blocked by fault: pre=0x%02x target=0x%02x after=0x%02x SAFETY_A/B/C=0x%02x/0x%02x/0x%02x PF_A/B/C/D=0x%02x/0x%02x/0x%02x/0x%02x BAT_STATUS=0x%04x MFG_STATUS=0x%04x",
			fet_status_before, target, after_mask,
			safety_a, safety_b, safety_c,
			pf_a, pf_b, pf_c, pf_d,
			static_cast<unsigned>(bat_status),
			static_cast<unsigned>(mfg_status));
		return PX4_ERROR;
	}

	if ((after_mask & DEFAULT_FET_MASK) != DEFAULT_FET_MASK) {
		PX4_WARN("enableMainFets pending (chip auto sequence): pre=0x%02x target=0x%02x after=0x%02x BAT_STATUS=0x%04x MFG_STATUS=0x%04x",
			 fet_status_before, target, after_mask,
			 static_cast<unsigned>(bat_status),
			 static_cast<unsigned>(mfg_status));

	} else {
		PX4_INFO("enableMainFets: pre=0x%02x target=0x%02x after=0x%02x BAT_STATUS=0x%04x MFG_STATUS=0x%04x",
			 fet_status_before, target, after_mask,
			 static_cast<unsigned>(bat_status),
			 static_cast<unsigned>(mfg_status));
	}

	return PX4_OK;
}

void BQ769x2::logFaultChange(uint8_t safety_a, uint8_t safety_b, uint8_t safety_c, uint8_t fet_status,
			     uint16_t battery_status, bool battery_status_valid,
			     float stack_voltage_v, float pack_voltage_v, float current_a)
{
	const uint8_t new_a = static_cast<uint8_t>(safety_a & ~_last_safety_a);
	const uint8_t new_b = static_cast<uint8_t>(safety_b & ~_last_safety_b);
	const uint8_t new_c = static_cast<uint8_t>(safety_c & ~_last_safety_c);

	if (new_a || new_b || new_c) {
		const uint8_t fet_lo = static_cast<uint8_t>(fet_status & 0x0F);

		PX4_WARN("safety bits raised: A=0x%02x B=0x%02x C=0x%02x; FET_STATUS=0x%02x",
			 new_a, new_b, new_c, fet_status);

		// Decode the most relevant bits so the operator immediately sees the cause.
		if (new_a & BQ769X2_SAFETY_A_CUV)  { PX4_WARN(" -> cell undervoltage (CUV)"); }
		if (new_a & BQ769X2_SAFETY_A_COV)  { PX4_WARN(" -> cell overvoltage (COV)"); }
		if (new_a & BQ769X2_SAFETY_A_OCC)  { PX4_WARN(" -> charge overcurrent (OCC)"); }
		if (new_a & BQ769X2_SAFETY_A_OCD1) { PX4_WARN(" -> discharge overcurrent (OCD1)"); }
		if (new_a & BQ769X2_SAFETY_A_OCD2) { PX4_WARN(" -> discharge overcurrent (OCD2)"); }
		if (new_a & BQ769X2_SAFETY_A_SCD)  { PX4_WARN(" -> short circuit discharge (SCD)"); }

		if (new_b & BQ769X2_SAFETY_B_UTC)   { PX4_WARN(" -> charge undertemperature (UTC)"); }
		if (new_b & BQ769X2_SAFETY_B_UTD)   { PX4_WARN(" -> discharge undertemperature (UTD)"); }
		if (new_b & BQ769X2_SAFETY_B_UTINT) { PX4_WARN(" -> internal undertemperature (UTINT)"); }
		if (new_b & BQ769X2_SAFETY_B_OTC)   { PX4_WARN(" -> charge overtemperature (OTC)"); }
		if (new_b & BQ769X2_SAFETY_B_OTD)   { PX4_WARN(" -> discharge overtemperature (OTD)"); }
		if (new_b & BQ769X2_SAFETY_B_OTINT) { PX4_WARN(" -> internal overtemperature (OTINT)"); }
		if (new_b & BQ769X2_SAFETY_B_OTF)   { PX4_WARN(" -> FET overtemperature (OTF)"); }

		if (new_c & BQ769X2_SAFETY_C_HWDF) { PX4_WARN(" -> host watchdog fault (HWDF)"); }
		if (new_c & BQ769X2_SAFETY_C_PTO)  { PX4_WARN(" -> precharge timeout (PTO)"); }
		if (new_c & BQ769X2_SAFETY_C_COVL) { PX4_WARN(" -> cell overvoltage latch (COVL)"); }
		if (new_c & BQ769X2_SAFETY_C_OCDL) { PX4_WARN(" -> overcurrent discharge latch (OCDL)"); }
		if (new_c & BQ769X2_SAFETY_C_SCDL) { PX4_WARN(" -> short-circuit discharge latch (SCDL)"); }
		if (new_c & BQ769X2_SAFETY_C_OCD3) { PX4_WARN(" -> overcurrent discharge tier 3 (OCD3)"); }

		if ((fet_lo & DEFAULT_FET_MASK) != DEFAULT_FET_MASK) {
			PX4_WARN(" -> chip dropped FETs (FET_STATUS lower nibble = 0x%02x, expected 0x%02x)",
				 fet_lo, DEFAULT_FET_MASK);
		}
	}

	// Cleared bits also worth reporting (chip recovered).
	const uint8_t cleared_a = static_cast<uint8_t>(_last_safety_a & ~safety_a);
	const uint8_t cleared_b = static_cast<uint8_t>(_last_safety_b & ~safety_b);
	const uint8_t cleared_c = static_cast<uint8_t>(_last_safety_c & ~safety_c);

	if (cleared_a || cleared_b || cleared_c) {
		PX4_INFO("safety bits cleared: A=0x%02x B=0x%02x C=0x%02x; FET_STATUS=0x%02x",
			 cleared_a, cleared_b, cleared_c, fet_status);
	}

	const uint8_t fet_lo = static_cast<uint8_t>(fet_status & 0x0F);
	const bool fet_changed = _last_fet_status_valid && (fet_lo != static_cast<uint8_t>(_last_fet_status & 0x0F));

	if (fet_changed) {
		const hrt_abstime now = hrt_absolute_time();
		const float dt_ms = (_last_fet_transition_time > 0) ? (now - _last_fet_transition_time) * 1e-3f : NAN;
		const uint8_t prev_fet_lo = static_cast<uint8_t>(_last_fet_status & 0x0F);
		const float bus_delta = (PX4_ISFINITE(stack_voltage_v) && PX4_ISFINITE(pack_voltage_v))
					? fabsf(stack_voltage_v - pack_voltage_v) : NAN;
		uint8_t pf_a{0}, pf_b{0}, pf_c{0}, pf_d{0};
		const bool pf_valid = (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_A, pf_a) == PX4_OK)
				      && (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_B, pf_b) == PX4_OK)
				      && (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_C, pf_c) == PX4_OK)
				      && (_protocol.directReadU1(BQ769X2_CMD_PF_STATUS_D, pf_d) == PX4_OK);
		uint16_t mfg_status{0};
		const bool mfg_valid = (_protocol.subcommandReadU2(BQ769X2_SUBCMD_MFG_STATUS, mfg_status) == PX4_OK);

		PX4_WARN("FET transition: 0x%02x -> 0x%02x in %.1f ms | SAFETY_A/B/C=0x%02x/0x%02x/0x%02x | PF_A/B/C/D=%s0x%02x/0x%02x/0x%02x/0x%02x | BAT_STATUS=%s0x%04x | MFG_STATUS=%s0x%04x | Vstack=%.3fV Vpack=%.3fV dV=%.3fV I=%.2fA",
			 prev_fet_lo, fet_lo, (double)dt_ms,
			 safety_a, safety_b, safety_c,
			 pf_valid ? "" : "n/a:",
			 pf_a, pf_b, pf_c, pf_d,
			 battery_status_valid ? "" : "n/a:",
			 static_cast<unsigned>(battery_status),
			 mfg_valid ? "" : "n/a:",
			 static_cast<unsigned>(mfg_status),
			 (double)stack_voltage_v, (double)pack_voltage_v, (double)bus_delta, (double)current_a);

		if ((fet_lo & DEFAULT_FET_MASK) != DEFAULT_FET_MASK) {
			PX4_WARN(" -> main FETs no longer fully on (mask=0x%02x, expected 0x%02x)",
				 fet_lo, DEFAULT_FET_MASK);
		}

		if (!new_a && !new_b && !new_c && (safety_a == 0) && (safety_b == 0) && (safety_c == 0)) {
			PX4_WARN(" -> transition occurred with no SAFETY bits set (possible autonomous precharge/FET policy or external command)");
		}

		_last_fet_transition_time = now;
	}

	_last_safety_a = safety_a;
	_last_safety_b = safety_b;
	_last_safety_c = safety_c;
	_last_fet_status = fet_status;

	if (!_last_fet_status_valid) {
		_last_fet_status_valid = true;
		_last_fet_transition_time = hrt_absolute_time();
	}
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
	PX4_INFO("connected: %s, switches initialized: %s", _connected ? "yes" : "no",
		 _switches_initialized ? "yes" : "no");
	PX4_INFO("FET policy: chip auto (HW auto-PDSG=on, FET_OPTIONS=0x1D, PDSG_TIMEOUT=%u ms, PDSG_STOP_DV=%u mV)",
		 static_cast<unsigned>(_pdsg_timeout_ms), static_cast<unsigned>(_pdsg_stop_dv_mv));

	uint8_t pdsg_timeout_raw{0};
	uint8_t pdsg_stop_dv_raw{0};

	if (_protocol.datamemReadU1(BQ769X2_SET_FET_PDSG_TIMEOUT, pdsg_timeout_raw) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_FET_PDSG_STOP_DV, pdsg_stop_dv_raw) == PX4_OK) {
		PX4_INFO("auto-PDSG chip cfg: timeout=%u ms (raw=0x%02x), stop_delta=%u mV (raw=0x%02x)",
			 static_cast<unsigned>(pdsg_timeout_raw) * 10u, pdsg_timeout_raw,
			 static_cast<unsigned>(pdsg_stop_dv_raw) * 10u, pdsg_stop_dv_raw);

	} else {
		PX4_WARN("failed to read PDSG timeout/stop-delta");
	}

	uint8_t alert_pin_cfg{0};
	uint8_t sf_alert_mask_a{0};
	uint8_t sf_alert_mask_b{0};
	uint8_t pf_alert_mask_a{0};
	uint8_t pf_alert_mask_b{0};
	uint8_t pf_alert_mask_c{0};
	uint8_t pf_alert_mask_d{0};
	uint16_t alarm_default_mask{0};

	if (_protocol.datamemReadU1(BQ769X2_SET_CONF_ALERT, alert_pin_cfg) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_ALARM_SF_ALERT_MASK_A, sf_alert_mask_a) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_ALARM_SF_ALERT_MASK_B, sf_alert_mask_b) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_A, pf_alert_mask_a) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_B, pf_alert_mask_b) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_C, pf_alert_mask_c) == PX4_OK
	    && _protocol.datamemReadU1(BQ769X2_SET_ALARM_PF_ALERT_MASK_D, pf_alert_mask_d) == PX4_OK
	    && _protocol.datamemReadU2(BQ769X2_SET_ALARM_DEFAULT_MASK, alarm_default_mask) == PX4_OK) {
		PX4_INFO("alert cfg: CONF_ALERT=0x%02x SF_MASK_A/B=0x%02x/0x%02x PF_MASK_A/B/C/D=0x%02x/0x%02x/0x%02x/0x%02x DEFAULT_MASK=0x%04x (SF routing %s)",
			 alert_pin_cfg, sf_alert_mask_a, sf_alert_mask_b,
			 pf_alert_mask_a, pf_alert_mask_b, pf_alert_mask_c, pf_alert_mask_d,
			 static_cast<unsigned>(alarm_default_mask),
			 (alarm_default_mask & 0x1000u) ? "on" : "off");

	} else {
		PX4_WARN("failed to read ALERT pin/alarm mask config");
	}

	int16_t raw_pack_mv{0};
	int16_t raw_stack_mv{0};

	if (_protocol.directReadI2(BQ769X2_CMD_VOLTAGE_PACK, raw_pack_mv) == PX4_OK
	    && _protocol.directReadI2(BQ769X2_CMD_VOLTAGE_STACK, raw_stack_mv) == PX4_OK) {
		const float v_pack = static_cast<float>(raw_pack_mv) * 1e-2f;
		const float v_stack = static_cast<float>(raw_stack_mv) * 1e-2f;
		const float delta_v = fabsf(v_stack - v_pack);
		const float stop_v = static_cast<float>(pdsg_stop_dv_raw) * 0.01f;
		PX4_INFO("bus equalization: Vstack=%.3fV Vpack=%.3fV delta=%.3fV (auto-PDSG stop %.3fV)",
			 (double)v_stack, (double)v_pack, (double)delta_v, (double)stop_v);

	} else {
		PX4_WARN("failed to read pack/stack voltages");
	}

	PX4_INFO("openwire check: %s, hw_period: %u s, tolerance: %.1f mV/cell", _openwire_check ? "on" : "off",
		 _openwire_check_time_s, (double)_openwire_tol_mv_per_cell);
	PX4_INFO("voltage prot: COV %.3fV/%.3fV reset/%.0fms, CUV %.3fV/%.3fV reset/%.0fms",
		 (double)_cell_ov_limit_v, (double)_cell_ov_reset_v, (double)_cell_ov_delay_ms,
		 (double)_cell_uv_limit_v, (double)_cell_uv_reset_v, (double)_cell_uv_delay_ms);
	PX4_INFO("current prot: OCC %.1fA/%.1fms, OCD %.1fA/%.1fms, SCD %.1fA/%.0fus",
		 (double)_occ_limit_a, (double)_occ_delay_ms, (double)_ocd_limit_a, (double)_ocd_delay_ms,
		 (double)_scd_limit_a, (double)_scd_delay_us);
	PX4_INFO("temp prot: %s, OTC %.1fC, UTC %.1fC, OTD %.1fC, UTD %.1fC, hyst %.1fC",
		 _temp_prot_enable ? "on" : "off", (double)_chg_ot_limit_c, (double)_chg_ut_limit_c,
		 (double)_dis_ot_limit_c, (double)_dis_ut_limit_c, (double)_temp_hyst_c);
	PX4_INFO("capacity: %.0f mAh", (double)_capacity_mah);
	PX4_INFO("consumed: %.1f mAh, %.3f Wh", (double)_discharged_mah, (double)_discharged_wh);
	PX4_INFO("battery id: %u, cells: %u, vcell_mode: 0x%04x", _battery_id, __builtin_popcount(activeVcellModeMask()),
		 activeVcellModeMask());
	PX4_INFO("device: %u, fw: 0x%08lx, hw: 0x%08lx", _device_number,
		 static_cast<unsigned long>(_fw_version), static_cast<unsigned long>(_hw_version));

	uint8_t fet_status{0};
	uint8_t safety_a{0};
	uint8_t safety_b{0};
	uint8_t safety_c{0};
	uint8_t pf_a{0};
	uint8_t pf_b{0};
	uint8_t pf_c{0};
	uint8_t pf_d{0};
	uint16_t mfg_status{0};

	if (_protocol.directReadU1(BQ769X2_CMD_FET_STATUS, fet_status) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_A, safety_a) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_B, safety_b) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_SAFETY_STATUS_C, safety_c) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_A, pf_a) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_B, pf_b) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_C, pf_c) == PX4_OK
	    && _protocol.directReadU1(BQ769X2_CMD_PF_STATUS_D, pf_d) == PX4_OK) {
		const uint8_t current_fet_mask = static_cast<uint8_t>(fet_status & 0x0F);
		(void)_protocol.subcommandReadU2(BQ769X2_SUBCMD_MFG_STATUS, mfg_status);
		PX4_INFO("regs: FET_STATUS=0x%02x (mask=0x%02x: CHG=%u PCHG=%u DSG=%u PDSG=%u), SAFETY_A/B/C=0x%02x/0x%02x/0x%02x, PF_A/B/C/D=0x%02x/0x%02x/0x%02x/0x%02x, MFG_STATUS=0x%04x",
			 fet_status, current_fet_mask,
			 (current_fet_mask & BQ769X2_FET_CHG)  ? 1u : 0u,
			 (current_fet_mask & BQ769X2_FET_PCHG) ? 1u : 0u,
			 (current_fet_mask & BQ769X2_FET_DSG)  ? 1u : 0u,
			 (current_fet_mask & BQ769X2_FET_PDSG) ? 1u : 0u,
			 safety_a, safety_b, safety_c,
			 pf_a, pf_b, pf_c, pf_d,
			 static_cast<unsigned>(mfg_status));

	} else {
		PX4_WARN("regs: failed to read FET/SAFETY/PF status");
	}
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
