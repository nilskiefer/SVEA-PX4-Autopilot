#include "svea_peripheral_mcu.hpp"

#include <string.h>

const SveaPeripheralMcu::MessageRoute SveaPeripheralMcu::kMessageRoutes[] = {
	{PeripheralMsgId::WheelDistance, (uint8_t)sizeof(WheelDistancePayload), "wheel_distance", &SveaPeripheralMcu::handle_wheel_distance},
	{PeripheralMsgId::WheelEncoders, (uint8_t)sizeof(WheelEncodersPayload), "wheel_encoders", &SveaPeripheralMcu::handle_wheel_encoders},
};

size_t SveaPeripheralMcu::message_route_count() const
{
	return sizeof(kMessageRoutes) / sizeof(kMessageRoutes[0]);
}

uint16_t SveaPeripheralMcu::crc16_ccitt(const uint8_t *data, uint16_t len)
{
	uint16_t crc = 0xFFFF;

	for (uint16_t i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i] << 8;

		for (int bit = 0; bit < 8; bit++) {
			if (crc & 0x8000) {
				crc = (uint16_t)((crc << 1) ^ 0x1021);

			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

bool SveaPeripheralMcu::dispatch_frame()
{
	for (size_t i = 0; i < message_route_count(); i++) {
		const MessageRoute &route = kMessageRoutes[i];

		if ((uint8_t)route.msg_id != _frame_msg_id) {
			continue;
		}

		if (_frame_len != route.expected_payload_len) {
			_frames_parse_error++;
			return false;
		}

		const bool ok = (this->*(route.handler))(_payload, _frame_len);

		if (ok) {
			_route_frames[i]++;

		} else {
			_frames_parse_error++;
		}

		return ok;
	}

	_frames_unknown_msgid++;
	_frames_parse_error++;
	return false;
}

void SveaPeripheralMcu::parse_byte(uint8_t byte)
{
	if (byte == FRAME_MAGIC0) {
		_magic0_hits++;
	}

	if (byte == FRAME_MAGIC1) {
		_magic1_hits++;
	}

	if (byte == 0xFE) {
		_mavlink_v1_hits++;
	}

	if (byte == 0xFD) {
		_mavlink_v2_hits++;
	}

	_last_bytes[_last_bytes_pos] = byte;
	_last_bytes_pos = (_last_bytes_pos + 1) % sizeof(_last_bytes);

	if (_last_bytes_len < sizeof(_last_bytes)) {
		_last_bytes_len++;
	}

	switch (_state) {
	case ParseState::WaitMagic0:
		if (byte == FRAME_MAGIC0) {
			_state = ParseState::WaitMagic1;
		}

		break;

	case ParseState::WaitMagic1:
		if (byte == FRAME_MAGIC1) {
			_state = ParseState::WaitVersion;

		} else {
			_state = ParseState::WaitMagic0;
		}

		break;

	case ParseState::WaitVersion:
		_frame_version = byte;

		if (_frame_version != FRAME_VERSION) {
			_frames_parse_error++;
			_state = ParseState::WaitMagic0;

		} else {
			_state = ParseState::WaitMsgId;
		}

		break;

	case ParseState::WaitMsgId:
		_frame_msg_id = byte;
		_state = ParseState::WaitLength;
		break;

	case ParseState::WaitLength:
		_frame_len = byte;

		if (_frame_len == 0 || _frame_len > FRAME_PAYLOAD_MAX_LEN) {
			_frames_parse_error++;
			_state = ParseState::WaitMagic0;

		} else {
			_payload_pos = 0;
			_state = ParseState::WaitPayload;
		}

		break;

	case ParseState::WaitPayload:
		_payload[_payload_pos++] = byte;

		if (_payload_pos >= _frame_len) {
			_state = ParseState::WaitCrc0;
		}

		break;

	case ParseState::WaitCrc0:
		_crc_lsb = byte;
		_state = ParseState::WaitCrc1;
		break;

	case ParseState::WaitCrc1: {
			const uint16_t crc_rx = (uint16_t)_crc_lsb | ((uint16_t)byte << 8);
			uint8_t crc_data[3 + FRAME_PAYLOAD_MAX_LEN] {};
			crc_data[0] = _frame_version;
			crc_data[1] = _frame_msg_id;
			crc_data[2] = _frame_len;
			memcpy(&crc_data[3], _payload, _frame_len);
			const uint16_t crc_calc = crc16_ccitt(crc_data, (uint16_t)(3 + _frame_len));

			if (crc_rx == crc_calc) {
				if (dispatch_frame()) {
					_frames_rx++;
				}

			} else {
				_frames_crc_error++;
			}

			_state = ParseState::WaitMagic0;
			break;
		}
	}
}
