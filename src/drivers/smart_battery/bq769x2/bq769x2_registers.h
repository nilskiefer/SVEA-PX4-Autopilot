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

#include <stdint.h>

static constexpr uint8_t BQ769X2_DEFAULT_I2C_ADDR{0x08};

/* Direct commands */
static constexpr uint8_t BQ769X2_CMD_SAFETY_STATUS_A{0x03};
static constexpr uint8_t BQ769X2_CMD_SAFETY_STATUS_B{0x05};
static constexpr uint8_t BQ769X2_CMD_SAFETY_STATUS_C{0x07};
static constexpr uint8_t BQ769X2_CMD_PF_STATUS_A{0x0B};
static constexpr uint8_t BQ769X2_CMD_PF_STATUS_B{0x0D};
static constexpr uint8_t BQ769X2_CMD_PF_STATUS_C{0x0F};
static constexpr uint8_t BQ769X2_CMD_PF_STATUS_D{0x11};
static constexpr uint8_t BQ769X2_CMD_BATTERY_STATUS{0x12};
static constexpr uint8_t BQ769X2_CMD_VOLTAGE_CELL_1{0x14};
static constexpr uint8_t BQ769X2_CMD_VOLTAGE_STACK{0x34};
static constexpr uint8_t BQ769X2_CMD_VOLTAGE_PACK{0x36};
static constexpr uint8_t BQ769X2_CMD_CURRENT_CC2{0x3A};
static constexpr uint8_t BQ769X2_CMD_SUBCMD_LOWER{0x3E};
static constexpr uint8_t BQ769X2_SUBCMD_DATA_START{0x40};
static constexpr uint8_t BQ769X2_SUBCMD_DATA_CHECKSUM{0x60};
static constexpr uint8_t BQ769X2_SUBCMD_DATA_LENGTH{0x61};
static constexpr uint8_t BQ769X2_CMD_TEMP_INT{0x68};
static constexpr uint8_t BQ769X2_CMD_FET_STATUS{0x7F};

/* Subcommands */
static constexpr uint16_t BQ769X2_SUBCMD_DEVICE_NUMBER{0x0001};
static constexpr uint16_t BQ769X2_SUBCMD_FW_VERSION{0x0002};
static constexpr uint16_t BQ769X2_SUBCMD_HW_VERSION{0x0003};
static constexpr uint16_t BQ769X2_SUBCMD_MFG_STATUS{0x0057};
static constexpr uint16_t BQ769X2_SUBCMD_FET_ENABLE{0x0022};
static constexpr uint16_t BQ769X2_SUBCMD_SET_CFGUPDATE{0x0090};
static constexpr uint16_t BQ769X2_SUBCMD_EXIT_CFGUPDATE{0x0092};
static constexpr uint16_t BQ769X2_SUBCMD_DSG_PDSG_OFF{0x0093};
static constexpr uint16_t BQ769X2_SUBCMD_CHG_PCHG_OFF{0x0094};
static constexpr uint16_t BQ769X2_SUBCMD_ALL_FETS_OFF{0x0095};
static constexpr uint16_t BQ769X2_SUBCMD_ALL_FETS_ON{0x0096};
static constexpr uint16_t BQ769X2_SUBCMD_FET_CONTROL{0x0097};

/* Data memory */
static constexpr uint16_t BQ769X2_CAL_CURR_CC_GAIN{0x91A8};
static constexpr uint16_t BQ769X2_SET_CONF_POWER{0x9234};
static constexpr uint16_t BQ769X2_SET_CONF_ALERT{0x92FC};
static constexpr uint16_t BQ769X2_SET_PROT_ENABLED_A{0x9261};
static constexpr uint16_t BQ769X2_SET_PROT_ENABLED_B{0x9262};
static constexpr uint16_t BQ769X2_SET_ALARM_DEFAULT_MASK{0x926D};
static constexpr uint16_t BQ769X2_SET_ALARM_SF_ALERT_MASK_A{0x926F};
static constexpr uint16_t BQ769X2_SET_ALARM_SF_ALERT_MASK_B{0x9270};
static constexpr uint16_t BQ769X2_SET_ALARM_PF_ALERT_MASK_A{0x92C4};
static constexpr uint16_t BQ769X2_SET_ALARM_PF_ALERT_MASK_B{0x92C5};
static constexpr uint16_t BQ769X2_SET_ALARM_PF_ALERT_MASK_C{0x92C6};
static constexpr uint16_t BQ769X2_SET_ALARM_PF_ALERT_MASK_D{0x92C7};
static constexpr uint16_t BQ769X2_SET_CONF_DA{0x9303};
static constexpr uint16_t BQ769X2_SET_CONF_VCELL_MODE{0x9304};
static constexpr uint16_t BQ769X2_SET_FET_OPTIONS{0x9308};
static constexpr uint16_t BQ769X2_SET_FET_PDSG_TIMEOUT{0x930E};
static constexpr uint16_t BQ769X2_SET_FET_PDSG_STOP_DV{0x930F};   // 10 mV/LSB
static constexpr uint16_t BQ769X2_SET_OPEN_WIRE_CHECK_TIME{0x9314};

