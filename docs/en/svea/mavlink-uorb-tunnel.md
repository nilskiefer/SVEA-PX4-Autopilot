# MAVLink uORB Tunnel

## What the tunnel carries {#why-it-exists-in-this-fork}

SVEA publishes the INA226 and INA3221 channels as separate `power_monitor` instances. The `PX4_UORB_TUNNEL` stream wraps selected uORB instances in MAVLink `TUNNEL` frames so the companion can decode each channel separately.

## Runtime Configuration on Clicker4

From `boards/mikroe/clicker4-stm32f7/init/rc.board_extras`:

```sh
mavlink uorb_tunnel add -t power_monitor -i 0 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 1 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 2 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 3 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 4 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 5 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 6 -r 4 -f
mavlink uorb_tunnel add -t power_monitor -i 7 -r 4 -f
```

Instance plan:

- `0..1` -> `svea_ina226` (2 rails)
- `2..7` -> `svea_ina3221` (2 chips x 3 channels)

This layout depends on startup order and first advertisement order from drivers.

## CLI Behavior

### Add/list/remove

```sh
mavlink uorb_tunnel list
mavlink uorb_tunnel add -t power_monitor -i 0 -r 4
mavlink uorb_tunnel remove -s 1
```

Rules implemented in `mavlink_uorb_tunnel.cpp`:

- `-i` range: `0..9`
- rate limit: `<=100 Hz`
- topic must exist
- without `-f`, add is rejected if the uORB instance is not advertised yet

## TX Path (Autopilot -> MAVLink)

Implementation: `src/modules/mavlink/streams/PX4_UORB_TUNNEL.hpp`.

Behavior:

1. Reads configured tunnel topics from global registry (`uorb_tunnel_count/get_config`).
2. Subscribes to each configured uORB instance.
3. On update + rate gate, serializes the topic payload into MAVLink `TUNNEL` frames.
4. Splits large payloads into fragments.

Frame fields include:

- protocol version
- uORB instance
- sequence
- topic `message_hash`
- topic name length + name
- total payload length
- fragment offset

Payload type used: `0xE001`.

## Commands from ROS

The tunnel carries outbound telemetry. Send [ROS manual control](ros-manual-control.md) through MAVLink `MANUAL_CONTROL`. SWB on the RC transmitter selects which source has control.

## Observability

```sh
mavlink uorb_tunnel list
listener power_monitor
mavlink status
```

## Related

- [Wheel encoder bridge](peripheral-mcu-bridge.md)
- [ROS topics and power measurements](ros-topic-reference.md)
- [Current sensor publish rates](current-sensor-publish-rates.md)

## When To Use `-f`

Use `-f` in startup scripts when topic publishers may start after tunnel configuration.
This is required for deterministic boot configuration of dynamic sensor stacks.
