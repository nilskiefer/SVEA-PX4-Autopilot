# ROS Topic and Power Measurement Reference

Use this lookup when choosing signals for a research recording. Names below assume the SVEA MAVROS + `svea_core` uORB tunnel bridge and their default namespaces. Confirm names and message types with `ros2 topic list -t` on the car; remapping and enabled MAVROS plugins can change what is exposed.

## Quick Lookup

| Research signal | ROS topic / firmware source | Meaning |
| --- | --- | --- |
| ESC electrical input | `/px4/uorb/power_monitor/instance_0` | Voltage/current/power at the ESC supply shunt. Includes ESC losses; does not measure mechanical motor power. |
| Servo supply | `/px4/uorb/power_monitor/instance_1` | Combined supply to the servo connector rail, rather than one individual servo. |
| Total battery current | `/mavros/battery` (`sensor_msgs/msg/BatteryState`); PX4 `battery_status` | BQ769x2 pack measurement through the battery return shunt. This is separate from the eight `power_monitor` instances. |
| Onboard PC input power | `/px4/uorb/power_monitor/instance_5`, `instance_6` or `instance_7` | The PC uses one of the three 12V connectors: `U501`, `U42` or `U401`, respectively. |
| Commands sent to ESC and servos | `/mavros/manual_control/send` (`mavros_msgs/msg/ManualControl`) | Control input to PX4. `z` is throttle and `y` is steering in this setup; see [Controlling the Car](controlling-the-car.md) for scaling, mode gating and auxiliary fields. |
| Tunnel diagnostics | `/px4/uorb_tunnel/frame` (`std_msgs/msg/String`) | JSON frame/debug information from the companion bridge, rather than a physical sensor measurement. |

Throttle commands reach PCA9685 CH0; steering reaches CH1. Telemetry from `instance_0` reports the ESC **power supply**, not its PWM command, RPM, torque or efficiency. See the [actuator channel map](pmb3/actuators.md).

## Eight Power Monitor Instances

All dedicated topics below have prefix `/px4/uorb/power_monitor/` and message type `PowerMonitor` from the companion's compatible generated ROS message package (normally `px4_msgs`). Each carries `voltage_v`, `current_a` and `power_w`.

This is the **expected healthy-boot mapping**, derived from `rc.board_sensors` startup order and the drivers' first publications. uORB assigns instances at advertisement time. Failed initialization, delayed sensors, driver restarts or another publisher can change the mapping. `PowerMonitor` has no device/address identifier. Verify the mapping before labeling research data; an instance number alone is not a permanent hardware identity.

| Topic suffix | Device on PX4 I2C bus 2 | Schematic reference / channel | Shunt | Physical branch measured | Expected current sign for load draw |
| --- | --- | --- | --- | --- | --- |
| `instance_0` | INA226 `0x4E` | `U72` | `R18`, 1 mΩ | Switched ESC supply to `VCC_ESC`, connector `CN103` | Positive |
| `instance_1` | INA226 `0x4F` | `U81` | `R16`, 5 mΩ | Servo supply from `servo_vcc` to `servo_vcc_indicator`, connectors `H3`/`H4` | Positive |
| `instance_2` | INA3221 `0x41` | `CUR_2`, CH1 | `R9`, 16 mΩ | `VCC` branch to `CN101` pin 1 | Positive |
| `instance_3` | INA3221 `0x41` | `CUR_2`, CH2 | `R189`, 1 mΩ | `PACK+` branch to `CN101` pin 2 | Positive |
| `instance_4` | INA3221 `0x41` | `CUR_2`, CH3 | `R3`, 16 mΩ | `+12V` branch to `CN101` pin 3 | **Negative**: IN+ is on the connector side and IN− on the source side |
| `instance_5` | INA3221 `0x40` | `CUR_1`, CH1 | `R501`, 16 mΩ | Separate 12V connector `U501` | Positive |
| `instance_6` | INA3221 `0x40` | `CUR_1`, CH2 | `R104`, 16 mΩ | Separate 12V connector `U42` | Positive |
| `instance_7` | INA3221 `0x40` | `CUR_1`, CH3 | `R402`, 16 mΩ | Separate 12V connector `U401` | Positive |

