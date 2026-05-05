#include <px4_platform_common/getopt.h>
#include <px4_platform_common/log.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <drivers/drv_hrt.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <uORB/Publication.hpp>
#include <uORB/topics/wheel_distance.h>

extern "C" __EXPORT int svea_encoder_uart_main(int argc, char *argv[]);

class SveaEncoderUart : public ModuleBase, public px4::ScheduledWorkItem
{
public:
	static Descriptor desc;

	SveaEncoderUart(const char *device, int baudrate) :
		ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::lp_default),
		_baudrate(baudrate)
	{
		strncpy(_device, device, sizeof(_device) - 1);
		_device[sizeof(_device) - 1] = '\0';
	}

	~SveaEncoderUart() override
	{
		if (_fd >= 0) {
			::close(_fd);
		}
	}

	static int task_spawn(int argc, char *argv[]);
	static SveaEncoderUart *instantiate(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]) { return print_usage("unknown command"); }
	static int print_usage(const char *reason = nullptr);

	bool init();
	void Run() override;
	int print_status() override;

private:
	struct __attribute__((packed)) FramePayload {
		uint32_t sequence;
		uint32_t time_ms;
		float left_distance_m;
		float right_distance_m;
	};

	static constexpr uint8_t FRAME_MAGIC0 = 0x53; // 'S'
	static constexpr uint8_t FRAME_MAGIC1 = 0x45; // 'E'
	static constexpr uint8_t FRAME_VERSION = 1;
	static constexpr uint8_t FRAME_PAYLOAD_LEN = sizeof(FramePayload);
	static constexpr uint32_t SCHEDULE_INTERVAL_US = 5000;
	static constexpr const char *BUILD_TAG = "encuart-rxfix-20260505-2";

	static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
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

	void parse_byte(uint8_t byte);
	int open_serial();

	uORB::Publication<wheel_distance_s> _wheel_distance_pub{ORB_ID(wheel_distance)};

	int _fd{-1};
	char _device[32] {};
	int _baudrate{115200};

	enum class ParseState : uint8_t {
		WaitMagic0,
		WaitMagic1,
		WaitVersion,
		WaitLength,
		WaitPayload,
		WaitCrc0,
		WaitCrc1,
	};

	ParseState _state{ParseState::WaitMagic0};
	uint8_t _payload[FRAME_PAYLOAD_LEN] {};
	uint8_t _payload_pos{0};
	uint8_t _frame_version{0};
	uint8_t _frame_len{0};
	uint8_t _crc_lsb{0};
	uint32_t _frames_rx{0};
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

ModuleBase::Descriptor SveaEncoderUart::desc{task_spawn, custom_command, print_usage};

int SveaEncoderUart::open_serial()
{
	// Open read/write to avoid NuttX returning a write-only descriptor mode
	// for this tty path on some board configurations (read() then fails EBADF).
	_fd = ::open(_device, O_RDWR | O_NOCTTY);

	if (_fd < 0) {
		PX4_ERR("open failed: %s (%d)", _device, errno);
		return PX4_ERROR;
	}

	_isatty = (::isatty(_fd) == 1);

	if (!_isatty) {
		PX4_ERR("fd for %s is not a TTY device", _device);
		return PX4_ERROR;
	}

	_fd_flags_before = ::fcntl(_fd, F_GETFL, 0);

	if (_fd_flags_before < 0) {
		PX4_ERR("fcntl(F_GETFL) failed (%d)", errno);
		return PX4_ERROR;
	}

	// Hard-fail if we cannot clear O_NONBLOCK: this driver expects timed/blocking reads.
	if ((_fd_flags_before & O_NONBLOCK) != 0) {
		if (::fcntl(_fd, F_SETFL, _fd_flags_before & ~O_NONBLOCK) != 0) {
			PX4_ERR("fcntl(F_SETFL clear O_NONBLOCK) failed (%d)", errno);
			return PX4_ERROR;
		}
	}

	_fd_flags_after = ::fcntl(_fd, F_GETFL, 0);

	if (_fd_flags_after < 0) {
		PX4_ERR("fcntl(F_GETFL after) failed (%d)", errno);
		return PX4_ERROR;
	}

	speed_t speed;

#ifndef B921600
#define B921600 921600
#endif

	switch (_baudrate) {
	case 115200: speed = B115200; break;

	case 230400: speed = B230400; break;

	case 460800: speed = B460800; break;

	case 921600: speed = B921600; break;

	default:
		PX4_ERR("unsupported baudrate: %d", _baudrate);
		return PX4_ERROR;
	}

	struct termios config {};

	if (tcgetattr(_fd, &config) != 0) {
		PX4_ERR("tcgetattr failed (%d)", errno);
		return PX4_ERROR;
	}

	config.c_cflag |= (CLOCAL | CREAD);
	config.c_cflag &= ~CSIZE;
	config.c_cflag |= CS8;
	config.c_cflag &= ~PARENB;
	config.c_cflag &= ~CSTOPB;
	config.c_cflag &= ~CRTSCTS;
	config.c_iflag = 0;
	config.c_oflag = 0;
	config.c_lflag = 0;
	config.c_cc[VTIME] = 1; // 100 ms read timeout
	config.c_cc[VMIN] = 0;

	if (cfsetispeed(&config, speed) != 0 || cfsetospeed(&config, speed) != 0) {
		PX4_ERR("cfset speed failed (%d)", errno);
		return PX4_ERROR;
	}

	if (tcsetattr(_fd, TCSANOW, &config) != 0) {
		PX4_ERR("tcsetattr failed (%d)", errno);
		return PX4_ERROR;
	}

	_cfg_vmin = config.c_cc[VMIN];
	_cfg_vtime = config.c_cc[VTIME];
	_cfg_cflag = config.c_cflag;

	return PX4_OK;
}

