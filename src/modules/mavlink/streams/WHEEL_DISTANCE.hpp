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

#ifndef WHEEL_DISTANCE_HPP
#define WHEEL_DISTANCE_HPP

#include <uORB/topics/wheel_distance.h>

class MavlinkStreamWheelDistance : public MavlinkStream
{
public:
	static MavlinkStream *new_instance(Mavlink *mavlink) { return new MavlinkStreamWheelDistance(mavlink); }

	static constexpr const char *get_name_static() { return "WHEEL_DISTANCE"; }
	static constexpr uint16_t get_id_static() { return MAVLINK_MSG_ID_WHEEL_DISTANCE; }

	const char *get_name() const override { return get_name_static(); }
	uint16_t get_id() override { return get_id_static(); }

	unsigned get_size() override
	{
		return _wheel_distance_sub.advertised() ? (MAVLINK_MSG_ID_WHEEL_DISTANCE_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES) : 0;
	}

private:
	explicit MavlinkStreamWheelDistance(Mavlink *mavlink) : MavlinkStream(mavlink) {}

	uORB::Subscription _wheel_distance_sub{ORB_ID(wheel_distance)};

	bool send() override
	{
		wheel_distance_s wheel_distance{};

		if (_wheel_distance_sub.update(&wheel_distance)) {
			mavlink_wheel_distance_t msg{};

			msg.time_usec = (wheel_distance.timestamp_sample != 0) ? wheel_distance.timestamp_sample : wheel_distance.timestamp;
			msg.count = 2;
			msg.distance[0] = (double)wheel_distance.left_distance_m;
			msg.distance[1] = (double)wheel_distance.right_distance_m;

			mavlink_msg_wheel_distance_send_struct(_mavlink->get_channel(), &msg);
			return true;
		}

		return false;
	}
};

#endif // WHEEL_DISTANCE_HPP
