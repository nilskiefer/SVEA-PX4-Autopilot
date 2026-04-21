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
 * Enable BQ769x2 battery monitor support in board startup scripts.
 *
 * @group Sensors
 * @boolean
 * @reboot_required true
 */
PARAM_DEFINE_INT32(SENS_EN_BQ769X2, 0);

/**
 * Default I2C address for BQ769x2.
 *
 * @group Sensors
 * @min 1
 * @max 127
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_ADDR, 8);

/**
 * Number of series cells wired to BQ769x2.
 *
 * @group Sensors
 * @min 1
 * @max 14
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_CELLS, 3);

/**
 * Enable BQ769x2 CRC mode for host communication.
 *
 * SVEA powerboard defaults to CRC enabled.
 *
 * @group Sensors
 * @boolean
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_CRC, 1);

/**
 * Apply BQ769x2 configuration writes on startup.
 *
 * When enabled, the driver enters config-update mode and writes data-memory fields.
 * Startup fails if any write/verification fails.
 *
 * @group Sensors
 * @boolean
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_CFG, 1);

/**
 * BQ769x2 shunt resistance in micro-ohms.
 *
 * Used to program CAL_CURR_CC_GAIN in data memory, following TI/LibreSolar convention.
 *
 * @group Sensors
 * @min 1
 * @max 10000
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_SHUNT, 1000.f);

/**
 * Enable autonomous balancing in BQ769x2 (CB_CHG + CB_RLX).
 *
 * @group Sensors
 * @boolean
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_BAL_EN, 1);

/**
 * Minimum cell voltage for balancing (charge and relaxed), in volts.
 *
 * 3S LiPo/NMC default: 3.8V.
 *
 * @group Sensors
 * @min 2.5
 * @max 4.3
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_BAL_VMIN, 3.80f);

/**
 * Minimum/stop delta for balancing, in volts.
 *
 * Applied to both charge and relaxed balancing modes.
 *
 * @group Sensors
 * @min 0.002
 * @max 0.12
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_BAL_DV, 0.01f);

/**
 * Idle current threshold for relaxed balancing, in amps.
 *
 * Written to both DSG_CURR_TH and CHG_CURR_TH.
 *
 * @group Sensors
 * @min 0.0
 * @max 20.0
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_BAL_IIDL, 0.10f);

/**
 * Maximum simultaneously balanced cells.
 *
 * Board thermal limit. For 3S default is 3.
 *
 * @group Sensors
 * @min 1
 * @max 16
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_BAL_MAX, 3);

/**
 * Cell overvoltage threshold (COV) in volts.
 *
 * @group Sensors
 * @min 1.5
 * @max 5.6
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_COV_V, 4.20f);

/**
 * Cell overvoltage recovery threshold in volts.
 *
 * @group Sensors
 * @min 1.5
 * @max 5.6
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_COV_RV, 4.10f);

/**
 * Cell overvoltage detection delay in milliseconds.
 *
 * @group Sensors
 * @min 3.3
 * @max 6755
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_COV_DLY, 1000.f);

/**
 * Cell undervoltage threshold (CUV) in volts.
 *
 * @group Sensors
 * @min 1.5
 * @max 4.6
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_CUV_V, 3.20f);

/**
 * Cell undervoltage recovery threshold in volts.
 *
 * @group Sensors
 * @min 1.5
 * @max 4.6
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_CUV_RV, 3.30f);

/**
 * Cell undervoltage detection delay in milliseconds.
 *
 * @group Sensors
 * @min 3.3
 * @max 6755
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_CUV_DLY, 1000.f);

/**
 * Charge overcurrent threshold in amps (OCC).
 *
 * @group Sensors
 * @min 1
 * @max 500
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCC_A, 10.f);

/**
 * Charge overcurrent delay in milliseconds (OCC).
 *
 * @group Sensors
 * @min 6.6
 * @max 425
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCC_DLY, 200.f);

/**
 * Discharge overcurrent threshold in amps (OCD1).
 *
 * @group Sensors
 * @min 1
 * @max 500
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCD_A, 140.f);

/**
 * Discharge overcurrent delay in milliseconds (OCD1).
 *
 * @group Sensors
 * @min 6.6
 * @max 425
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCD_DLY, 320.f);

/**
 * Short-circuit threshold in amps (SCD).
 *
 * @group Sensors
 * @min 10
 * @max 1000
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_SCD_A, 150.f);

/**
 * Short-circuit delay in microseconds (SCD).
 *
 * @group Sensors
 * @min 0
 * @max 450
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_SCD_DLY, 100.f);

/**
 * Charge over-temperature threshold in degC (OTC).
 *
 * @group Sensors
 * @min -40
 * @max 120
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OTC_C, 45.f);

/**
 * Charge under-temperature threshold in degC (UTC).
 *
 * @group Sensors
 * @min -40
 * @max 120
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_UTC_C, 0.f);

/**
 * Discharge over-temperature threshold in degC (OTD).
 *
 * @group Sensors
 * @min -40
 * @max 120
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OTD_C, 60.f);

/**
 * Discharge under-temperature threshold in degC (UTD).
 *
 * @group Sensors
 * @min -40
 * @max 120
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_UTD_C, -20.f);

/**
 * Temperature recovery hysteresis in degC.
 *
 * Recovery thresholds are OTC/OTD minus hysteresis and UTC/UTD plus hysteresis.
 *
 * @group Sensors
 * @min 1
 * @max 20
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_T_HYST_C, 5.f);

/**
 * Enable external thermistor temperature protections (OTC/OTD/UTC/UTD).
 *
 * Keep disabled when TS thermistors are not connected.
 *
 * @group Sensors
 * @boolean
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_TPROT_EN, 0);

/**
 * Enable open-wire consistency check.
 *
 * Compares measured stack voltage against sum of selected cell voltages.
 * On mismatch, FAULT_CELL_FAIL is raised in telemetry.
 *
 * @group Sensors
 * @boolean
 */
