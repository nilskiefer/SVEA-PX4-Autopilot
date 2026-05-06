#include "svea_peripheral_mcu.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int SveaPeripheralMcu::open_serial()
{
	close_serial();
	_fd = ::open(_device, O_RDWR | O_NOCTTY);

	if (_fd < 0) {
		PX4_ERR("open failed: %s (%d)", _device, errno);
		return PX4_ERROR;
	}

	_isatty = (::isatty(_fd) == 1);

	if (!_isatty) {
		PX4_ERR("fd for %s is not a TTY device", _device);
		close_serial();
		return PX4_ERROR;
	}

	_fd_flags_before = ::fcntl(_fd, F_GETFL, 0);

	if (_fd_flags_before < 0) {
		PX4_ERR("fcntl(F_GETFL) failed (%d)", errno);
		close_serial();
		return PX4_ERROR;
	}

	if ((_fd_flags_before & O_NONBLOCK) != 0) {
		if (::fcntl(_fd, F_SETFL, _fd_flags_before & ~O_NONBLOCK) != 0) {
			PX4_ERR("fcntl(F_SETFL clear O_NONBLOCK) failed (%d)", errno);
			close_serial();
			return PX4_ERROR;
		}
	}

	_fd_flags_after = ::fcntl(_fd, F_GETFL, 0);

	if (_fd_flags_after < 0) {
		PX4_ERR("fcntl(F_GETFL after) failed (%d)", errno);
		close_serial();
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
		close_serial();
		return PX4_ERROR;
	}

	struct termios config {};

	if (tcgetattr(_fd, &config) != 0) {
		PX4_ERR("tcgetattr failed (%d)", errno);
		close_serial();
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
	config.c_cc[VTIME] = 1;
	config.c_cc[VMIN] = 0;

	if (cfsetispeed(&config, speed) != 0 || cfsetospeed(&config, speed) != 0) {
		PX4_ERR("cfset speed failed (%d)", errno);
		close_serial();
		return PX4_ERROR;
	}

	if (tcsetattr(_fd, TCSANOW, &config) != 0) {
		PX4_ERR("tcsetattr failed (%d)", errno);
		close_serial();
		return PX4_ERROR;
	}

	_cfg_vmin = config.c_cc[VMIN];
	_cfg_vtime = config.c_cc[VTIME];
	_cfg_cflag = config.c_cflag;

	return PX4_OK;
}

void SveaPeripheralMcu::close_serial()
{
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

void SveaPeripheralMcu::Run()
{
	_run_count++;
	_last_run_us = hrt_absolute_time();

	if (should_exit()) {
		close_serial();
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
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
			_read_err_count++;
			_last_read_errno = errno;

			if (errno == EBADF) {
				close_serial();
			}
		}
	}

	ScheduleDelayed(SCHEDULE_INTERVAL_US);
}