`CUR_2` (`0x41`) starts **before** `CUR_1` (`0x40`). The schematic reference numbers are not the firmware instance order. `CN101` pin 4 is GND. The branch at pin 2 measures only what passes through `R189` to that connector, not every load on `PACK+` and not total battery power. The three 12V connector measurements also exclude other loads on the 12V rail.


## Field Dictionary and Interpretation

| Field | Unit | Interpretation |
| --- | --- | --- |
| `timestamp` | µs | PX4 time since boot at driver publication/readout; not ROS wall time or a hardware conversion timestamp. Restarting PX4 resets this clock. |
| `voltage_v` | V | INA3221: IN− bus voltage. INA226: driver reports the bus register value plus measured shunt drop; see the wiring caveat below. |
| `current_a` | A | Signed current. INA226 uses its calibrated current register; INA3221 computes shunt voltage divided by the configured shunt resistance. |
| `power_w` | W | INA226: chip power register scaled to watts. INA3221: `voltage_v * current_a`, so the reversed CN101 12V shunt also produces negative power for load draw. |
| `rconf` | register bits | Sensor configuration. Interpret as a 16-bit bitfield even though the message uses `int16`. |
| `rsv`, `rbv` | raw register codes | Shunt and bus voltage registers; their scaling differs between INA226 and INA3221. |
| `rc`, `rp`, `rcal` | device dependent | INA226: raw current, power and calibration registers. INA3221: current in mA, power in mW (both clamped to signed 16-bit), and shunt resistance in µΩ. Prefer the floating-point fields for analysis. |
| `me`, `al` | device dependent | INA226: mask/enable and alert-limit registers. INA3221: mask/enable register and channel number `1..3`. |

On a failed sensor read the drivers publish `voltage_v=0`, `current_a=-1`, `power_w=-1`. There is no dedicated validity flag in this message. Treat that combination as invalid rather than a measured zero-power state; do not reject all negative current, since polarity and reverse flow can legitimately make it negative.

**INA226 voltage caveat:** in the checked schematic, `U72` and `U81` have VBUS connected to VIN+. The driver adds the shunt voltage to the bus register as though that register measured VIN−. Consequently, the reported voltage includes an extra shunt-drop offset relative to the wired VIN+ node. Its `power_w` is the chip's power-register reading and need not equal reported `voltage_v * current_a`. Account for this when comparing voltages or power precisely; this documentation does not change the firmware calculation.

## Battery Power and Sign

The BQ769x2 (`U80`, address `0x08`) senses battery current through `R207`, a 1 mΩ shunt between battery negative and board GND. Its PX4 `battery_status` report includes pack voltage, current, individual cell voltages, discharged charge and battery state. The board default `BQ769X2_CURR_INV=1` inverts the chip current for the PX4 convention; verify the saved parameter and sign on the actual car.