static constexpr uint16_t BQ769X2_PROT_CUV_THRESHOLD{0x9275};   // 50.6 mV
static constexpr uint16_t BQ769X2_PROT_CUV_DELAY{0x9276};       // 3.3 ms
static constexpr uint16_t BQ769X2_PROT_CUV_RECOV_HYST{0x927B};  // 50.6 mV
static constexpr uint16_t BQ769X2_PROT_COV_THRESHOLD{0x9278};   // 50.6 mV
static constexpr uint16_t BQ769X2_PROT_COV_DELAY{0x9279};       // 3.3 ms
static constexpr uint16_t BQ769X2_PROT_COV_RECOV_HYST{0x927C};  // 50.6 mV
static constexpr uint16_t BQ769X2_PROT_OCC_THRESHOLD{0x9280};   // 2 mV
static constexpr uint16_t BQ769X2_PROT_OCC_DELAY{0x9281};       // 3.3 ms
static constexpr uint16_t BQ769X2_PROT_OCD1_THRESHOLD{0x9282};  // 2 mV
static constexpr uint16_t BQ769X2_PROT_OCD1_DELAY{0x9283};      // 3.3 ms
static constexpr uint16_t BQ769X2_PROT_SCD_THRESHOLD{0x9286};
static constexpr uint16_t BQ769X2_PROT_SCD_DELAY{0x9287};       // 15 us
static constexpr uint16_t BQ769X2_PROT_OTC_THRESHOLD{0x929A};   // degC
static constexpr uint16_t BQ769X2_PROT_OTC_RECOVERY{0x929C};    // degC
static constexpr uint16_t BQ769X2_PROT_OTD_THRESHOLD{0x929D};   // degC
static constexpr uint16_t BQ769X2_PROT_OTD_RECOVERY{0x929F};    // degC
static constexpr uint16_t BQ769X2_PROT_UTC_THRESHOLD{0x92A6};   // degC
static constexpr uint16_t BQ769X2_PROT_UTC_RECOVERY{0x92A8};    // degC
static constexpr uint16_t BQ769X2_PROT_UTD_THRESHOLD{0x92A9};   // degC
static constexpr uint16_t BQ769X2_PROT_UTD_RECOVERY{0x92AB};    // degC
static constexpr uint16_t BQ769X2_SET_PROT_BODY_DIODE_TH{0x9273}; // mA

static constexpr uint8_t BQ769X2_SUBCMD_OVERHEAD_BYTES{4};
static constexpr uint8_t BQ769X2_DATA_BUFFER_SIZE{32};

static constexpr uint16_t BQ769X2_BAT_STATUS_CFGUPDATE_MASK{1u << 0};
static constexpr uint16_t BQ769X2_MFG_STATUS_FET_EN_MASK{1u << 4};

/* FET status/FET control bits */
static constexpr uint8_t BQ769X2_FET_CHG{1u << 0};
static constexpr uint8_t BQ769X2_FET_PCHG{1u << 1};
static constexpr uint8_t BQ769X2_FET_DSG{1u << 2};
static constexpr uint8_t BQ769X2_FET_PDSG{1u << 3};

// PROT_ENABLED_A bit masks (same bit positions as safety A status fields)
static constexpr uint8_t BQ769X2_PROT_EN_A_CUV{1u << 2};
static constexpr uint8_t BQ769X2_PROT_EN_A_COV{1u << 3};
static constexpr uint8_t BQ769X2_PROT_EN_A_OCC{1u << 4};
static constexpr uint8_t BQ769X2_PROT_EN_A_OCD1{1u << 5};
static constexpr uint8_t BQ769X2_PROT_EN_A_OCD2{1u << 6};
static constexpr uint8_t BQ769X2_PROT_EN_A_SCD{1u << 7};
static constexpr uint8_t BQ769X2_PROT_EN_B_UTC{1u << 0};
static constexpr uint8_t BQ769X2_PROT_EN_B_UTD{1u << 1};
static constexpr uint8_t BQ769X2_PROT_EN_B_OTC{1u << 4};
static constexpr uint8_t BQ769X2_PROT_EN_B_OTD{1u << 5};

/* Safety status register bits */
static constexpr uint8_t BQ769X2_SAFETY_A_CUV{1u << 2};
static constexpr uint8_t BQ769X2_SAFETY_A_COV{1u << 3};
static constexpr uint8_t BQ769X2_SAFETY_A_OCC{1u << 4};
static constexpr uint8_t BQ769X2_SAFETY_A_OCD1{1u << 5};
static constexpr uint8_t BQ769X2_SAFETY_A_OCD2{1u << 6};
static constexpr uint8_t BQ769X2_SAFETY_A_SCD{1u << 7};

static constexpr uint8_t BQ769X2_SAFETY_B_UTC{1u << 0};
static constexpr uint8_t BQ769X2_SAFETY_B_UTD{1u << 1};
static constexpr uint8_t BQ769X2_SAFETY_B_UTINT{1u << 2};
static constexpr uint8_t BQ769X2_SAFETY_B_OTC{1u << 4};
static constexpr uint8_t BQ769X2_SAFETY_B_OTD{1u << 5};
static constexpr uint8_t BQ769X2_SAFETY_B_OTINT{1u << 6};
static constexpr uint8_t BQ769X2_SAFETY_B_OTF{1u << 7};

static constexpr uint8_t BQ769X2_SAFETY_C_HWDF{1u << 1};
static constexpr uint8_t BQ769X2_SAFETY_C_PTO{1u << 2};
static constexpr uint8_t BQ769X2_SAFETY_C_COVL{1u << 4};
static constexpr uint8_t BQ769X2_SAFETY_C_OCDL{1u << 5};
static constexpr uint8_t BQ769X2_SAFETY_C_SCDL{1u << 6};
static constexpr uint8_t BQ769X2_SAFETY_C_OCD3{1u << 7};
