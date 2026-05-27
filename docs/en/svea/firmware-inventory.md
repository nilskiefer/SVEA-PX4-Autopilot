# Firmware Inventory

## Board Target

- Target: `mikroe_clicker4-stm32f7_noboot`
- Board config: `boards/mikroe/clicker4-stm32f7/noboot.px4board`

## Core Board Init Scripts

- `boards/mikroe/clicker4-stm32f7/init/rc.board_defaults`
- `boards/mikroe/clicker4-stm32f7/init/rc.board_sensors`
- `boards/mikroe/clicker4-stm32f7/init/rc.board_mavlink`
- `boards/mikroe/clicker4-stm32f7/init/rc.board_extras`

## Enabled SVEA/PMB3 Drivers and Modules

### Sensors and powerboard

- `bq769x2` (smart battery / pack policy)
- `lsm6dsox` (IMU)
- `pcal6524` x2 (GPIO expanders)
- `svea_ina226` x2 (rail monitors)
- `svea_ina3221` x2 (multi-channel rail monitors)
- `pca9685_pwm_out` (actuator outputs)

### Board-specific modules

- `svea_power_gate`
- `svea_peripheral_mcu`

### MAVLink/USB behavior

- USB CDC startup is explicit via `sercon` + `mavlink start -d /dev/ttyACM0`
- `PX4_UORB_TUNNEL` forwarding for `power_monitor` is set in `rc.board_extras`

## Effective Runtime Topology (from startup scripts)

### I2C bus 2 devices

- BQ769x2 at `0x08`
- LSM6DSOX at `0x6b`
- PCAL6524 at `0x22` and `0x23`
- INA226 at `0x4E` and `0x4F`
- INA3221 at `0x40` and `0x41`
- PCA9685 at `0x61`

### `power_monitor` instance plan

- `0..1`: INA226
- `2..7`: INA3221 channels

## Notable Build-Time Choices

From `noboot.px4board`:

- `CONFIG_MODULES_EKF2=n`
- `CONFIG_DRIVERS_GPS=n`
- `CONFIG_SYSTEMCMDS_TOP=y`
- `CONFIG_MODULES_LOAD_MON=y`
- `CONFIG_MODULES_ROVER_ACKERMANN=y`

## Related Source Paths

- `src/drivers/power_monitor/svea_ina226`
- `src/drivers/power_monitor/svea_ina3221`
- `src/drivers/gpio/pcal6524`
- `src/drivers/smart_battery/bq769x2`
- `src/modules/svea_power_gate`
- `src/modules/svea_peripheral_mcu`

## Detailed References

- [Manual Control Gating (ROS vs RC)](manual-control-gating.md)
- [Peripheral MCU Bridge](peripheral-mcu-bridge.md)
- [MAVLink uORB Tunnel](mavlink-uorb-tunnel.md)