For valid raw PX4 battery telemetry with positive discharge current, discharge power is `voltage_v * current_a`. MAVROS converts MAVLink battery current to the ROS `BatteryState` convention, making discharge current negative. Thus for `/mavros/battery`, positive discharge power is `-voltage * current`. `BatteryState` has no direct watts field. MAVROS may reconstruct voltage from the cell voltages in `BATTERY_STATUS`, whereas the BQ driver reports its PACK voltage register in raw `battery_status`. See the [MAVROS battery conversion implementation](https://github.com/mavlink/mavros/blob/ros2/mavros/src/plugins/sys_status.cpp).

For the unmodified PX4 fields over the tunnel, explicitly register `battery_status` and enable the tunnel stream as described in [Publish Rates](current-sensor-publish-rates.md). This creates `/px4/uorb/battery_status/instance_0` when instance 0 is the BQ publisher. It is not in the board's default tunnel list. `/fmu/out/battery_status` is a separate uXRCE-DDS path and only exists when that transport is running; it is not a substitute name for the MAVROS tunnel.

Do not sum these branch powers and call the result battery power. Conversion losses, unmonitored loads, alternative power sources, sampling timing and sign conventions prevent that equivalence. Use the BQ measurement for net battery draw and dedicated connector measurements for load attribution.

## Other Research Topics Available Through the Tunnel

These uORB sources are published by the peripheral MCU bridge, but are **not registered in the default board tunnel list**. Register the required topic/instance and enable the tunnel stream before expecting the corresponding ROS topic. See [Peripheral MCU Bridge](peripheral-mcu-bridge.md).

| ROS topic after explicit forwarding | Message / useful fields | Meaning |
| --- | --- | --- |
| `/px4/uorb/wheel_distance/instance_0` | `WheelDistance`: `left_distance_m`, `right_distance_m` | Left/right cumulative wheel distance in metres from the peripheral MCU. `timestamp_sample` is the MCU's `time_ms × 1000`, so it is a separate clock from PX4 `timestamp`. |
| `/px4/uorb/wheel_encoders/instance_0` | `WheelEncoders`: `wheel_speed`, `wheel_angle` | Angular speed in rad/s and angle in rad; array index **0 is right, 1 is left**. These are wheel measurements, not ESC motor RPM or commanded velocity. |

## Verify and Record

Run on the **PX4 NSH console** (for example QGroundControl's MAVLink Console):

```sh
svea_ina226 status
svea_ina3221 status
bq769x2 status
listener power_monitor -n 5 -i 0
listener power_monitor -n 5 -i 3
listener battery_status -n 5 -i 0
mavlink uorb_tunnel list
```

`listener` uses `-n` for sample count and `-i` for instance in this firmware. INA3221 status prints each chip's channel readings; compare those with the dedicated instances and use controlled load changes on a known connector to confirm assignment. Stop labeling a channel if that check disagrees with the table.

Run on the **ROS companion**:

```sh
ros2 topic list -t
ros2 topic info /px4/uorb/power_monitor/instance_0 -v
ros2 topic echo /px4/uorb/power_monitor/instance_0
ros2 topic hz /px4/uorb/power_monitor/instance_0
```

`/px4/uorb/power_monitor` multiplexes the latest arriving messages from **all** instances into one topic, without adding an instance field to `PowerMonitor`. Its rate can be the aggregate across rails. Use the dedicated `instance_N` topics for recording; the aggregate cannot identify which rail a message belongs to.

## Sources and Scope

Checked on 2026-10-01 against firmware `53453cddf9` and the local `svea_powerboard_3` schematic working tree based on `3efb3195ccf38c19269df69e284be5afa36df557` (which contained local schematic edits). The hardware mapping was traced through exported KiCad connectivity, including net ties and input filters, rather than inferred from topic numbers.

- Firmware: [startup order](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/53453cddf9/boards/mikroe/clicker4-stm32f7/init/rc.board_sensors), [message fields](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/53453cddf9/msg/PowerMonitor.msg), [INA226 calculations](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/53453cddf9/src/drivers/power_monitor/svea_ina226/svea_ina226.cpp), [INA3221 calculations](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/53453cddf9/src/drivers/power_monitor/svea_ina3221/svea_ina3221.cpp).
- Hardware repository: `svea_powerboard_3/hardware/kicad/svea_powerboard_rev3.kicad_sch`, especially `01_Battery-System.kicad_sch`, `MISC_IO_CURR_SENSING_1.kicad_sch`, `INA226_SINGLE_CURR_SENSOR.kicad_sch`, `19_Battery-Power.kicad_sch` and `18_Battery-BMS.kicad_sch`.
- Companion bridge: `svea/src/svea_core/scripts/px4_uorb_tunnel.py`, which creates both aggregate and dedicated instance publishers.

For rate tuning and measurement bandwidth, continue to [Current Sensor Publish Rates](current-sensor-publish-rates.md).
