/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <px4_platform_common/getopt.h>
#include <px4_platform_common/log.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>

#include <drivers/drv_hrt.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/ioexpander/gpio.h>
#include <uORB/Subscription.hpp>
#include <uORB/topics/actuator_armed.h>
#include <unistd.h>

extern "C" __EXPORT int svea_power_gate_main(int argc, char *argv[]);

class SveaPowerGate : public ModuleBase, public px4::ScheduledWorkItem {
public:
    static Descriptor desc;

    SveaPowerGate() : ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default) {}
    ~SveaPowerGate() override = default;

    static int task_spawn(int argc, char *argv[]);
    static SveaPowerGate *instantiate(int argc, char *argv[]);
    static int custom_command(int argc, char *argv[]) { return print_usage("unknown command"); }
    static int print_usage(const char *reason = nullptr);

    void Run() override;
    int print_status() override;

private:
    static constexpr const char *kEscEnDev = "/dev/gpio9";
    static constexpr const char *kServoEnDev = "/dev/gpio10";
    static constexpr useconds_t kRailStaggerUsON = 500000;
    static constexpr useconds_t kRailStaggerUsOFF = 50000;
    static constexpr uint32_t kPollIntervalUs = 200000;
    static constexpr hrt_abstime kReassertIntervalUs = 200000;
    static constexpr hrt_abstime kHeartbeatIntervalUs = 2000000;

    int write_gpio(const char *dev, bool high);
    void apply_power(bool on);

    uORB::Subscription _actuator_armed_sub{ORB_ID(actuator_armed)};
    bool _requested_on{false};
    bool _rails_on{false};
    hrt_abstime _last_apply{0};
    hrt_abstime _last_heartbeat{0};
    uint32_t _run_count{0};
};

ModuleBase::Descriptor SveaPowerGate::desc{task_spawn, custom_command, print_usage};

int SveaPowerGate::write_gpio(const char *dev, bool high) {
    PX4_INFO("gpio write begin: dev=%s target=%d", dev, high ? 1 : 0);
    int fd = open(dev, O_RDWR);

    if (fd < 0) {
        PX4_ERR("open %s failed (%d)", dev, errno);
        return -errno;
    }

    int ret = ioctl(fd, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);

    if (ret != 0) {
        const int err = errno;
        PX4_ERR("setpintype %s failed (%d)", dev, err);
        close(fd);
        return -err;
    }

    ret = ioctl(fd, GPIOC_WRITE, high ? 1 : 0);

    if (ret != 0) {
        const int err = errno;
        PX4_ERR("write %s=%d failed (%d)", dev, high ? 1 : 0, err);
        close(fd);
        return -err;
    }

    close(fd);
    PX4_INFO("gpio write ok: dev=%s value=%d", dev, high ? 1 : 0);
    return PX4_OK;
}

void SveaPowerGate::apply_power(bool on) {
    PX4_INFO("apply_power begin: request=%d prev_requested=%d rails_on=%d",
             on ? 1 : 0, _requested_on ? 1 : 0, _rails_on ? 1 : 0);
    _requested_on = on;
    _last_apply = hrt_absolute_time();

    int servo = PX4_ERROR;
    int esc = PX4_ERROR;

    if (on) {
        // Enable servo rail first, then ESC rail after a short delay.
        servo = write_gpio(kServoEnDev, true);
        usleep(kRailStaggerUsON);
        esc = write_gpio(kEscEnDev, true);

    } else {
        // Disable ESC rail first, then servo rail after a short delay.
        esc = write_gpio(kEscEnDev, false);
        usleep(kRailStaggerUsOFF);
        servo = write_gpio(kServoEnDev, false);
    }

    if (esc == PX4_OK && servo == PX4_OK) {
        _rails_on = on;
        PX4_INFO("power rails: ESC=%d ServoTPS=%d", on ? 1 : 0, on ? 1 : 0);

    } else {
        PX4_WARN("apply_power incomplete: request=%d esc_ret=%d servo_ret=%d",
                 on ? 1 : 0, esc, servo);
    }
}

