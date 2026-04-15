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

#include <stddef.h>
#include <stdint.h>

class BQ769x2ProtocolBus
{
public:
	virtual ~BQ769x2ProtocolBus() = default;
	virtual int busTransfer(const uint8_t *send, unsigned send_len, uint8_t *recv, unsigned recv_len) = 0;
};

class BQ769x2Protocol
{
public:
	BQ769x2Protocol(BQ769x2ProtocolBus &bus, uint8_t i2c_addr, bool crc_enabled = false);

	void setCRCEnabled(bool enabled) { _crc_enabled = enabled; }

	int directReadU1(uint8_t reg_addr, uint8_t &value);
	int directReadU2(uint8_t reg_addr, uint16_t &value);
	int directReadI2(uint8_t reg_addr, int16_t &value);

	int subcommand(uint16_t subcmd);
	int subcommandWriteU1(uint16_t subcmd, uint8_t value);
	int subcommandReadU2(uint16_t subcmd, uint16_t &value);
	int subcommandReadU4(uint16_t subcmd, uint32_t &value);

	int datamemReadU1(uint16_t addr, uint8_t &value);
	int datamemReadU2(uint16_t addr, uint16_t &value);
	int datamemWriteU1(uint16_t addr, uint8_t value);
	int datamemWriteU2(uint16_t addr, uint16_t value);
	int datamemWriteF4(uint16_t addr, float value);

	int setConfigUpdateMode(bool enabled);

private:
	static constexpr int ReadMaxAttempts{100};
	static constexpr uint32_t ReadDelayUs{100};

	BQ769x2ProtocolBus &_bus;
	const uint8_t _i2c_addr;
	bool _crc_enabled{false};

	int writeBytes(uint8_t reg_addr, const uint8_t *data, size_t num_bytes);
	int readBytes(uint8_t reg_addr, uint8_t *data, size_t num_bytes);
	int dataRead(uint16_t addr, uint8_t *bytes, size_t num_bytes);
	int dataWrite(uint16_t addr, uint32_t value, size_t num_bytes);

	static uint8_t crc8Ccitt(const uint8_t *data, size_t len);
};
