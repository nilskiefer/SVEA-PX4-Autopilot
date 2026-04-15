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
 * LSM6DSOX accelerometer full-scale range [g]
 *
 * @group Sensors
 * @value 2 2 g
 * @value 4 4 g
 * @value 8 8 g
 * @value 16 16 g
 * @reboot_required true
 */
PARAM_DEFINE_INT32(LSM6DSOX_ACC_FS, 16);

/**
 * LSM6DSOX gyroscope full-scale range [dps]
 *
 * @group Sensors
 * @value 125 125 dps
 * @value 250 250 dps
 * @value 500 500 dps
 * @value 1000 1000 dps
 * @value 2000 2000 dps
 * @reboot_required true
 */
PARAM_DEFINE_INT32(LSM6DSOX_GYR_FS, 2000);

/**
 * LSM6DSOX accelerometer output data rate [Hz]
 *
 * @group Sensors
 * @value 52 52 Hz
 * @value 104 104 Hz
 * @value 208 208 Hz
 * @value 417 417 Hz
 * @value 833 833 Hz
 * @reboot_required true
 */
PARAM_DEFINE_INT32(LSM6DSOX_ACC_ODR, 417);

/**
 * LSM6DSOX gyroscope output data rate [Hz]
 *
 * @group Sensors
 * @value 52 52 Hz
 * @value 104 104 Hz
 * @value 208 208 Hz
 * @value 417 417 Hz
 * @value 833 833 Hz
 * @reboot_required true
 */
PARAM_DEFINE_INT32(LSM6DSOX_GYR_ODR, 417);
