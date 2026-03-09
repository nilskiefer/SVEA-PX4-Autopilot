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
#include <px4_platform_common/atomic.h>

namespace led_bus
{
// Bit i corresponds to physical LED Li+1 (i=0 -> L1, ..., i=5 -> L6).
inline px4::atomic<uint8_t> &pattern_mask()
{
	static px4::atomic<uint8_t> s_pattern_mask{0};
	return s_pattern_mask;
}

inline px4::atomic<uint8_t> &button_mask()
{
	static px4::atomic<uint8_t> s_button_mask{0};
	return s_button_mask;
}

inline void set_pattern_mask(uint8_t mask)
{
	pattern_mask().store(mask & 0x3fu);
}

inline void set_button_mask(uint8_t mask)
{
	button_mask().store(mask & 0x3fu);
}

inline void clear_pattern_mask()
{
	pattern_mask().store(0);
}

inline void clear_button_mask()
{
	button_mask().store(0);
}

inline uint8_t get_combined_mask()
{
	// ON wins: button requests force LEDs on over pattern.
	return static_cast<uint8_t>(pattern_mask().load() | button_mask().load());
}
} // namespace led_bus