PARAM_DEFINE_INT32(BQ769X2_OW_CHK, 1);

/**
 * Hardware open-wire-check period in seconds.
 *
 * Written to BQ769x2 OPEN_WIRE_CHECK_TIME data memory.
 * Set 0 to disable periodic hardware open-wire check.
 *
 * @group Sensors
 * @min 0
 * @max 255
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_OW_TIME, 10);

/**
 * Open-wire tolerance in mV per detected cell.
 *
 * Deviation beyond (cells * tolerance) is treated as open-wire mismatch.
 *
 * @group Sensors
 * @min 1
 * @max 500
 */
PARAM_DEFINE_FLOAT(BQ769X2_OWTOL, 50.f);

/**
 * BQ769x2 VCELL_MODE bitmask.
 *
 * Bit 0 = VC1 channel, bit 1 = VC2 ... bit 15 = VC16.
 * Set to 0 to use contiguous channels from BQ769X2_CELLS.
 *
 * @group Sensors
 * @min 0
 * @max 65535
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_VCMODE, 0);

/**
 * Auto-PDSG timeout in milliseconds.
 *
 * Written to BQ769x2 FET_PDSG_TIMEOUT (0x930E), encoded in 10 ms steps.
 * Must be >0 so predischarge doesn't hold indefinitely.
 *
 * @group Sensors
 * @min 10
 * @max 2550
 * @reboot_required true
 * @unit ms
 */
PARAM_DEFINE_INT32(BQ769X2_PDSG_TO, 2000);

/**
 * Auto-PDSG stop delta in millivolts.
 *
 * Written to BQ769x2 FET_PDSG_STOP_DV (0x930F), encoded in 10 mV steps.
 * 0 disables voltage-stop condition (timeout-only behavior).
 *
 * @group Sensors
 * @min 0
 * @max 2550
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_PDSG_DV, 500);
