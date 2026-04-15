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

#include "bq769x2_protocol.h"

#include "bq769x2_registers.h"

#include <px4_platform_common/defines.h>
#include <px4_platform_common/time.h>

#include <errno.h>
#include <string.h>

BQ769x2Protocol::BQ769x2Protocol(BQ769x2ProtocolBus &bus, uint8_t i2c_addr, bool crc_enabled) :
	_bus(bus),
	_i2c_addr(i2c_addr),
	_crc_enabled(crc_enabled)
{
}

int BQ769x2Protocol::directReadU1(uint8_t reg_addr, uint8_t &value)
{
	return readBytes(reg_addr, &value, 1);
}

int BQ769x2Protocol::directReadU2(uint8_t reg_addr, uint16_t &value)
{
	uint8_t bytes[2] {};
	const int ret = readBytes(reg_addr, bytes, sizeof(bytes));

	if (ret != PX4_OK) {
		return ret;
	}

	value = static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
	return PX4_OK;
}

int BQ769x2Protocol::directReadI2(uint8_t reg_addr, int16_t &value)
{
	uint16_t u16{0};
	const int ret = directReadU2(reg_addr, u16);

	if (ret != PX4_OK) {
		return ret;
	}

	value = static_cast<int16_t>(u16);
	return PX4_OK;
}

int BQ769x2Protocol::subcommand(uint16_t subcmd)
{
	return dataWrite(subcmd, 0, 0);
}

int BQ769x2Protocol::subcommandWriteU1(uint16_t subcmd, uint8_t value)
{
	return dataWrite(subcmd, value, 1);
}

int BQ769x2Protocol::subcommandReadU2(uint16_t subcmd, uint16_t &value)
{
	uint8_t bytes[2] {};
	const int ret = dataRead(subcmd, bytes, sizeof(bytes));

	if (ret != PX4_OK) {
		return ret;
	}

	value = static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
	return PX4_OK;
}

