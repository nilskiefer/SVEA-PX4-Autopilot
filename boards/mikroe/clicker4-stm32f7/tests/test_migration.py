"""Exercise migrated board initialization, RC authority and release classification."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[4]


def compile_and_run(source, flags=()):
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp)
        (path / 'test.cpp').write_text(source)
        subprocess.run([shutil.which('c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        *flags, str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)


class MigrationTest(unittest.TestCase):
    def test_saved_airframe_migration_preserves_other_selections(self):
        source = (ROOT / 'boards/mikroe/clicker4-stm32f7/init/rc.board_defaults').read_text()
        block = source[source.index('# Default to the SVEA TRX4 profile'):
                       source.index('# Use USB CDC')]
        for old, expected in [('unset', '51004'), ('0', '51004'), ('51003', '51004'),
                              ('51004', '51004'), ('51002', '51002')]:
            with self.subTest(saved=old):
                script = r'''
set -eu
value="$SAVED_AIRFRAME"
param() {
    case "$1" in
        set-default) if [ "$value" = unset ]; then value="$3"; fi ;;
        compare) [ "$value" = "$3" ] ;;
        set) if [ "$2" = SYS_AUTOSTART ]; then value="$3"; fi ;;
        *) return 1 ;;
    esac
}
''' + block + '\nprintf "%s\\n" "$value"\n'
                result = subprocess.run(['sh', '-c', script], check=True, capture_output=True,
                                        text=True, env=dict(os.environ, SAVED_AIRFRAME=old))
                self.assertEqual(result.stdout.splitlines()[-1], expected)

    def test_new_airframe_id_keeps_upstream_profile_unchanged(self):
        source = ROOT / 'ROMFS/px4fmu_common/init.d/airframes/CMakeLists.txt'
        for board, selected, excluded in [
            ('mikroe_clicker4-stm32f7', '51004_svea_trx4', ''),
            ('px4_fmu-v6x', '51003_hiwonder_ackermann', '51004_svea_trx4'),
        ]:
            with self.subTest(board=board), tempfile.TemporaryDirectory() as temp:
                path = Path(temp)
                script = f'''set(CONFIG_MODULES_ROVER_ACKERMANN ON)
set(PX4_BOARD "{board}")
function(px4_add_romfs_files)
    list(APPEND files ${{ARGV}})
    set(files "${{files}}" PARENT_SCOPE)
endfunction()
include("{source}")
file(WRITE "{path / 'files'}" "${{files}}")
'''
                (path / 'test.cmake').write_text(script)
                subprocess.run([shutil.which('cmake'), '-P', str(path / 'test.cmake')], check=True)
                files = (path / 'files').read_text().split(';')
                self.assertIn(selected, files)
                self.assertIn('51003_hiwonder_ackermann', files)
                if excluded:
                    self.assertNotIn(excluded, files)

    def test_outbound_tunnel_preserves_instances_and_fragment_format(self):
        source = (ROOT / 'src/modules/mavlink/streams/PX4_UORB_TUNNEL.hpp').read_text()
        constants = source[source.index('\tstatic constexpr uint16_t kTunnelPayloadType'):
                           source.index('\n\tstruct TopicState')]
        codec = source[source.index('\tstatic void put_u16'):source.index('\n\tbool send() override')]
        compile_and_run(r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#define MAVLINK_MSG_TUNNEL_FIELD_PAYLOAD_LEN 128
struct orb_metadata { const char *o_name; uint16_t o_size_no_padding; uint32_t message_hash; };
struct mavlink_tunnel_t {
    uint16_t payload_type;
    uint8_t target_system, target_component, payload_length, payload[128];
};
struct Mavlink { int get_channel() const { return 0; } };
std::vector<mavlink_tunnel_t> packets;
void mavlink_msg_tunnel_send_struct(int channel, const mavlink_tunnel_t *packet) {
    assert(channel == 0); packets.push_back(*packet);
}
struct Stream {
    Mavlink link;
    Mavlink *_mavlink = &link;
    uint8_t _sequence = 0;
''' + constants + codec + r'''
};
uint16_t u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
int main() {
    Stream stream;
    uint8_t data[256];
    for (int i = 0; i < 256; ++i) data[i] = i;
    orb_metadata meta{"power_monitor", sizeof(data), 0x12345678};
    for (uint8_t instance = 0; instance < 8; ++instance) {
        packets.clear();
        assert(stream.send_uorb_payload(&meta, instance, data));
        assert(packets.size() == 3);
        unsigned offset = 0;
        for (const auto &packet : packets) {
            const auto *frame = packet.payload;
            assert(packet.payload_type == 0xE001 && packet.payload_length <= 128);
            assert(packet.target_system == 0 && packet.target_component == 0);
            assert(frame[0] == 2 && frame[2] == instance && frame[3] == instance);
            assert(frame[4] == 0x78 && frame[5] == 0x56 && frame[6] == 0x34 && frame[7] == 0x12);
            assert(u16(frame + 8) == 13 && u16(frame + 10) == 256 && u16(frame + 12) == offset);
            assert(std::memcmp(frame + 14, "power_monitor", 13) == 0);
            const unsigned size = packet.payload_length - 27;
            assert(std::memcmp(frame + 27, data + offset, size) == 0);
            offset += size;
        }
        assert(offset == sizeof(data));
    }
    packets.clear();
    assert(!stream.send_uorb_payload(nullptr, 0, data));
    assert(packets.empty());
}
''')

    def test_gpio_reads_select_the_matching_expander(self):
        source = (ROOT / 'src/drivers/gpio/pcal6524/pcal6524.h').read_text()
        reader = source[source.index('class PCAL6524InputReader'):source.index('\nstruct pcal6524_gpio_dev_s')]
        compile_and_run(r'''
#include <cassert>
#include <cstdint>
enum class ORB_ID { gpio_in };
struct gpio_in_s { static constexpr uint8_t MAX_INSTANCES = 8; uint32_t device_id = 0, state = 0; };
namespace uORB {
struct Subscription {
    bool present = false; gpio_in_s value;
    bool copy(gpio_in_s *out) { *out = value; return present; }
};
Subscription inputs[8];
template<typename T, uint8_t N> struct SubscriptionMultiArray {
    explicit SubscriptionMultiArray(ORB_ID) {}
    Subscription *begin() { return inputs; }
    Subscription *end() { return inputs + N; }
};
}
''' + reader + r'''
int main() {
    PCAL6524InputReader primary, secondary;
    primary.dev_id = 22; secondary.dev_id = 23;
    uint32_t value = 0;
    assert(!primary.read(value));
    uORB::inputs[0] = {true, {23, 0x100}};
    uORB::inputs[1] = {true, {22, 0x200}};
    assert(primary.read(value) && value == 0x200);
    assert(secondary.read(value) && value == 0x100);
    uORB::inputs[1].value.state = 0x400;
    assert(primary.read(value) && value == 0x400);
}
''')

    def test_board_owns_i2c_without_general_call_reset(self):
        board = ROOT / 'boards/mikroe/clicker4-stm32f7/src'
        header = (board / 'board_config.h').read_text()
        define = re.search(r'^#define BOARD_I2C_LATEINIT.*$', header, re.M).group()
        source = (board / 'init.c').read_text()
        start = source.index('\tstruct i2c_master_s *i2c =')
        block = source[start:source.index('\n\t/* configure the DMA allocator */', start)]
        common = (ROOT / 'platforms/nuttx/src/px4/common/px4_init.cpp').read_text()
        start = common.index('#if defined(CONFIG_I2C) && !defined(BOARD_I2C_LATEINIT)')
        guard = common[start:common.index('#endif', start) + len('#endif')]
        source = r'''
#include <cassert>
#include <cerrno>
#include <cstddef>
#define CONFIG_I2C 1
''' + define + r'''
struct i2c_master_s {} device;
bool missing = false;
int resets = 0, releases = 0, broadcast_resets = 0, reset_result = 0;
i2c_master_s *px4_i2cbus_initialize(int bus) { assert(bus == 2); return missing ? nullptr : &device; }
void px4_i2cbus_uninitialize(i2c_master_s *bus) { assert(bus == &device); ++releases; }
int I2C_RESET(i2c_master_s *bus) { assert(bus == &device); ++resets; return reset_result; }
void px4_platform_i2c_init() { ++broadcast_resets; }
void px4_platform_init() {
''' + guard + r'''
}
int board_init() {
    px4_platform_init();
''' + block + r'''
    return 0;
}
int main() {
    assert(board_init() == 0 && releases == 1 && broadcast_resets == 0);
#ifdef CONFIG_I2C_RESET
    assert(resets == 1);
    reset_result = -EIO;
    assert(board_init() == -EIO && releases == 2);
#else
    assert(resets == 0);
#endif
    missing = true;
    assert(board_init() == -ENODEV && broadcast_resets == 0);
}
'''
        for flags in ((), ('-DCONFIG_I2C_RESET=1',)):
            with self.subTest(flags=flags):
                compile_and_run(source, flags)

    def test_physical_kill_remains_authoritative_with_companion_selected(self):
        source = (ROOT / 'src/modules/manual_control/ManualControl.cpp').read_text()
        start = source.index('\tif (switches_updated) {\n\t\tif (_previous_switches_initialized)',
                             source.index('// Keep the physical RC kill') - 100)
        end = source.index('\n}\n\nvoid ManualControl::updateParams()', start)
        block = source[start:end]
        compile_and_run(r'''
#include <cassert>
#include <initializer_list>
struct manual_control_switches_s {
    enum { SWITCH_POS_OFF, SWITCH_POS_ON };
    int kill_switch = SWITCH_POS_OFF, termination_switch = SWITCH_POS_OFF;
};
struct manual_control_setpoint_s { enum { SOURCE_RC, SOURCE_MAVLINK_0 }; };
struct action_request_s { enum { ACTION_KILL, ACTION_UNKILL, ACTION_TERMINATION, SOURCE_RC_SWITCH }; };
struct Setpoint { bool valid = true; int data_source = manual_control_setpoint_s::SOURCE_MAVLINK_0; };
struct Selector { Setpoint point; const Setpoint &setpoint() const { return point; } };
struct Control {
    Selector _selector;
    bool _previous_switches_initialized = false;
    manual_control_switches_s _previous_switches;
    int kills = 0, unkills = 0, terminations = 0;
    void sendActionRequest(int action, int source) {
        assert(source == action_request_s::SOURCE_RC_SWITCH);
        if (action == action_request_s::ACTION_KILL) ++kills;
        if (action == action_request_s::ACTION_UNKILL) ++unkills;
        if (action == action_request_s::ACTION_TERMINATION) ++terminations;
    }
    void update(manual_control_switches_s switches, bool switches_updated = true) {
''' + block + r'''
    }
};
int main() {
    for (int selected : {manual_control_setpoint_s::SOURCE_RC, manual_control_setpoint_s::SOURCE_MAVLINK_0}) {
        Control c; c._selector.point.data_source = selected;
        c.update({});
        c.update({manual_control_switches_s::SWITCH_POS_ON, manual_control_switches_s::SWITCH_POS_ON});
        assert(c.kills == 1 && c.terminations == (selected == manual_control_setpoint_s::SOURCE_RC));
        c.update({manual_control_switches_s::SWITCH_POS_ON, manual_control_switches_s::SWITCH_POS_ON});
        assert(c.kills == 1);
        c.update({});
        assert(c.unkills == 1);
    }
}
''')

    def test_rc_switch_gates_both_companion_control_messages(self):
        source = (ROOT / 'src/modules/mavlink/mavlink_receiver.cpp').read_text()
        for name, end in (('handle_message_manual_control', '\tmanual_control_setpoint_s manual_control_setpoint{};'),
                          ('handle_message_rc_channels_override', '\t// fill uORB message')):
            start = source.index('// SVEA: accept companion control', source.index('MavlinkReceiver::' + name))
            block = source[start:source.index(end, start)]
            with self.subTest(handler=name):
                compile_and_run(r'''
#include <cassert>
#include <initializer_list>
struct manual_control_switches_s { enum { MODE_SLOT_1 = 1 }; int mode_slot = 0; };
struct Subscription {
    bool present = false; int slot = 0;
    bool copy(manual_control_switches_s *value) { value->mode_slot = slot; return present; }
};
struct Receiver {
    Subscription _manual_control_switches_sub;
    bool accepted = false;
    void receive() {
''' + block + r'''
        accepted = true;
    }
};
int main() {
    Receiver r;
    r.receive(); assert(!r.accepted);
    r._manual_control_switches_sub = {true, 2};
    r.receive(); assert(!r.accepted);
    r._manual_control_switches_sub.slot = 1;
    r.receive(); assert(r.accepted);
}
''')

    def test_release_candidates_are_not_published_as_stable(self):
        workflow = (ROOT / '.github/workflows/svea_pmb_mikroe_build.yml').read_text()
        step = workflow.split('      - name: Resolve release channel\n', 1)[1].split('\n      - name:', 1)[0]
        script = textwrap.dedent(step.split('        run: |\n', 1)[1])
        for name, candidate in [('v1.18.0-1.1.0-rc1', True), ('v1.18.0-1.1.0', False)]:
            with self.subTest(tag=name), tempfile.TemporaryDirectory() as temp:
                output = Path(temp) / 'output'
                env = dict(os.environ, GITHUB_REF='refs/tags/' + name, GITHUB_REF_NAME=name, GITHUB_OUTPUT=str(output))
                subprocess.run(['bash', '-c', script], env=env, check=True)
                values = dict(line.split('=', 1) for line in output.read_text().splitlines())
                self.assertEqual(values['is_prerelease'], str(candidate).lower())
                self.assertEqual(values['make_latest'], str(not candidate).lower())
                self.assertEqual(values['retag_latest'], 'false')
        with tempfile.TemporaryDirectory() as temp:
            env = dict(os.environ, GITHUB_REF='refs/tags/v1.18.0-rc1', GITHUB_REF_NAME='v1.18.0-rc1',
                       GITHUB_OUTPUT=str(Path(temp) / 'output'))
            result = subprocess.run(['bash', '-c', script], env=env, capture_output=True)
            self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
