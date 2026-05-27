# Peripheral MCU Bridge

## Purpose

`svea_peripheral_mcu` reads framed UART data from an external MCU and publishes:

- `wheel_distance`
- `wheel_encoders`

Source path:

- `src/modules/svea_peripheral_mcu/svea_peripheral_mcu.cpp`

## Startup

Started from board extras:

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

Published from same frame family with:

- `wheel_speed[0..1]`
- `wheel_angle[0..1]`

## Diagnostics

```sh
svea_peripheral_mcu status
listener wheel_distance 1
listener wheel_encoders 1
```

Status output includes counters for:

- bytes/frames received
- wheel topic publish counts
- framing/magic statistics
- MAVLink-v1/v2 hit counters in parser path

## MAVLink `WHEEL_DISTANCE` Stream

`wheel_distance` is streamed over MAVLink using `WHEEL_DISTANCE`:

- stream implementation: `src/modules/mavlink/streams/WHEEL_DISTANCE.hpp`
- message id: `MAVLINK_MSG_ID_WHEEL_DISTANCE`
- `count=2`
- `distance[0]=left_distance_m`
- `distance[1]=right_distance_m`
- time uses `timestamp_sample` when available, else `timestamp`

Enable/check stream:

```sh
mavlink stream -s WHEEL_DISTANCE -r 20
mavlink status
```
