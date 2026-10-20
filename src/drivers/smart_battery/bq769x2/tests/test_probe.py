"""Compile the real protocol and probe against a simulated BQ769x2 I2C device."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#include "bq769x2_protocol.h"
#include "bq769x2_registers.h"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <vector>
#include <cstring>
#define PX4_OK 0
#define PX4_ERROR -1
#define PARAM_INVALID -1
#define PX4_ERR(...) ((void)0)
#define PX4_INFO(...) ((void)0)
void px4_usleep(unsigned) {}
int saved_crc = -1;
bool param_error = false;
int param_set(int handle, const int32_t *v) {
    assert(handle == 1); if (param_error) { return -1; } saved_crc = *v; return 0;
}
uint8_t crc(const std::vector<uint8_t> &data) {
    uint8_t value = 0;
    for (uint8_t byte : data) {
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit) { value = (value & 0x80) ? (value << 1) ^ 7 : value << 1; }
    }
    return value;
}
struct Chip : BQ769x2ProtocolBus {
    bool use_crc = false, short_hw = false, bad_checksum = false;
    uint16_t command = 0;
    std::vector<uint8_t> payload() {
        if (command == BQ769X2_SUBCMD_DEVICE_NUMBER) { return {0x95, 0x76}; }
        if (command == BQ769X2_SUBCMD_FW_VERSION) { return {0x76, 0x95, 0x12, 0x34, 0x56, 0x78}; }
        if (command == BQ769X2_SUBCMD_HW_VERSION) { return short_hw ? std::vector<uint8_t>{0x23} : std::vector<uint8_t>{0x23, 0x01}; }
        assert(false); return {};
    }
    int busTransfer(const uint8_t *send, unsigned nsend, uint8_t *recv, unsigned nrecv) override {
        if (!nrecv) {
            if (nsend != (use_crc ? 5u : 3u) || send[0] != 0x3e) { return -EIO; }
            const uint8_t lo = send[1], hi = send[use_crc ? 3 : 2];
            if (use_crc && (send[2] != crc({0x10, 0x3e, lo}) || send[4] != crc({hi}))) { return -EIO; }
            command = lo | (hi << 8); return 0;
        }
        assert(nsend == 1);
        const auto p = send[0] == BQ769X2_CMD_BATTERY_STATUS ? std::vector<uint8_t>{0, 0} : payload();
        std::vector<uint8_t> out;
        switch (send[0]) {
        case BQ769X2_CMD_BATTERY_STATUS: out = p; break;
        case 0x3e: out = {uint8_t(command), uint8_t(command >> 8)}; break;
        case 0x61: out = {uint8_t(p.size() + 4)}; break;
        case 0x40: out = p; break;
        case 0x60: {
            uint8_t sum = uint8_t(command) + uint8_t(command >> 8);
            for (auto b : p) { sum += b; }
            out = {uint8_t(~sum)};
            if (bad_checksum) { out[0] ^= 1; }
            break;
        }
        default: return -EIO;
        }
        if (nrecv != out.size() * (use_crc ? 2 : 1)) { return -EIO; }
        for (unsigned i = 0; i < out.size(); ++i) {
            if (!use_crc) { recv[i] = out[i]; }
            else { recv[2*i] = out[i]; recv[2*i+1] = i ? crc({out[i]}) : crc({0x10, send[0], 0x11, out[i]}); }
        }
        return 0;
    }
};
struct BQ769x2 {
    explicit BQ769x2(BQ769x2Protocol &protocol) : _protocol(protocol) {}
    int probe();
    BQ769x2Protocol &_protocol;
    bool _crc_param_enabled = false, _connected = false;
    int _param_crc = 1;
    uint16_t _device_number = 0, _hw_version = 0, _fw_version = 0, _fw_build = 0;
};
'''
CHECKS = r'''
int main() {
    for (bool use_crc : {false, true}) {
        Chip chip; chip.use_crc = use_crc;
        BQ769x2Protocol protocol(chip, 8, use_crc);
        BQ769x2 device(protocol); device._crc_param_enabled = use_crc;
        saved_crc = -1;
        assert(device.probe() == 0 && device._connected);
        assert(device._hw_version == 0x0123);
        assert(device._fw_version == 0x1234 && device._fw_build == 0x5678);
        assert(saved_crc == -1);
    }
    Chip chip; chip.use_crc = true;
    BQ769x2Protocol protocol(chip, 8, false);
    BQ769x2 device(protocol);
    saved_crc = -1;
    assert(device.probe() == 0 && device._crc_param_enabled && saved_crc == 1);
    uint16_t hw = 0; assert(protocol.subcommandReadU2(BQ769X2_SUBCMD_HW_VERSION, hw) == 0);
    chip.short_hw = true; device._connected = false;
    assert(device.probe() != 0 && !device._connected); // No partial-probe acceptance.
    chip.short_hw = false; chip.bad_checksum = true;
    assert(device.probe() != 0 && !device._connected);
    Chip plain; BQ769x2Protocol plain_protocol(plain, 8, true);
    BQ769x2 plain_device(plain_protocol); plain_device._crc_param_enabled = true;
    saved_crc = -1;
    assert(plain_device.probe() == 0 && !plain_device._crc_param_enabled && saved_crc == 0);
    chip.bad_checksum = false; device._crc_param_enabled = false; param_error = true;
    assert(device.probe() != 0 && !device._connected); // Cannot accept an uncorrected CRC parameter.
}
'''


class ProbeTest(unittest.TestCase):
    def test_metadata_crc_and_failures(self):
        source = (ROOT / 'bq769x2.cpp').read_text()
        probe = source[source.index('int BQ769x2::probe()'):source.index('bool BQ769x2::configMatchesDesired()')]
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp)
            stubs = p / 'px4_platform_common'
            stubs.mkdir()
            (stubs / 'defines.h').write_text('#define PX4_OK 0\n')
            (stubs / 'time.h').write_text('void px4_usleep(unsigned);\n')
            (p / 'test.cpp').write_text(HARNESS + probe + CHECKS)
            subprocess.run([shutil.which('c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I', str(p), '-I', str(ROOT), str(p / 'test.cpp'),
                            str(ROOT / 'bq769x2_protocol.cpp'), '-o', str(p / 'test')], check=True)
            subprocess.run([str(p / 'test')], check=True)


if __name__ == '__main__':
    unittest.main()
