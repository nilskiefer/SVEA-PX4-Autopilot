# Power gate {#svea-power-gate}

The `svea_power_gate` module controls power to the ESC and servo rails using arming and receiver state.

## Rail outputs {#purpose}

- ESC rail enable: `/dev/gpio9`
- Servo rail enable: `/dev/gpio10`

## Startup

The Clicker4 starts the module from `boards/mikroe/clicker4-stm32f7/init/rc.board_sensors`. The module calls `apply_power(false)` before scheduling its first check.

## Control Inputs

Subscriptions:

- `actuator_armed`
- `rc_channels`

Enable condition (`should_enable`) from code:

- `actuator_armed.armed`
- `!actuator_armed.kill`
- `!actuator_armed.lockdown`
- `!actuator_armed.termination`

## Rail sequence {#actual-rail-behavior-current-code}

### Enable the rails {#when-enabling-apply-power-true}

1. Set servo rail high (`/dev/gpio10=1`)
2. Wait `500 ms`
3. Set ESC rail high (`/dev/gpio9=1`)

### Disable the rails {#when-disabling-apply-power-false}

For `apply_power(false)`, the sequence depends on the receiver link.

| Receiver | Rail sequence |
| --- | --- |
| Disconnected (`rc_channels.signal_lost=true`) | Set `/dev/gpio9=0`, wait 50 ms, then set `/dev/gpio10=0`. |
| Connected | Leave the ESC rail at its current state and set `/dev/gpio10=0`. |

## Reliability Behavior

- Poll interval: `200 ms`
- Reassert interval: `200 ms`
- If a GPIO write fails and requested state != observed state, module retries automatically.

## Commands

```sh
svea_power_gate status
```

Status prints requested state, rail state, run count, and device nodes.
