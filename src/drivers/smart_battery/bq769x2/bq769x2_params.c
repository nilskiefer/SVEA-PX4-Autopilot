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
 * Keep disabled unless data memory communication type is configured accordingly.
 *
 * @group Sensors
 * @boolean
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_CRC, 0);

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
 * Cell overvoltage threshold (COV) in volts.
 *
 * @group Sensors
 * @min 1.5
 * @max 5.6
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_COV_V, 4.25f);

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
PARAM_DEFINE_FLOAT(BQ769X2_CUV_V, 2.80f);

/**
 * Cell undervoltage recovery threshold in volts.
 *
 * @group Sensors
 * @min 1.5
 * @max 4.6
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_CUV_RV, 3.00f);

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
PARAM_DEFINE_FLOAT(BQ769X2_OCC_A, 120.f);

/**
 * Charge overcurrent delay in milliseconds (OCC).
 *
 * @group Sensors
 * @min 6.6
 * @max 425
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCC_DLY, 100.f);

/**
 * Discharge overcurrent threshold in amps (OCD1).
 *
 * @group Sensors
 * @min 1
 * @max 500
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCD_A, 130.f);

/**
 * Discharge overcurrent delay in milliseconds (OCD1).
 *
 * @group Sensors
 * @min 6.6
 * @max 425
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_OCD_DLY, 20.f);

/**
 * Short-circuit threshold in amps (SCD).
 *
 * @group Sensors
 * @min 10
 * @max 1000
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_SCD_A, 220.f);

/**
 * Short-circuit delay in microseconds (SCD).
 *
 * @group Sensors
 * @min 0
 * @max 450
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_SCD_DLY, 60.f);

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
 * BQ769x2 CONF_POWER data memory word.
 *
 * Default 0x2882 follows LibreSolar: disables sleep-driven CHG dropouts.
 *
 * @group Sensors
 * @min 0
 * @max 65535
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_PWR_CFG, 10370);

/**
 * Body-diode threshold in mA.
 *
 * @group Sensors
 * @min 0
 * @max 32767
 * @reboot_required true
 */
PARAM_DEFINE_FLOAT(BQ769X2_DIODEMA, 500.f);

/**
 * BQ769x2 FET behavior options (data memory FET_OPTIONS).
 *
 * Default 0x1D follows LibreSolar pre-discharge behavior.
 *
 * @group Sensors
 * @min 0
 * @max 255
 * @reboot_required true
 */
PARAM_DEFINE_INT32(BQ769X2_FETOPT, 29);

/**
 * Automatic FET control policy.
 *
 * When enabled, CHG/DSG/PCHG are controlled in firmware:
 *  - forced OFF when BQ safety faults are active or ALL_OK gate is low
 *  - driven to BQ769X2_FETMASK when all checks are OK
 *
 * @group Sensors
 * @boolean
 */
PARAM_DEFINE_INT32(BQ769X2_FET_AUTO, 1);

/**
 * FET mask to apply when automatic policy is allowed.
 *
 * Bits: 0=CHG, 1=PCHG, 2=DSG, 3=PDSG.
 *
 * @group Sensors
 * @min 0
 * @max 15
 */
PARAM_DEFINE_INT32(BQ769X2_FETMASK, 5);

/**
 * Precharge stage mask used during ALL_OK transition to ON.
 *
 * Bits: 0=CHG, 1=PCHG, 2=DSG, 3=PDSG.
 * During staged turn-on, CHG and/or DSG can be replaced by PCHG/PDSG if enabled here.
 *
 * @group Sensors
 * @min 0
 * @max 15
 */
PARAM_DEFINE_INT32(BQ769X2_PCHGMASK, 8);

/**
 * Minimum precharge stage time in milliseconds.
 *
 * Applied only when transitioning from all FETs OFF to ON and staged mask differs from final mask.
 * Precharge completion additionally requires |Vstack - Vpack| <= BQ769X2_PDV_MV
 * before BQ769X2_PTO_MS timeout.
 *
 * @group Sensors
 * @min 0
 * @max 2000
 */
PARAM_DEFINE_INT32(BQ769X2_PCHG_MS, 50);

/**
 * Postcharge overlap duration in milliseconds.
 *
 * After precharge stage, main FETs are turned on while precharge FET stays on
 * for this additional overlap time, then precharge FET is turned off.
 *
 * @group Sensors
 * @min 0
 * @max 2000
 */
PARAM_DEFINE_INT32(BQ769X2_POST_MS, 10);

/**
 * Precharge equalization timeout in milliseconds.
 *
 * During staged precharge, firmware compares stack voltage and pack voltage.
 * If |Vstack - Vpack| does not fall below BQ769X2_PDV_MV before timeout,
 * precharge fails and main FET closure is aborted.
 *
 * @group Sensors
 * @min 1
 * @max 5000
 */
PARAM_DEFINE_INT32(BQ769X2_PTO_MS, 500);

/**
 * Precharge equalization delta threshold in mV.
 *
 * Staged precharge is considered complete when |Vstack - Vpack|
 * is less than or equal to this threshold.
 *
 * @group Sensors
 * @min 1
 * @max 5000
 */
PARAM_DEFINE_FLOAT(BQ769X2_PDV_MV, 200.f);

/**
 * External ALL_OK gate for automatic FET policy.
 *
 * Intended for a board manager module to gate FET closure from cross-device checks.
 * If 0, automatic policy forces all FETs off.
 *
 * @group Sensors
 * @boolean
 */
PARAM_DEFINE_INT32(BQ769X2_ALL_OK, 1);

/**
 * Enable open-wire consistency check.
 *
 * Compares measured stack voltage against sum of selected cell voltages.
 * On mismatch, FAULT_CELL_FAIL is raised and automatic FET policy will force OFF.
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
