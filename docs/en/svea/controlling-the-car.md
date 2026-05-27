# Controlling the Car (RC and ROS/MAVLink)

## Transmitter Controls

- Green square: arm button (momentary, CH7)
- Red circle: 3-way mode switch (CH5)
  - left: ROS/MAVLink manual control (`MODE_SLOT_1`) with RC fallback
  - middle: RC-only manual control (`MODE_SLOT_2`)
  - right: kill switch position (`MODE_SLOT_3`)

![RC arm button and 3-way switch](pmb3/rc-controls.png)

In this image, the 3-way switch is currently in the middle position (RC-only).

## Control Flow

::: mermaid
flowchart TD
  A[Power on RC transmitter] --> C{RC link connected?}
  C -- No --> D[Cannot arm with RC button]
  C -- Yes --> E[Select mode on 3-way switch]
  E --> F{Switch position}
  F -- Left --> G[MODE_SLOT_1: MAVLink/ROS accepted, RC fallback active]
  F -- Middle --> H[MODE_SLOT_2: RC-only control]
  F -- Right --> I[MODE_SLOT_3: Kill asserted]
  G --> J[Press arm button to arm]
  H --> J
  J --> K[Drive]
  K --> L[Press arm button again to disarm]
:::

## Arming and Disarming

Board defaults use:

- `RC_MAP_ARM_SW=7`
- `COM_ARM_SWISBTN=1`

So:

- arm: press CH7 arm button once (green-marked button in image)
- disarm: press CH7 arm button once again (same green-marked button)

Note: with this operator flow, the RC receiver must be connected to use button arming.

Quick state check:

```sh
listener actuator_armed 1
```

## Mode Rules in This Fork

- `MANUAL_CONTROL` accepted only in `MODE_SLOT_1`
- `RC_CHANNELS_OVERRIDE` accepted only in `MODE_SLOT_1`
- other mode slots reject MAVLink manual input paths

Source:

- `src/modules/mavlink/mavlink_receiver.cpp`

## MAVLink/ROS Validity Requirement

Manual input validity is timeout-based via `COM_RC_LOSS_T` (seconds).

- valid only while updates arrive within `COM_RC_LOSS_T`
- if updates stop past timeout, MAVLink manual input becomes stale
- in `MODE_SLOT_1`, stale MAVLink input falls back to RC

Board default:

- `COM_RC_LOSS_T=0.5`

Implication:

- theoretical minimum: `>2 Hz`
- practical recommendation: `20 Hz` (`10-20 Hz` minimum)

## Verify ROS `MANUAL_CONTROL` Lands in PX4 (NSH)

If you are not already in NSH, first use:

- [NSH Access Without QGroundControl](troubleshooting.md#nsh-access-without-qgroundcontrol)

1. MAVLink RX path alive:

```sh
mavlink status
```

2. Decoded manual setpoint updates:

```sh
listener manual_control_setpoint 10
```

3. Output path updates:

```sh
listener actuator_servos 10
listener actuator_motors 10
listener actuator_outputs 10
```

4. Gate check:

```sh
listener manual_control_switches 5
```

`mode_slot` must be `MODE_SLOT_1` for MAVLink manual input to be accepted.

## ROS Example

Start SVEA core:

```sh
ros2 launch svea_core svea.xml is_sim:=false
```

Publish manual control at 20 Hz:

```sh
ros2 topic pub -r 20 /mavros/manual_control/send mavros_msgs/msg/ManualControl "{x: 0, y: 1000, z: 600, r: 0, buttons: 0, buttons2: 0, enabled_extensions: 252, s: 0, t: 0, aux1: 1000, aux2: -1000, aux3: -1000, aux4: 0, aux5: 0, aux6: 0}"
```

For more details, consult the SVEA ROS repository documentation:

- [kth-sml/svea](https://github.com/kth-sml/svea)