bool SveaEncoderUart::init()
{
	PX4_INFO("listening on %s @ %d", _device, _baudrate);
	PX4_INFO("build_tag=%s", BUILD_TAG);
	_started_us = hrt_absolute_time();
	ScheduleNow();
	return true;
}

void SveaEncoderUart::parse_byte(uint8_t byte)
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
			_state = ParseState::WaitLength;
		}

		break;

	case ParseState::WaitLength:
		_frame_len = byte;

		if (_frame_len != FRAME_PAYLOAD_LEN) {
			_frames_parse_error++;
			_state = ParseState::WaitMagic0;

		} else {
			_payload_pos = 0;
			_state = ParseState::WaitPayload;
		}

		break;

	case ParseState::WaitPayload:
		_payload[_payload_pos++] = byte;

		if (_payload_pos >= FRAME_PAYLOAD_LEN) {
			_state = ParseState::WaitCrc0;
		}

		break;

	case ParseState::WaitCrc0:
		_crc_lsb = byte;
		_state = ParseState::WaitCrc1;
		break;

	case ParseState::WaitCrc1: {
			const uint16_t crc_rx = (uint16_t)_crc_lsb | ((uint16_t)byte << 8);
			uint8_t crc_data[2 + FRAME_PAYLOAD_LEN] {};
			crc_data[0] = _frame_version;
			crc_data[1] = _frame_len;
			memcpy(&crc_data[2], _payload, FRAME_PAYLOAD_LEN);
			const uint16_t crc_calc = crc16_ccitt(crc_data, sizeof(crc_data));

			if (crc_rx == crc_calc) {
				FramePayload payload{};
				memcpy(&payload, _payload, sizeof(payload));

				wheel_distance_s msg{};
				msg.timestamp = hrt_absolute_time();
				msg.timestamp_sample = (uint64_t)payload.time_ms * 1000ULL;
				msg.sequence = payload.sequence;
				msg.left_distance_m = payload.left_distance_m;
				msg.right_distance_m = payload.right_distance_m;
				_wheel_distance_pub.publish(msg);
				_frames_rx++;

			} else {
				_frames_crc_error++;
			}

			_state = ParseState::WaitMagic0;
			break;
		}
	}
}

void SveaEncoderUart::Run()
{
	_run_count++;
	_last_run_us = hrt_absolute_time();

	if (should_exit()) {
		if (_fd >= 0) {
			::close(_fd);
			_fd = -1;
		}

		ScheduleClear();
		exit_and_cleanup(desc);
		return;
	}

	if (_fd < 0) {
		_open_attempts++;

		if (open_serial() != PX4_OK) {
			_open_fails++;
			_last_open_errno = errno;
			ScheduleDelayed(SCHEDULE_INTERVAL_US);
			return;
		}

		_open_ok++;
		_last_open_errno = 0;
	}

	uint8_t buf[128];
	const int n = ::read(_fd, buf, sizeof(buf));
	_last_read_n = n;

	if (n > 0) {
		_bytes_rx += n;
		_read_zero_count = 0;
		_last_read_errno = 0;
		_last_rx_us = _last_run_us;

		if (!_printed_raw_once) {
			char hexbuf[3 * 32 + 1] {};
			const int dump_n = (n < 32) ? n : 32;
			int pos = 0;

			for (int i = 0; i < dump_n && (pos + 4) < (int)sizeof(hexbuf); i++) {
				pos += snprintf(&hexbuf[pos], sizeof(hexbuf) - pos, "%02X%s", buf[i], (i + 1 < dump_n) ? " " : "");
			}

			PX4_WARN("raw_rx_first[%d]=%s", dump_n, hexbuf);
			_printed_raw_once = true;
		}

		for (int i = 0; i < n; i++) {
			parse_byte(buf[i]);
		}

	} else if (n == 0) {
		_read_zero_count++;

	} else {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			_read_err_count++;
			_last_read_errno = errno;
		}
	}

	ScheduleDelayed(SCHEDULE_INTERVAL_US);
}

