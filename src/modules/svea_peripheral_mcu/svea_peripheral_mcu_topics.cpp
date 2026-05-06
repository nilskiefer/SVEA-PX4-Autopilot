#include "svea_peripheral_mcu.hpp"

#include <string.h>

bool SveaPeripheralMcu::handle_wheel_distance(const uint8_t *payload, uint8_t payload_len)
{
	if (payload_len != sizeof(WheelDistancePayload)) {
		return false;
	}

	WheelDistancePayload frame{};
	memcpy(&frame, payload, sizeof(frame));

	wheel_distance_s msg{};
	msg.timestamp = hrt_absolute_time();
	msg.timestamp_sample = (uint64_t)frame.time_ms * 1000ULL;
	msg.sequence = frame.sequence;
	msg.left_distance_m = frame.left_distance_m;
	msg.right_distance_m = frame.right_distance_m;
	_wheel_distance_pub.publish(msg);
	return true;
}

bool SveaPeripheralMcu::handle_wheel_encoders(const uint8_t *payload, uint8_t payload_len)
{
	if (payload_len != sizeof(WheelEncodersPayload)) {
		return false;
	}

	WheelEncodersPayload frame{};
	memcpy(&frame, payload, sizeof(frame));

	wheel_encoders_s msg{};
	msg.timestamp = hrt_absolute_time();
	msg.wheel_speed[0] = frame.right_wheel_speed_rad_s;
	msg.wheel_speed[1] = frame.left_wheel_speed_rad_s;
	msg.wheel_angle[0] = frame.right_wheel_angle_rad;
	msg.wheel_angle[1] = frame.left_wheel_angle_rad;
	_wheel_encoders_pub.publish(msg);
	return true;
}