void SveaPowerGate::Run() {
    _run_count++;

    if (should_exit()) {
        PX4_INFO("Run: should_exit=1, forcing rails off");
        ScheduleClear();
        apply_power(false);
        exit_and_cleanup(desc);
        return;
    }

    if (_actuator_armed_sub.updated()) {
        actuator_armed_s actuator_armed{};

        if (_actuator_armed_sub.copy(&actuator_armed)) {
            // Rails may only stay enabled while fully armed and not killed/locked down.
            const bool should_enable = actuator_armed.armed && !actuator_armed.kill && !actuator_armed.lockdown && !actuator_armed.termination;
            PX4_INFO("armed update: armed=%d prearmed=%d ready=%d lockdown=%d manual_lockdown=%d force_failsafe=%d in_esc_cal=%d soft_stop=%d kill=%d term=%d -> should_enable=%d",
                     actuator_armed.armed ? 1 : 0,
                     actuator_armed.prearmed ? 1 : 0,
                     actuator_armed.ready_to_arm ? 1 : 0,
                     actuator_armed.lockdown ? 1 : 0,
                     actuator_armed.manual_lockdown ? 1 : 0,
                     actuator_armed.force_failsafe ? 1 : 0,
                     actuator_armed.in_esc_calibration_mode ? 1 : 0,
                     actuator_armed.soft_stop ? 1 : 0,
                     actuator_armed.kill ? 1 : 0,
                     actuator_armed.termination ? 1 : 0,
                     should_enable ? 1 : 0);

            if (should_enable != _requested_on) {
                PX4_INFO("state change: requested_on %d -> %d", _requested_on ? 1 : 0, should_enable ? 1 : 0);
                apply_power(should_enable);
            }
        }
    }

    // If any GPIO write failed earlier, keep re-asserting the requested state.
    if (_requested_on != _rails_on && hrt_elapsed_time(&_last_apply) >= kReassertIntervalUs) {
        PX4_WARN("reasserting rails: requested_on=%d rails_on=%d elapsed_us=%llu",
                 _requested_on ? 1 : 0, _rails_on ? 1 : 0,
                 (unsigned long long)hrt_elapsed_time(&_last_apply));
        apply_power(_requested_on);
    }

    if ((_last_heartbeat == 0) || (hrt_elapsed_time(&_last_heartbeat) >= kHeartbeatIntervalUs)) {
        _last_heartbeat = hrt_absolute_time();
        PX4_INFO("heartbeat: run_count=%u requested_on=%d rails_on=%d last_apply_us_ago=%llu",
                 _run_count, _requested_on ? 1 : 0, _rails_on ? 1 : 0,
                 (unsigned long long)hrt_elapsed_time(&_last_apply));
    }

    ScheduleDelayed(kPollIntervalUs);
}

int SveaPowerGate::task_spawn(int argc, char *argv[]) {
    PX4_INFO("task_spawn: argc=%d", argc);
    SveaPowerGate *instance = instantiate(argc, argv);

    if (instance == nullptr) {
        return PX4_ERROR;
    }

    desc.object.store(instance);
    desc.task_id = task_id_is_work_queue;

    // Initialize to safe state until we observe arming.
    PX4_INFO("task_spawn: initialize safe state (rails off), schedule now");
    instance->apply_power(false);
    instance->ScheduleNow();
    return PX4_OK;
}

SveaPowerGate *SveaPowerGate::instantiate(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    return new SveaPowerGate();
}

int SveaPowerGate::print_status() {
    PX4_INFO("requested_on=%d rails_on=%d run_count=%u esc_dev=%s servo_dev=%s",
             _requested_on ? 1 : 0, _rails_on ? 1 : 0, _run_count, kEscEnDev, kServoEnDev);
    return PX4_OK;
}

int SveaPowerGate::print_usage(const char *reason) {
    if (reason) {
        PX4_WARN("%s", reason);
    }

    PRINT_MODULE_DESCRIPTION(
        R"DESCR_STR(
### Description
SVEA power gate helper.
Toggles ESC and servo power rails from arming state:
- arm   -> enable /dev/gpio9 and /dev/gpio10
- disarm -> disable /dev/gpio9 and /dev/gpio10
)DESCR_STR");

    PRINT_MODULE_USAGE_NAME("svea_power_gate", "module");
    PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
    return PX4_OK;
}

int svea_power_gate_main(int argc, char *argv[]) {
    return ModuleBase::main(SveaPowerGate::desc, argc, argv);
}
