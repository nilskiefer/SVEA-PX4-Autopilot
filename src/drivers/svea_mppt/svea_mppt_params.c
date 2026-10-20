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
 * Enable PMB3 digital MPPT adjustment
 *
 * 0 releases GPIO6: Q601 bypasses the digipot, using the fixed divider (~19.1 V).
 * 1 sinks GPIO6 after wiper readback: SVEA_MPPT_V adjusts the CN3722 setpoint.
 * This does not enable/disable charging or run an MPPT search algorithm.
 *
 * @boolean
 * @group SVEA MPPT
 */
PARAM_DEFINE_INT32(SVEA_MPPT_EN, 0);

/**
 * PMB3 MPPT input-voltage target
 *
 * Converted to the nearest nominal TPL0401B wiper setting using the PMB3
 * 100 kOhm / 5.76 kOhm divider, 10 kOhm digipot and 1.04 V CN3722 reference, with typical 100 Ohm
 * H-terminal and 35 Ohm wiper resistance.
 * Component tolerances and switch resistance affect the actual voltage.
 * Digital adjustment requires SVEA_MPPT_EN=1. Targets outside the range fault.
 *
 * @unit V
 * @min 7.66
 * @max 18.98
 * @decimal 2
 * @increment 0.1
 * @group SVEA MPPT
 */
PARAM_DEFINE_FLOAT(SVEA_MPPT_V, 18.0f);
