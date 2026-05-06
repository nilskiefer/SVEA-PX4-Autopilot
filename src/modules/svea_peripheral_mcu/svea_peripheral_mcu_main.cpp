#include "svea_peripheral_mcu.hpp"

#include <px4_platform_common/getopt.h>
#include <px4_platform_common/log.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern "C" __EXPORT int svea_peripheral_mcu_main(int argc, char *argv[]);

SveaPeripheralMcu::SveaPeripheralMcu(const char *device, int baudrate) :
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::lp_default),
	_baudrate(baudrate)
{
	strncpy(_device, device, sizeof(_device) - 1);
	_device[sizeof(_device) - 1] = '\0';
}

SveaPeripheralMcu::~SveaPeripheralMcu()
{
	close_serial();
}

bool SveaPeripheralMcu::init()
{
	if (message_route_count() > MAX_MESSAGE_ROUTES) {
		PX4_ERR("route table too large: %u > %u", (unsigned)message_route_count(), (unsigned)MAX_MESSAGE_ROUTES);
		return false;
	}

	PX4_INFO("listening on %s @ %d", _device, _baudrate);
	PX4_INFO("build_tag=%s", BUILD_TAG);
	_started_us = hrt_absolute_time();
	ScheduleNow();
	return true;
}

int SveaPeripheralMcu::task_spawn(int argc, char *argv[])
{
	SveaPeripheralMcu *instance = instantiate(argc, argv);

	if (instance == nullptr) {
		return PX4_ERROR;
	}

	_object.store(instance);
	_task_id = task_id_is_work_queue;

	if (!instance->init()) {
		delete instance;
		_object.store(nullptr);
		_task_id = -1;
		return PX4_ERROR;
	}

	return PX4_OK;
}

SveaPeripheralMcu *SveaPeripheralMcu::instantiate(int argc, char *argv[])
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

	return new SveaPeripheralMcu(device, baudrate);
}

int SveaPeripheralMcu::print_status()
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

	PX4_INFO("diag_parser state=%u payload_pos=%u frame_v=%u msg_id=%u frame_len=%u",
		 (unsigned)_state, (unsigned)_payload_pos, (unsigned)_frame_version,
		 (unsigned)_frame_msg_id, (unsigned)_frame_len);

	PX4_INFO("diag_counters bytes=%" PRIu32 " frames=%" PRIu32
		 " unknown_msgid=%" PRIu32 " crc_err=%" PRIu32 " parse_err=%" PRIu32
		 " S=0x53:%" PRIu32 " E=0x45:%" PRIu32 " mav_fe=%" PRIu32 " mav_fd=%" PRIu32,
		 _bytes_rx, _frames_rx, _frames_unknown_msgid,
		 _frames_crc_error, _frames_parse_error,
		 _magic0_hits, _magic1_hits, _mavlink_v1_hits, _mavlink_v2_hits);

	for (size_t i = 0; i < message_route_count(); i++) {
		const MessageRoute &route = kMessageRoutes[i];

		PX4_INFO("diag_route msg_id=%u name=%s expected_len=%u frames=%" PRIu32,
			 (unsigned)route.msg_id, route.name, (unsigned)route.expected_payload_len, _route_frames[i]);
	}

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

int SveaPeripheralMcu::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
SVEA peripheral MCU UART ingest.
Reads framed peripheral samples from an external MCU over UART and publishes typed uORB.

Current routes:
- msg_id=1 -> `wheel_distance`
- msg_id=2 -> `wheel_encoders`

To add support for a new uORB topic, add a payload struct + handler + route entry.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("svea_peripheral_mcu", "system");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_STRING('d', "/dev/ttyS0", "<file:dev>", "UART device", true);
	PRINT_MODULE_USAGE_PARAM_INT('b', 115200, 9600, 2000000, "Baudrate", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

extern "C" __EXPORT int svea_peripheral_mcu_main(int argc, char *argv[])
{
	return SveaPeripheralMcu::main(argc, argv);
}
