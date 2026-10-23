"""Check the actual UART setup against a host pseudo-terminal.

Run: python3 -m unittest discover -s src/modules/svea_peripheral_mcu/tests -v
Requires a POSIX host and a C++ compiler.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'svea_peripheral_mcu.cpp'
HARNESS = r'''
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
// macOS omits this NuttX baud constant; the test uses B115200 only.
#ifndef B460800
#define B460800 460800
#endif
#define PX4_ERROR -1
#define PX4_OK 0
#define PX4_ERR(...) fprintf(stderr, __VA_ARGS__)
class SveaPeripheralMcu {
public:
    int open_serial();
    int _fd{-1}, _baudrate{115200};
    char _device[256]{};
    bool _isatty{false};
    int _fd_flags_before{0}, _fd_flags_after{0};
    uint8_t _cfg_vmin{0}, _cfg_vtime{0};
    uint32_t _cfg_cflag{0};
};
OPEN_SERIAL
int main() {
    const int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
    SveaPeripheralMcu bridge;
    const char *slave = ptsname(master);
    assert(slave != nullptr && strlen(slave) < sizeof(bridge._device));
    strcpy(bridge._device, slave);
    assert(bridge.open_serial() == PX4_OK);
    assert((fcntl(bridge._fd, F_GETFL) & O_NONBLOCK) != 0);
    assert(bridge._cfg_vmin == 0 && bridge._cfg_vtime == 0);
    struct termios configured{};
    assert(tcgetattr(bridge._fd, &configured) == 0);
    assert(configured.c_iflag == 0 && configured.c_oflag == 0 && configured.c_lflag == 0);
    assert((configured.c_cflag & CSIZE) == CS8);
    assert((configured.c_cflag & (PARENB | CSTOPB | CRTSCTS)) == 0);
    assert(cfgetispeed(&configured) == B115200);
    uint8_t buffer[128];
    // Idle reads must return immediately, even with no ESP connected.
    for (int i = 0; i < 10; i++) {
        const int n = read(bridge._fd, buffer, sizeof(buffer));
        assert(n == 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)));
    }
    // A partial binary frame must return without waiting for all 128 bytes.
    const uint8_t fragment[] = {0x53, 0x45, 0x02, 0x00, 0xff};
    assert(write(master, fragment, sizeof(fragment)) == sizeof(fragment));
    struct pollfd ready{bridge._fd, POLLIN, 0};
    assert(poll(&ready, 1, 1000) == 1);
    const int n = read(bridge._fd, buffer, sizeof(buffer));
    assert(n == sizeof(fragment) && memcmp(buffer, fragment, sizeof(fragment)) == 0);
    close(bridge._fd);
    close(master);
}
'''


class UartTest(unittest.TestCase):
    def test_idle_and_partial_binary_reads_do_not_block(self):
        source = SOURCE.read_text()
        start = source.index('int SveaPeripheralMcu::open_serial()')
        end = source.index('\nbool SveaPeripheralMcu::init()', start)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            test = path / 'test.cpp'
            test.write_text(HARNESS.replace('OPEN_SERIAL', source[start:end]))
            subprocess.run([shutil.which('c++'), '-std=c++17', str(test), '-o', str(path / 'test')],
                           check=True)
            subprocess.run([str(path / 'test')], check=True, timeout=3,
                           capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