int BQ769x2Protocol::subcommandReadU4(uint16_t subcmd, uint32_t &value)
{
	uint8_t bytes[4] {};
	const int ret = dataRead(subcmd, bytes, sizeof(bytes));

	if (ret != PX4_OK) {
		return ret;
	}

	value = static_cast<uint32_t>(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
	return PX4_OK;
}

int BQ769x2Protocol::datamemReadU1(uint16_t addr, uint8_t &value)
{
	return dataRead(addr, &value, 1);
}

int BQ769x2Protocol::datamemReadU2(uint16_t addr, uint16_t &value)
{
	uint8_t bytes[2] {};
	const int ret = dataRead(addr, bytes, sizeof(bytes));

	if (ret != PX4_OK) {
		return ret;
	}

	value = static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
	return PX4_OK;
}

int BQ769x2Protocol::datamemWriteU1(uint16_t addr, uint8_t value)
{
	return dataWrite(addr, value, 1);
}

int BQ769x2Protocol::datamemWriteU2(uint16_t addr, uint16_t value)
{
	return dataWrite(addr, value, 2);
}

int BQ769x2Protocol::datamemWriteF4(uint16_t addr, float value)
{
	uint32_t raw{0};
	static_assert(sizeof(raw) == sizeof(value), "float size mismatch");
	memcpy(&raw, &value, sizeof(raw));
	return dataWrite(addr, raw, 4);
}

int BQ769x2Protocol::setConfigUpdateMode(bool enabled)
{
	int ret = enabled ? subcommand(BQ769X2_SUBCMD_SET_CFGUPDATE) : subcommand(BQ769X2_SUBCMD_EXIT_CFGUPDATE);

	if (ret != PX4_OK) {
		return ret;
	}

	px4_usleep(enabled ? 2000 : 1000);

	for (int attempt = 0; attempt < 5; attempt++) {
		uint16_t battery_status{0};
		ret = directReadU2(BQ769X2_CMD_BATTERY_STATUS, battery_status);

		if (ret == PX4_OK) {
			const bool config_update = (battery_status & BQ769X2_BAT_STATUS_CFGUPDATE_MASK) != 0;

			if (config_update == enabled) {
				return PX4_OK;
			}
		}

		px4_usleep(500);
	}

	return -EIO;
}

int BQ769x2Protocol::writeBytes(uint8_t reg_addr, const uint8_t *data, size_t num_bytes)
{
	if (data == nullptr || num_bytes < 1) {
		return -EINVAL;
	}

	if (!_crc_enabled) {
		uint8_t tx[BQ769X2_DATA_BUFFER_SIZE + 1] {};

		if (num_bytes > BQ769X2_DATA_BUFFER_SIZE) {
			return -EINVAL;
		}

		tx[0] = reg_addr;
		memcpy(&tx[1], data, num_bytes);
		return _bus.busTransfer(tx, num_bytes + 1, nullptr, 0);
	}

	if (num_bytes > 4) {
		return -EINVAL;
	}

	uint8_t tx[(2 * 4) + 1] {};
	tx[0] = reg_addr;

	const uint8_t addr_w = static_cast<uint8_t>(_i2c_addr << 1);
	uint8_t crc_input[4] {addr_w, reg_addr, 0, 0};

	for (size_t i = 0; i < num_bytes; i++) {
		const uint8_t byte = data[i];
		tx[1 + (2 * i)] = byte;

		if (i == 0) {
			crc_input[2] = byte;
			tx[2 + (2 * i)] = crc8Ccitt(crc_input, 3);

		} else {
			tx[2 + (2 * i)] = crc8Ccitt(&byte, 1);
		}
	}

	return _bus.busTransfer(tx, static_cast<unsigned>(1 + (2 * num_bytes)), nullptr, 0);
}

int BQ769x2Protocol::readBytes(uint8_t reg_addr, uint8_t *data, size_t num_bytes)
{
	if (data == nullptr || num_bytes < 1) {
		return -EINVAL;
	}

	if (!_crc_enabled) {
		return _bus.busTransfer(&reg_addr, 1, data, static_cast<unsigned>(num_bytes));
	}

	if (num_bytes > BQ769X2_DATA_BUFFER_SIZE) {
		return -EINVAL;
	}

	uint8_t rx[BQ769X2_DATA_BUFFER_SIZE * 2] {};
	const int ret = _bus.busTransfer(&reg_addr, 1, rx, static_cast<unsigned>(num_bytes * 2));

	if (ret != PX4_OK) {
		return ret;
	}

	const uint8_t addr_w = static_cast<uint8_t>(_i2c_addr << 1);
	const uint8_t addr_r = static_cast<uint8_t>((_i2c_addr << 1) | 0x01);
	uint8_t first_crc_input[4] {addr_w, reg_addr, addr_r, rx[0]};
	const uint8_t first_crc = crc8Ccitt(first_crc_input, sizeof(first_crc_input));

	if (rx[1] != first_crc) {
		return -EIO;
	}

	data[0] = rx[0];

	for (size_t i = 1; i < num_bytes; i++) {
		const uint8_t byte = rx[2 * i];
		const uint8_t crc_read = rx[(2 * i) + 1];
		const uint8_t crc_expected = crc8Ccitt(&byte, 1);

		if (crc_read != crc_expected) {
			return -EIO;
		}

		data[i] = byte;
	}

	return PX4_OK;
}

int BQ769x2Protocol::dataRead(uint16_t addr, uint8_t *bytes, size_t num_bytes)
{
	if (bytes == nullptr || num_bytes < 1 || num_bytes > BQ769X2_DATA_BUFFER_SIZE) {
		return -EINVAL;
	}

	const uint8_t addr_bytes[2] {
		static_cast<uint8_t>(addr & 0xFF),
		static_cast<uint8_t>(addr >> 8)
	};

	int ret = writeBytes(BQ769X2_CMD_SUBCMD_LOWER, addr_bytes, sizeof(addr_bytes));

	if (ret != PX4_OK) {
		return ret;
	}

	px4_usleep(50);

	uint8_t echo[2] {};

	for (int attempt = 0; attempt < ReadMaxAttempts; attempt++) {
		ret = readBytes(BQ769X2_CMD_SUBCMD_LOWER, echo, sizeof(echo));

		if (ret != PX4_OK) {
			return ret;
		}

		if (echo[0] == addr_bytes[0] && echo[1] == addr_bytes[1]) {
			break;
		}

		if (attempt == (ReadMaxAttempts - 1)) {
			return -EIO;
		}

		px4_usleep(ReadDelayUs);
	}

	uint8_t length{0};
	ret = readBytes(BQ769X2_SUBCMD_DATA_LENGTH, &length, 1);

	if (ret != PX4_OK) {
		return ret;
	}

	if (length < BQ769X2_SUBCMD_OVERHEAD_BYTES) {
		return -EIO;
	}

	const uint8_t payload_length = static_cast<uint8_t>(length - BQ769X2_SUBCMD_OVERHEAD_BYTES);

	if (payload_length > BQ769X2_DATA_BUFFER_SIZE || payload_length < num_bytes) {
		return -EIO;
	}

	uint8_t payload[BQ769X2_DATA_BUFFER_SIZE] {};
	ret = readBytes(BQ769X2_SUBCMD_DATA_START, payload, payload_length);

	if (ret != PX4_OK) {
		return ret;
	}

	uint8_t checksum{static_cast<uint8_t>(addr_bytes[0] + addr_bytes[1])};

	for (uint8_t i = 0; i < payload_length; i++) {
		checksum = static_cast<uint8_t>(checksum + payload[i]);
	}

	checksum = static_cast<uint8_t>(~checksum);

	uint8_t checksum_read{0};
	ret = readBytes(BQ769X2_SUBCMD_DATA_CHECKSUM, &checksum_read, 1);

	if (ret != PX4_OK) {
		return ret;
	}

	if (checksum != checksum_read) {
		return -EIO;
	}

	memcpy(bytes, payload, num_bytes);
	return PX4_OK;
}

int BQ769x2Protocol::dataWrite(uint16_t addr, uint32_t value, size_t num_bytes)
{
	if (num_bytes > 4) {
		return -EINVAL;
	}

	const uint8_t addr_bytes[2] {
		static_cast<uint8_t>(addr & 0xFF),
		static_cast<uint8_t>(addr >> 8)
	};

	int ret = writeBytes(BQ769X2_CMD_SUBCMD_LOWER, addr_bytes, sizeof(addr_bytes));

	if (ret != PX4_OK) {
		return ret;
	}

	if (num_bytes > 0) {
		uint8_t payload[4] {};
		uint8_t checksum{static_cast<uint8_t>(addr_bytes[0] + addr_bytes[1])};

		for (size_t i = 0; i < num_bytes; i++) {
			payload[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
			checksum = static_cast<uint8_t>(checksum + payload[i]);
		}

		ret = writeBytes(BQ769X2_SUBCMD_DATA_START, payload, num_bytes);

		if (ret != PX4_OK) {
			return ret;
		}

		checksum = static_cast<uint8_t>(~checksum);
		const uint8_t checksum_length[2] {
			checksum,
			static_cast<uint8_t>(num_bytes + BQ769X2_SUBCMD_OVERHEAD_BYTES)
		};

		ret = writeBytes(BQ769X2_SUBCMD_DATA_CHECKSUM, checksum_length, sizeof(checksum_length));

		if (ret != PX4_OK) {
			return ret;
		}
	}

	px4_usleep(200);
	return PX4_OK;
}

uint8_t BQ769x2Protocol::crc8Ccitt(const uint8_t *data, size_t len)
{
	uint8_t crc{0};

	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];

		for (int bit = 0; bit < 8; bit++) {
			if (crc & 0x80) {
				crc = static_cast<uint8_t>((crc << 1) ^ 0x07);

			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}
