"""Exercise the actual driver with simulated I2C, GPIO and parameter backends."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'svea_mppt.cpp'
STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#include <limits>
#define PX4_OK 0
#define PX4_ERROR -1
template<typename... Args> void log(const char *, Args...) {}
#define PX4_ERR(...) log(__VA_ARGS__)
#define PX4_INFO(...) log(__VA_ARGS__)
#define GPIO_INPUT_PIN 0
#define GPIO_OUTPUT_PIN 1
#define GPIOC_SETPINTYPE 2
#define O_RDWR 2
using param_t = int;
constexpr int PARAM_INVALID = -1;
int enable_value = 0, wiper = 64;
float voltage_value = 18.f;
bool bus_error = false, gpio_error = false, wrong_readback = false, param_error = false;
std::vector<int> events;
param_t param_find(const char *name) { return !strcmp(name, "SVEA_MPPT_EN") ? 0 : 1; }
int param_get(param_t h, void *v) {
    if (h == 0) { *static_cast<int32_t *>(v) = enable_value; }
    else { *static_cast<float *>(v) = voltage_value; }
    return param_error ? -1 : 0;
}
int open(const char *path, int) { assert(!strcmp(path, "/dev/gpio6")); return 5; }
int close(int) { return 0; }
int ioctl(int fd, int cmd, int type) {
    assert(fd == 5 && cmd == GPIOC_SETPINTYPE);
    events.push_back(type);
    return gpio_error ? -1 : 0;
}
struct I2CSPIDriverConfig {};
struct I2CSPIDriverBase {
    virtual ~I2CSPIDriverBase() = default;
    virtual void print_status() {}
    virtual void exit_and_cleanup() {}
    bool should_exit() const { return exiting; }
    void ScheduleOnInterval(int, int) {}
    void ScheduleClear() { stopped = true; }
    bool exiting = false, stopped = false;
};
template<class T> struct I2CSPIDriver : I2CSPIDriverBase {
    explicit I2CSPIDriver(const I2CSPIDriverConfig &) {}
};
namespace device {
struct I2C {
    explicit I2C(const I2CSPIDriverConfig &) {}
    virtual ~I2C() = default;
    virtual int probe() { return 0; }
    virtual int init() { return probe(); }
    int transfer(const uint8_t *send, unsigned len, uint8_t *recv, unsigned recv_len) {
        if (bus_error) { return -1; }
        if (send) {
            assert(len == 2 && send[0] == 0 && send[1] <= 127 && recv_len == 0);
            wiper = send[1]; events.push_back(10);
        } else {
            assert(len == 0 && recv_len == 1);
            *recv = wrong_readback ? 255 : wiper; events.push_back(11);
        }
        return 0;
    }
};
}
'''
CHECKS = r'''
void reset() {
    events.clear(); enable_value = 0; voltage_value = 18.f; wiper = 64;
    bus_error = gpio_error = wrong_readback = param_error = false;
}
int main() {
    const I2CSPIDriverConfig config{};
    assert(SveaMppt::voltage_to_code(18.f) == 4);
    assert(fabsf(SveaMppt::nominal_voltage(4) - 18.0686f) < 0.001f);
    assert(fabsf(SveaMppt::nominal_voltage(5) - 17.8535f) < 0.001f);
    assert(SveaMppt::voltage_to_code(18.98f) == 0);
    assert(SveaMppt::voltage_to_code(7.66f) == 127);
    assert(SveaMppt::voltage_to_code(20.f) == -1);
    assert(SveaMppt::voltage_to_code(std::numeric_limits<float>::quiet_NaN()) == -1);
    assert(SveaMppt::voltage_to_code(std::numeric_limits<float>::infinity()) == -1);
    for (int step = 0; step <= 100; ++step) {
        const float target = 7.66f + (18.98f - 7.66f) * step / 100.f;
        const int chosen = SveaMppt::voltage_to_code(target);
        assert(chosen >= 0 && chosen <= 127);
        for (int code = 0; code <= 127; ++code) {
            assert(fabsf(SveaMppt::nominal_voltage(chosen) - target)
                   <= fabsf(SveaMppt::nominal_voltage(code) - target));
        }
    }
    reset();
    {
        SveaMppt d(config); assert(d.init() == 0);
        assert(events.front() == GPIO_INPUT_PIN);
        events.clear(); d.RunImpl();
        assert(wiper == 4 && !d._enabled && !d._fault);
        enable_value = 1; voltage_value = 7.66f; events.clear(); d.RunImpl();
        assert((events == std::vector<int>{10, 11, GPIO_OUTPUT_PIN}));
        assert(wiper == 127 && d._enabled);
        enable_value = 0; d.RunImpl(); assert(!d._enabled);
        enable_value = 1; voltage_value = 20.f; d.RunImpl();
        assert(d._fault && d.stopped && !d._enabled);
        voltage_value = 18.f; d.RunImpl(); assert(d._fault && !d._enabled);
    }
    for (int mode = 0; mode < 7; ++mode) {
        reset(); SveaMppt d(config); assert(d.init() == 0);
        enable_value = 1; d.RunImpl(); assert(d._enabled);
        if (mode == 0) { bus_error = true; }
        if (mode == 1) { wrong_readback = true; }
        if (mode == 2) { voltage_value = 7.f; }
        if (mode == 3) { enable_value = 2; }
        if (mode == 4) { param_error = true; }
        if (mode == 5) { wiper = 64; } // Digipot reset must latch a fault, not silently re-enable.
        if (mode == 6) { voltage_value = 17.f; bus_error = true; } // Failed write while adjusting.
        d.RunImpl(); assert(d._fault && !d._enabled && d.stopped);
        assert(events.back() == GPIO_INPUT_PIN);
    }
    reset();
    { SveaMppt d(config); assert(d.init() == 0); enable_value = 1;
      gpio_error = true; d.RunImpl(); assert(d._fault); }
    reset();
    { SveaMppt d(config); assert(d.init() == 0); enable_value = 1; d.RunImpl();
      d.exit_and_cleanup(); assert(!d._enabled); }
    reset();
    { SveaMppt d(config); bus_error = true; assert(d.init() != 0);
      assert(events.front() == GPIO_INPUT_PIN); }
}
'''


class DriverTest(unittest.TestCase):
    def test_control_and_faults(self):
        compiler = shutil.which('c++')
        self.assertIsNotNone(compiler, 'C++ compiler required')
        source = SOURCE.read_text()
        # Keep the production class, init, transfer, control and fault handling intact.
        source = source[source.index('class SveaMppt'):source.index('I2CSPIDriverBase *SveaMppt::instantiate')]
        source = source.replace('private:', 'public:')
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / 'test.cpp'
            binary = Path(temp) / 'test'
            cpp.write_text(STUBS + source + CHECKS)
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
