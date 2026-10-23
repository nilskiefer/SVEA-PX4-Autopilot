# Peripheral MCU Bridge

## Purpose

`svea_peripheral_mcu` reads framed UART data from an external MCU and publishes:

- `wheel_distance`
- `wheel_encoders`

Source path:

- `src/modules/svea_peripheral_mcu/svea_peripheral_mcu.cpp`

## Peripheral Firmware and Wiring

Use [KTH-SML/SVEA-PX4-Autopilot-MCU-Peripheral](https://github.com/KTH-SML/SVEA-PX4-Autopilot-MCU-Peripheral). Compatibility checked against `eaf85d02`: protocol v2, CRC16-CCITT, topic 1 (`wheel_distance`, 16 bytes), topic 2 (`wheel_encoders`, 24 bytes).

| Connection | Pins |
| --- | --- |
| UART, 115200 baud, 8N1 | PX4 PB6 TX → ESP GPIO17 RX; PX4 PB7 RX ← ESP GPIO16 TX |
| Left upper A / lower B | D4 / D5 → C6 GPIO22 / GPIO23 |
| Right upper A / lower B | D2 / D3 → C6 GPIO2 / GPIO21 |

Common GND; 3.3 V logic. The peripheral requires two quadrature signals per wheel. GPIO23 is left B, not the right encoder input used by the older single-channel firmware.

The *SVEA Main Platform* assembly guide, page 17, confirms these D-pin connections but illustrates an ESP32-S3 and Clicker4 STM32F4. Use the [XIAO ESP32-C6 GPIO mapping](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) and the STM32F7 UART pins above for this firmware. Pages 13 and 15 show different disc slot patterns; neither specifies a slot count.

The MCU publishes both topics every 20 ms (nominally 50 Hz). PX4 forwards `WHEEL_DISTANCE` at 10 Hz; `wheel_encoders` requires explicit tunnel registration.

### Peripheral Configuration

Set in `main/svea_config.h` before building:

- `ENCODER_EMULATION_ENABLE=0`: real inputs. In upstream `eaf85d02`, this still auto-emulates after 300 ms without GPIO activity; remove that fallback before using hardware data.
- `ENCODER_TICKS_PER_REV`: measured decoded counts per wheel revolution. The upstream value is 80; a 40-cycle encoder decoded at 4× produces 160. Verify the actual disc and gearing.
- `ENCODER_WHEEL_DIAM_M=0.115` and `ENCODER_SPEED_SCALE=0.93`: verify distance calibration.
- `ENCODER_LEFT_INVERT` / `ENCODER_RIGHT_INVERT`: set so forward wheel motion increases both distances.

## Startup

Included in both `default` and `noboot` builds; started from board extras:

```sh
svea_peripheral_mcu start -d /dev/ttyS0 -b 115200
```

Default UART path is `/dev/ttyS0`.

## Published Topics

### `wheel_distance`

Published when distance frame payload is decoded and validated.

Decoded payload fields:

- `sequence` -> `wheel_distance.sequence`
- `time_ms` -> `wheel_distance.timestamp_sample` (`time_ms * 1000`)
- `left_distance_m` -> `wheel_distance.left_distance_m`
- `right_distance_m` -> `wheel_distance.right_distance_m`

### `wheel_encoders`

Decoded from `WheelEncoders` frames; index **0 is right, 1 is left**:

- `wheel_speed[0..1]`
- `wheel_angle[0..1]`

## Diagnostics

```sh
svea_peripheral_mcu status
listener wheel_distance -n 1
listener wheel_encoders -n 1
```

At rest, distances and angles must stay constant and speeds must be zero. Turn one wheel at a time: only that wheel's distance/angle should change. Compare one measured revolution against `ENCODER_TICKS_PER_REV`. ISR activity alone does not prove valid quadrature decoding; check rejected-edge counters.

Status output includes counters for:

- bytes/frames received
- wheel topic publish counts
- framing/magic statistics
- MAVLink-v1/v2 hit counters in parser path

UART reads are nonblocking; an idle or disconnected peripheral must not stall the shared work queue. Fixed firmware reports `build_tag=periphmcu-nonblocking-20261020`.

| Status | Meaning |
| --- | --- |
| `run` increases; wheel counters increase | UART reception and frame decoding are active |
| `run` increases; `bytes=0` | No UART bytes received; check MCU power/firmware, GPIO16 TX → PX4 PB7 RX, common GND and 115200 baud |
| `run=1`; `last_run_age_ms` keeps growing | Older firmware is blocked in its first UART read; flash the nonblocking fix |
| `crc_err` / `parse_err` increase | Received data is corrupt or incompatible; check baud rate, wiring and protocol version |

## MAVLink `WHEEL_DISTANCE` Stream

`wheel_distance` is streamed over MAVLink using `WHEEL_DISTANCE`:

- stream implementation: `src/modules/mavlink/streams/WHEEL_DISTANCE.hpp`
- message id: `MAVLINK_MSG_ID_WHEEL_DISTANCE`
- `count=2`
- `distance[0]=left_distance_m`
- `distance[1]=right_distance_m`
- time uses `timestamp_sample` when available, else `timestamp`

Onboard links stream `WHEEL_DISTANCE` at 10 Hz by default. Runtime override/check:

```sh
mavlink stream -d /dev/ttyACM0 -s WHEEL_DISTANCE -r 10
mavlink status
```

Raw angular speed/angle over the uORB tunnel:

```sh
mavlink uorb_tunnel add -t wheel_encoders -i 0 -r 10
```

On the companion, check `/self/mavros/wheel_odometry/distance` or `/self/px4/uorb/wheel_encoders/instance_0` when launched with `name:=self`. Tunnel registrations are runtime-only; add them to `rc.board_extras` for boot persistence.
