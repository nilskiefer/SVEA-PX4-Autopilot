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

#include <cstring>
#include <drivers/device/i2c.h>

class ISM330DLC_I2C : public device::I2C
{
public:
	explicit ISM330DLC_I2C(const I2CSPIDriverConfig &config) : I2C(config) { _retries = 5; }
	~ISM330DLC_I2C() override = default;

	int read(unsigned address, void *data, unsigned count) override;
	int write(unsigned address, void *data, unsigned count) override;

protected:
	int probe() override;
};

int ISM330DLC_I2C::probe()
{
	uint8_t who_am_i = 0;

	if (read(ISM330DLC_ADDR_WHO_AM_I, &who_am_i, 1) != PX4_OK) {
		return -EIO;
	}

	if (who_am_i != ISM330DLC_WHO_AM_I) {
		DEVICE_DEBUG("unexpected WHO_AM_I: 0x%02x", who_am_i);
		return -EIO;
	}

	_retries = 1;
	return PX4_OK;
}

int ISM330DLC_I2C::read(unsigned address, void *data, unsigned count)
{
	uint8_t cmd = address;
	return transfer(&cmd, 1, static_cast<uint8_t *>(data), count);
}

int ISM330DLC_I2C::write(unsigned address, void *data, unsigned count)
{
	uint8_t buf[32] {};

	if (count + 1 > sizeof(buf)) {
		return -EIO;
	}

	buf[0] = address;
	memcpy(&buf[1], data, count);

	return transfer(buf, count + 1, nullptr, 0);
}

device::Device *ISM330DLC_I2C_interface(const I2CSPIDriverConfig &config)
{
	return new ISM330DLC_I2C(config);
}
