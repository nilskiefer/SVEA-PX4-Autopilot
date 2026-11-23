# ROS manual control

Publish to `/mavros/manual_control/send` with message type `mavros_msgs/msg/ManualControl`. Set the transmitter's [SWB mode switch](controlling-the-car.md#control-flow) to the left position before sending commands.

## Authority and command rate {#mavlink-ros-validity-requirement}

PX4 accepts `MANUAL_CONTROL` and `RC_CHANNELS_OVERRIDE` in `MODE_SLOT_1`. The receiver implements this check in `src/modules/mavlink/mavlink_receiver.cpp`.

Send commands continuously at 10–20 Hz. The Clicker4 default for `COM_RC_LOSS_T` is 0.5 seconds, so you need updates faster than 2 Hz to stay within the timeout. When the stream goes stale in `MODE_SLOT_1`, RC takes over.

## Check the command path in PX4 {#verify-ros-manual-control-lands-in-px4-nsh}

Open the [PX4 console](quick-bringup.md#_4-open-nsh-over-mavlink-shell). Check the MAVLink connection, decoded setpoints, and actuator outputs.

```sh
mavlink status
listener manual_control_setpoint -n 10
listener actuator_servos -n 10
listener actuator_motors -n 10
listener actuator_outputs -n 10
listener manual_control_switches -n 5
```

The switch topic must report `mode_slot` as `MODE_SLOT_1` for incoming manual control.

## Message fields {#field-mapping-and-ranges}

| Field | Meaning |
| --- | --- |
| `y` | Steering, `-1000..1000` |
| `z` | Throttle, `0..1000`, with `500` as neutral |
| `aux1..aux6` | Extra manual channels, `-1000..1000`, sent as MAVLink v2 extensions |
| `enabled_extensions` | Set to `252` to enable `aux1..aux6` |

Send the full state in each message. If `aux1` changes from `1000` to `0`, the output follows the new value.

## Actuator mapping {#current-output-mapping-board-defaults}

The [PCA9685 channel table](pmb3/actuators.md#channel-mapping-current-firmware) gives each output's PX4 function. CH2 through CH4 use 1200 and 1800 µs as their endpoints.

### Gear and differential polarity {#gear-diff-polarity}

This keeps the front and rear differential polarity and the gear convention from `svea_lli_zephyr`. For a binary channel, `-1000` selects the minimum pulse and `+1000` selects the maximum.

| Output | Field | ON / HIGH | OFF / LOW |
| --- | --- | --- | --- |
| Front differential, CH2 | `aux1` | `+1000` | `-1000` |
| Rear differential, CH3 | `aux2` | `-1000` | `+1000` |
| Gear, CH4 | `aux3` | `-1000` | `+1000` |

### Misc outputs {#mode-dependent-behavior-for-misc-channels}

`svea_rc_servo_latch` owns CH4 through CH6 through `Actuator_Set3/1/2`. For a MAVLink input source, `aux3` drives gear, `aux4` drives misc0, and `aux5` drives misc1 directly. For an RC source, [the transmitter switches and dial](controlling-the-car.md#rc-misc-servo-selector) control those outputs.

## Publish test commands {#test-commands}

::: details Full command template

```sh
STEER=1000           # y: steering (-1000..1000)
THROTTLE=600         # z: throttle (0..1000, 500=neutral)
FRONT_DIFF=1000      # aux1: front diff (-1000..1000)
REAR_DIFF=-1000      # aux2: rear diff (-1000..1000)
GEAR=-1000           # aux3: gear (-1000..1000)
AUX4=0               # aux4: misc0 setpoint in MAVROS mode (CH5 / Actuator_Set1)
AUX5=0               # aux5: misc1 setpoint in MAVROS mode (CH6 / Actuator_Set2)
AUX6=0               # aux6: currently unused

ros2 topic pub -r 20 /mavros/manual_control/send mavros_msgs/msg/ManualControl "{x: 0, y: ${STEER}, z: ${THROTTLE}, r: 0, buttons: 0, buttons2: 0, enabled_extensions: 252, s: 0, t: 0, aux1: ${FRONT_DIFF}, aux2: ${REAR_DIFF}, aux3: ${GEAR}, aux4: ${AUX4}, aux5: ${AUX5}, aux6: ${AUX6}}"
```

:::

::: code-group

```sh [Steer and drive]
ros2 topic pub -r 20 /mavros/manual_control/send mavros_msgs/msg/ManualControl "{x: 0, y: 1000, z: 600, r: 0, buttons: 0, buttons2: 0, enabled_extensions: 252, s: 0, t: 0, aux1: 1000, aux2: -1000, aux3: -1000, aux4: 0, aux5: 0, aux6: 0}"
```

```sh [Continuous neutral]
ros2 topic pub -r 20 /mavros/manual_control/send mavros_msgs/msg/ManualControl "{x: 0, y: 0, z: 500, r: 0, buttons: 0, buttons2: 0, enabled_extensions: 252, s: 0, t: 0, aux1: -1000, aux2: -1000, aux3: -1000, aux4: 0, aux5: 0, aux6: 0}"
```

```sh [One-shot neutral]
ros2 topic pub -1 /mavros/manual_control/send mavros_msgs/msg/ManualControl "{x: 0, y: 0, z: 500, r: 0, buttons: 0, buttons2: 0, enabled_extensions: 252, s: 0, t: 0, aux1: -1000, aux2: -1000, aux3: -1000, aux4: 0, aux5: 0, aux6: 0}"
```

:::

See the [SVEA ROS repository](https://github.com/kth-sml/svea) for the companion stack.
