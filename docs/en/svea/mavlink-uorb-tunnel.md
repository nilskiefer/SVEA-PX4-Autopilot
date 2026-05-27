# MAVLink uORB Tunnel

## Why It Exists in This Fork

SVEA power telemetry is published as multiple `power_monitor` uORB instances (INA226 + INA3221 channels).
Standard MAVLink streams do not carry this multi-instance topic set in the format needed by the companion stack.

This fork adds `PX4_UORB_TUNNEL` forwarding to move selected uORB topics over MAVLink `TUNNEL` frames.

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

Rules implemented in `mavlink_main.cpp`:

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

## RX Path (MAVLink -> Autopilot)

Experimental.

Implementation: `src/modules/mavlink/mavlink_receiver.cpp` (`handle_px4_uorb_tunnel_message`).

Validation path:

1. target system/component filter
2. payload length/header checks
3. protocol version check
4. topic lookup by name
5. message hash match
6. total length match vs topic size

Fragment handling:

- first fragment allocates pending buffer slot
- subsequent fragments append by offset
- stale fragment state is cleared periodically
- when complete, topic is published to requested instance

If checks fail, message is rejected with warning logs.

## Observability

```sh
mavlink uorb_tunnel list
listener power_monitor
mavlink status
```

## Related

- [Peripheral MCU Bridge](peripheral-mcu-bridge.md)

## When To Use `-f`

Use `-f` in startup scripts when topic publishers may start after tunnel configuration.
This is required for deterministic boot configuration of dynamic sensor stacks.
