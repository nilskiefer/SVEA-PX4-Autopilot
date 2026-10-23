# SVEA Power Gate

## Purpose

`src/modules/svea_power_gate` controls PMB3 rail enables based on arming state.

Device nodes used:

- ESC rail enable: `/dev/gpio9`
- Servo rail enable: `/dev/gpio10`

## Startup

Started from:

- `boards/mikroe/clicker4-stm32f7/init/rc.board_sensors`

On start, module forces safe state (`apply_power(false)`) before scheduling periodic checks.

## Control Inputs

Subscriptions:

- `actuator_armed`
- `rc_channels`

Enable condition (`should_enable`) from code:

- `actuator_armed.armed`
- `!actuator_armed.kill`
- `!actuator_armed.lockdown`
- `!actuator_armed.termination`

## Actual Rail Behavior (Current Code)

### When enabling (`apply_power(true)`)

1. Set servo rail high (`/dev/gpio10=1`)
2. Wait `500 ms`
3. Set ESC rail high (`/dev/gpio9=1`)

### When disabling (`apply_power(false)`)

- If RC is disconnected (`rc_channels.signal_lost=true`):
1. Set ESC rail low (`/dev/gpio9=0`)
2. Wait `50 ms`
3. Set servo rail low (`/dev/gpio10=0`)

- If RC is still connected:
1. Keep ESC rail unchanged
2. Set servo rail low (`/dev/gpio10=0`)

This is implementation-defined in `svea_power_gate.cpp`; docs should follow this behavior exactly.

## Reliability Behavior

- Poll interval: `200 ms`
- Reassert interval: `200 ms`
- If a GPIO write fails and requested state != observed state, module retries automatically.

## Commands

```sh
svea_power_gate status
```

Status prints requested state, rail state, run count, and device nodes.