int SveaEncoderUart::task_spawn(int argc, char *argv[])
{
	SveaEncoderUart *instance = instantiate(argc, argv);

	if (instance == nullptr) {
		return PX4_ERROR;
	}

	desc.object.store(instance);
	desc.task_id = task_id_is_work_queue;

	if (!instance->init()) {
		delete instance;
		desc.object.store(nullptr);
		desc.task_id = -1;
		return PX4_ERROR;
	}

	return PX4_OK;
}

SveaEncoderUart *SveaEncoderUart::instantiate(int argc, char *argv[])
{
	int myoptind = 1;
	const char *myoptarg = nullptr;
	int ch;
	const char *device = "/dev/ttyS0";
	int baudrate = 115200;

	while ((ch = px4_getopt(argc, argv, "d:b:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'd':
			device = myoptarg;
			break;

		case 'b':
			baudrate = atoi(myoptarg);
			break;

		default:
			return nullptr;
		}
	}

	return new SveaEncoderUart(device, baudrate);
}

int SveaEncoderUart::print_status()
{
	const hrt_abstime now = hrt_absolute_time();
	const uint64_t uptime_ms = (_started_us > 0 && now >= _started_us) ? (now - _started_us) / 1000ULL : 0ULL;
	const uint64_t last_run_age_ms = (_last_run_us > 0 && now >= _last_run_us) ? (now - _last_run_us) / 1000ULL : 0ULL;
	const uint64_t last_rx_age_ms = (_last_rx_us > 0 && now >= _last_rx_us) ? (now - _last_rx_us) / 1000ULL : UINT64_MAX;

	PX4_INFO("diag_sched run=%" PRIu32 " uptime_ms=%" PRIu64 " last_run_age_ms=%" PRIu64 " last_rx_age_ms=%s%" PRIu64,
		 _run_count, uptime_ms, last_run_age_ms,
		 (last_rx_age_ms == UINT64_MAX) ? "never/" : "", (last_rx_age_ms == UINT64_MAX) ? 0ULL : last_rx_age_ms);

	PX4_INFO("diag_io n=%d errno=%d read0=%" PRIu32 " readerr=%" PRIu32,
		 _last_read_n, _last_read_errno, _read_zero_count, _read_err_count);

	PX4_INFO("diag_fd flags_before=0x%08x flags_after=0x%08x isatty=%d",
		 _fd_flags_before, _fd_flags_after, _isatty ? 1 : 0);
	PX4_INFO("diag_open attempts=%" PRIu32 " ok=%" PRIu32 " fail=%" PRIu32 " last_open_errno=%d fd=%d",
		 _open_attempts, _open_ok, _open_fails, _last_open_errno, _fd);

	PX4_INFO("diag_cfg tty_vmin=%u tty_vtime=%u tty_cflag=0x%08" PRIx32,
		 (unsigned)_cfg_vmin, (unsigned)_cfg_vtime, _cfg_cflag);

	PX4_INFO("diag_parser state=%u payload_pos=%u frame_v=%u frame_len=%u",
		 (unsigned)_state, (unsigned)_payload_pos, (unsigned)_frame_version, (unsigned)_frame_len);

	PX4_INFO("diag_counters bytes=%" PRIu32 " frames=%" PRIu32 " crc_err=%" PRIu32 " parse_err=%" PRIu32
		 " S=0x53:%" PRIu32 " E=0x45:%" PRIu32 " mav_fe=%" PRIu32 " mav_fd=%" PRIu32,
		 _bytes_rx, _frames_rx, _frames_crc_error, _frames_parse_error,
		 _magic0_hits, _magic1_hits, _mavlink_v1_hits, _mavlink_v2_hits);
	PX4_INFO("device=%s baud=%d", _device, _baudrate);

	char hexbuf[3 * sizeof(_last_bytes) + 1] {};
	int pos = 0;
	const uint8_t count = _last_bytes_len;
	const uint8_t start = (_last_bytes_pos + sizeof(_last_bytes) - count) % sizeof(_last_bytes);

	for (uint8_t i = 0; i < count && (pos + 4) < (int)sizeof(hexbuf); i++) {
		const uint8_t b = _last_bytes[(start + i) % sizeof(_last_bytes)];
		pos += snprintf(&hexbuf[pos], sizeof(hexbuf) - pos, "%02X%s", b, (i + 1 < count) ? " " : "");
	}

	PX4_INFO("last_rx_bytes[%u]=%s", (unsigned)count, count > 0 ? hexbuf : "<none>");
	PX4_INFO("build_tag=%s", BUILD_TAG);
	return PX4_OK;
}

int SveaEncoderUart::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
SVEA encoder UART peripheral ingest.
Reads framed wheel distance samples from an external MCU over UART and publishes `wheel_distance` uORB.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("svea_encoder_uart", "system");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_STRING('d', "/dev/ttyS0", "<file:dev>", "UART device", true);
	PRINT_MODULE_USAGE_PARAM_INT('b', 115200, 9600, 2000000, "Baudrate", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

extern "C" __EXPORT int svea_encoder_uart_main(int argc, char *argv[])
{
	return ModuleBase::main(SveaEncoderUart::desc, argc, argv);
}
