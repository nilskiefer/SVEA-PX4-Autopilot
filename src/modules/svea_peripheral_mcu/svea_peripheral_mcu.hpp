#pragma once

#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <drivers/drv_hrt.h>
#include <stddef.h>
#include <uORB/Publication.hpp>
#include <uORB/topics/wheel_distance.h>
#include <uORB/topics/wheel_encoders.h>

class SveaPeripheralMcu : public ModuleBase<SveaPeripheralMcu>, public px4::ScheduledWorkItem
{
public:
	SveaPeripheralMcu(const char *device, int baudrate);
	~SveaPeripheralMcu() override;

	static int task_spawn(int argc, char *argv[]);
	static SveaPeripheralMcu *instantiate(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]) { return print_usage("unknown command"); }
	static int print_usage(const char *reason = nullptr);

	bool init();
	void Run() override;
	int print_status() override;

private:
	// Framing constants for MCU->PX4 transport:
	// [0x53 0x45][version][msg_id][payload_len][payload...][crc16_le]
	static constexpr uint8_t FRAME_MAGIC0 = 0x53; // 'S'
	static constexpr uint8_t FRAME_MAGIC1 = 0x45; // 'E'
	static constexpr uint8_t FRAME_VERSION = 2;
	static constexpr uint8_t FRAME_PAYLOAD_MAX_LEN = 64;
	static constexpr size_t MAX_MESSAGE_ROUTES = 16;
	static constexpr uint32_t SCHEDULE_INTERVAL_US = 5000;
	static constexpr const char *BUILD_TAG = "periphmcu-rxfix-20260505-5";

	enum class PeripheralMsgId : uint8_t {
		WheelDistance = 1,
		WheelEncoders = 2,
	};

	enum class ParseState : uint8_t {
		WaitMagic0,
		WaitMagic1,
		WaitVersion,
		WaitMsgId,
		WaitLength,
		WaitPayload,
		WaitCrc0,
		WaitCrc1,
	};

	struct __attribute__((packed)) WheelDistancePayload {
		uint32_t sequence;
		uint32_t time_ms;
		float left_distance_m;
		float right_distance_m;
	};

	struct __attribute__((packed)) WheelEncodersPayload {
		uint32_t sequence;
		uint32_t time_ms;
		float right_wheel_speed_rad_s;
		float left_wheel_speed_rad_s;
		float right_wheel_angle_rad;
		float left_wheel_angle_rad;
	};

	using RouteHandler = bool (SveaPeripheralMcu::*)(const uint8_t *payload, uint8_t payload_len);

	struct MessageRoute {
		PeripheralMsgId msg_id;
		uint8_t expected_payload_len;
		const char *name;
		RouteHandler handler;
	};

	// Add new MCU->uORB mappings here:
	// 1) define payload struct
	// 2) implement handler
	// 3) append route entry in kMessageRoutes
	static const MessageRoute kMessageRoutes[];
	size_t message_route_count() const;

	static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len);

	void parse_byte(uint8_t byte);
	bool dispatch_frame();
	bool handle_wheel_distance(const uint8_t *payload, uint8_t payload_len);
	bool handle_wheel_encoders(const uint8_t *payload, uint8_t payload_len);

	int open_serial();
	void close_serial();

	uORB::Publication<wheel_distance_s> _wheel_distance_pub{ORB_ID(wheel_distance)};
	uORB::Publication<wheel_encoders_s> _wheel_encoders_pub{ORB_ID(wheel_encoders)};

	int _fd{-1};
	char _device[32] {};
	int _baudrate{115200};

	ParseState _state{ParseState::WaitMagic0};
	uint8_t _payload[FRAME_PAYLOAD_MAX_LEN] {};
	uint8_t _payload_pos{0};
	uint8_t _frame_version{0};
	uint8_t _frame_msg_id{0};
	uint8_t _frame_len{0};
	uint8_t _crc_lsb{0};

	uint32_t _frames_rx{0};
	uint32_t _route_frames[MAX_MESSAGE_ROUTES] {};
	uint32_t _frames_unknown_msgid{0};
	uint32_t _frames_crc_error{0};
	uint32_t _frames_parse_error{0};
	uint32_t _bytes_rx{0};
	uint32_t _magic0_hits{0};
	uint32_t _magic1_hits{0};
	uint32_t _mavlink_v1_hits{0};
	uint32_t _mavlink_v2_hits{0};

	uint8_t _last_bytes[16] {};
	uint8_t _last_bytes_len{0};
	uint8_t _last_bytes_pos{0};

	uint32_t _read_zero_count{0};
	uint32_t _read_err_count{0};
	int _last_read_errno{0};
	int _last_read_n{0};

	uint32_t _run_count{0};
	hrt_abstime _started_us{0};
	hrt_abstime _last_run_us{0};
	hrt_abstime _last_rx_us{0};

	uint8_t _cfg_vmin{0};
	uint8_t _cfg_vtime{0};
	uint32_t _cfg_cflag{0};
	int _fd_flags_before{0};
	int _fd_flags_after{0};
	uint32_t _open_attempts{0};
	uint32_t _open_fails{0};
	uint32_t _open_ok{0};
	int _last_open_errno{0};
	bool _isatty{false};
	bool _printed_raw_once{false};
};
