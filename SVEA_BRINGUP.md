# SVEA Clicker4 STM32F7 Bringup

This document captures the working bringup flow for:

- Board: `MIKROE_CLICKER4_STM32F7`
- PX4 target: `mikroe_clicker4-stm32f7`
- Probe: CODEGRIP (`CMSIS-DAP`)

This bringup uses the `noboot` firmware target only.

## 1) Prerequisites

- You are in repo root: `/workspaces/SVEA-PX4-Autopilot`
- Probe is visible in `lsusb` as `MikroElektronika CODEGRIP-OneMcu [CMSIS-DAP]`
- `openocd` is installed
- Use **USB-C CN2** (debug/CMSIS-DAP path) for OpenOCD flashing
- UART debug console is on **MB1** header: **PA2 (TX)** / **PA3 (RX)** (`ttyS0`)

## 2) Build `noboot` firmware

Build:

```bash
make mikroe_clicker4-stm32f7_noboot
```

## 3) Flash firmware with OpenOCD (CN2)

Flash app at flash base `0x08000000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_noboot/mikroe_clicker4-stm32f7_noboot.bin 0x08000000 verify; reset run; shutdown"
```
This is via the usb c port marked CN2

If you are having issues it might be because the flash chip's firmware needs to be update:

### 3.1. Update the mikroE flasher firmware 
If you are having trouble flashing with openocd the likely cause is autodated flasher firmware.

If you get errors like 
`CMSIS-DAP command mismatch. Sent 0x10 received 0x0`
and you see a line like 
`Info : CMSIS-DAP: FW Version = 1.0`

Then this will likely solve the issue:

Install CODEGRIP from:

https://www.mikroe.com/codegrip

The download link is near the bottom of the page.

Follow these instructions [https://helpdesk.mikroe.com](https://helpdesk.mikroe.com/en-us/10-codegrip/79-how-to-update-codegrip-device-firmware)

### 3.2 Testing/troubleshooting USB connection
Connect USB cable from CN1 to a computer running Linux (eg jetson).

Running in terminal
`ls /dev/serial/by-id/*SVEA*` 
Should show something like 
`/dev/serial/by-id/usb-SVEA_PX4_AUTOPILOT_0-if00`

If not try pressing large RED button on the board (this resets the microcontroller but shouldn't turn off raspberry/jetson)
If not try flipping the off and on switch on the mikroe board (this will likely turn off the raspberry/jetson)

## 4) USB behavior notes

- This setup has two USB paths in practice:
  - CODEGRIP debug interface (`2dbc:*`)
  - PX4 app CDC
- `noboot` flow does not use PX4 bootloader USB upload.

## 5) Console + MAVLink split

- Keep UART (`PA2/PA3`, `ttyS0`) for readable NSH/debug text
- Keep MAVLink on USB CDC (`/dev/ttyACM0`)

From `nsh>`:

```sh
param set MAV_0_CONFIG 0
param set MAV_1_CONFIG 0
param set MAV_2_CONFIG 0
param set SYS_USB_AUTO 2
param save
reboot
```

Check:

```sh
mavlink status
```

## 6) LED/Button test modules

Current board startup launches:

- `led_bus_worker`
- `led_pattern`
- `button_led_mirror`

Manual control from `nsh>`:

```sh
led_pattern stop
led_pattern start -t 120 -n 6
button_led_mirror status
led_bus_worker status
```

## 6.1) Neopixel (WS2815F on PC9 / MB4PWM)

Start neopixel driver and run basic color checks:

```sh
neopixel start -n 1
neopixel status

led_control on -l 0 -c red   -p 255
led_control on -l 0 -c green -p 255
led_control on -l 0 -c blue  -p 255

led_control blink -l 0 -c red -s fast -n 0
```

Stop/reset:

```sh
led_control reset
led_control off -l 0
neopixel stop
```

## 6.2) Neopixel FX module (`neopixel_fx`)

`neopixel_fx` is a small effect module that publishes animated `led_control` patterns.

Ownership behavior:

- `neopixel_fx` requires `neopixel` driver running (`neopixel start -n 1`).
- While `neopixel_fx` runs, the neopixel driver's `led_control` intake is temporarily disabled.
- This prevents neopixel output contention, while other LED drivers on the system can still use `led_control`.
- On `neopixel_fx stop`, normal neopixel `led_control` handling is restored automatically.

Examples:

```sh
# rainbow cycle
neopixel_fx start -m rainbow -t 120 -l 0 -p 2
neopixel_fx status

# red/blue flash
neopixel_fx stop
neopixel_fx start -m police -t 90 -l 0 -p 2

# breathe while cycling colors
neopixel_fx stop
neopixel_fx start -m breathe -t 350 -l 0 -p 2

# stop effect
neopixel_fx stop
led_control off -l 0
```

## 7) GNSS on mikroBUS 2/3

- You can connect a GNSS module on **mikroBUS 2** or **mikroBUS 3**.
- Start GNSS manually from `nsh>` with:

```sh
gps stop
gps start -d /dev/ttyS0 -b 115200
gps status
listener sensor_gps 1
```

## 8) PCAL6524 GPIO common commands and mapping

The PCAL6524 driver registers each pin as a NuttX GPIO device node, and `gpio` systemcmd talks directly to these nodes.

Layout:

- Primary expander: `0x22`, `-M 0` => `/dev/gpio0` ... `/dev/gpio23`
- Secondary expander: `0x23`, `-M 24` => `/dev/gpio24` ... `/dev/gpio47`

Basic NSH commands:

```sh
# Read current pin level
gpio read /dev/gpio0

# Drive high
gpio write /dev/gpio0 1

# Drive low
gpio write /dev/gpio0 0

# Read back
gpio read /dev/gpio0
```

Observe live input publications:

```sh
listener gpio_in
```

Notes:

- Startup is read-only: the driver does not write direction/output/pull registers on init.
- On boot/FM U reset, PX4 reads live PCAL6524 registers and adopts the current chip pin state.
- `gpio write` on a pin may imply direction change to output on first use.
- Startup CLI flags (`-D/-O/-P`) are not forced into chip registers at init.
- Driver path for writes is: `gpio ioctl -> PCAL6524::gpio_write() -> gpio_out uORB -> driver callback -> I2C register write`.

### Primary expander (`0x22`) map => `/dev/gpio0..23`

| Pin | Signal | Direction | Device |
| --- | --- | --- | --- |
| P0_0 | BQ/ALERT/LED | output | `/dev/gpio0` |
| P0_1 | BQ/RST-SHUT | input | `/dev/gpio1` |
| P0_2 | USB-C/HUSB238A/EN# | output | `/dev/gpio2` |
| P0_4 | Power/5V-Buck/PGOOD | input | `/dev/gpio4` |
| P0_5 | Button/DIGITAL | input | `/dev/gpio5` |
| P0_6 | CHARGING-IC/DIGIPOT-#EN | input | `/dev/gpio6` |
| P0_7 | Power/5V-Buck/EN# | output | `/dev/gpio7` |
| P1_0 | IO-Expander/Secondary/INT | input | `/dev/gpio8` |
| P1_1 | ESC/EN | output | `/dev/gpio9` |
| P1_2 | Servo/TPS/EN | output | `/dev/gpio10` |
| P1_3 | Servo/TPS/PGOOD | input | `/dev/gpio11` |
| P1_5 | Power/12V-Buck/EN | output | `/dev/gpio13` |
| P1_6 | Charger/CN3722/EN# | output | `/dev/gpio14` |
| P2_0 | eFuse/TPS16630/PGOOD | input | `/dev/gpio16` |
| P2_1 | Receiver/EN | output | `/dev/gpio17` |
| P2_3 | ADS1115/ALERT | input | `/dev/gpio19` |
| P2_5 | Button/LOW | input | `/dev/gpio21` |
| P2_6 | eFuse/TPS16630/SHDN | output | `/dev/gpio22` |
| P2_7 | eFuse/TPS16630/FAULT | input | `/dev/gpio23` |

### Secondary expander (`0x23`) map => `/dev/gpio24..47`

| Pin | Signal | Direction | Device |
| --- | --- | --- | --- |
| P0_0..P0_7 | INA3221-CUR1/CUR2 flags | input | `/dev/gpio24..31` |
| P1_0 | INA226-ESC-0x4E/ALERT | input | `/dev/gpio32` |
| P1_1 | INA226-SERVO-0x4F/ALERT | input | `/dev/gpio33` |
| P1_5 | Charger/CN3072/CHG-DONE | input | `/dev/gpio37` |
| P1_6 | Charger/CN3072/CHG-ACTIVE | input | `/dev/gpio38` |
| P2_0 | USB-C/HUSB238A/FAULT-OUT2 | input | `/dev/gpio40` |
| P2_3 | USB-C/HUSB238A/INT | input | `/dev/gpio43` |
| P2_6 | IMU/INT2 | input | `/dev/gpio46` |
| P2_7 | IMU/INT1 | input | `/dev/gpio47` |

Safe output toggle examples:

```sh
# ESC enable
gpio write /dev/gpio9 1
gpio write /dev/gpio9 0

# 12V buck enable
gpio write /dev/gpio13 1
gpio write /dev/gpio13 0

# BQ alert/LED pin
gpio write /dev/gpio0 1
gpio write /dev/gpio0 0
```

### Arming-driven power rails

`svea_power_gate` is auto-started and controls:

- `/dev/gpio9`  (ESC enable)
- `/dev/gpio10` (Servo TPS enable)

Behavior:

- disarmed -> both rails forced low
- armed -> both rails set high

Check:

```sh
svea_power_gate status
gpio read /dev/gpio9
gpio read /dev/gpio10
```

## 9) SVEA firmware baseline (battery and BMS specs)

These are the baseline values set in `boards/mikroe/clicker4-stm32f7/init/rc.board_defaults`.

### Pack-level battery baseline

| Parameter | Default | Meaning |
| --- | --- | --- |
| `BAT1_SOURCE` | `1` | Battery source is external driver topic |
| `BAT1_CAPACITY` | `9000` | Capacity in mAh |
| `BAT1_N_CELLS` | `3` | 3S pack |
| `BAT1_V_CHARGED` | `4.2` | Full per-cell voltage (V) |
| `BAT1_V_EMPTY` | `3.2` | Empty per-cell voltage (V) |

### BQ769x2 baseline

| Parameter | Default | Meaning |
| --- | --- | --- |
| `SENS_EN_BQ769X2` | `1` | Enable BQ769x2 driver |
| `BQ769X2_ADDR` | `8` | I2C address (`0x08`) |
| `BQ769X2_CELLS` | `3` | Cell count |
| `BQ769X2_CRC` | `1` | CRC enabled |
| `BQ769X2_CFG` | `1` | Apply configuration to chip |
| `BQ769X2_SHUNT` | `1000` | Shunt in uOhm |
| `BQ769X2_COV_V` | `4.25` | Cell overvoltage trip (V) |
| `BQ769X2_COV_RV` | `4.10` | Cell overvoltage recovery (V) |
| `BQ769X2_COV_DLY` | `1000` | COV delay (ms) |
| `BQ769X2_CUV_V` | `3.20` | Cell undervoltage trip (V) |
| `BQ769X2_CUV_RV` | `3.30` | Cell undervoltage recovery (V) |
| `BQ769X2_CUV_DLY` | `1000` | CUV delay (ms) |
| `BQ769X2_OCC_A` | `6` | Charge overcurrent trip (A) |
| `BQ769X2_OCC_DLY` | `100` | OCC delay (ms) |
| `BQ769X2_OCD_A` | `25` | Discharge overcurrent trip (A) |
| `BQ769X2_OCD_DLY` | `425` | OCD delay (ms) |
| `BQ769X2_SCD_A` | `200` | Short-circuit trip (A) |
| `BQ769X2_SCD_DLY` | `60` | SCD delay (us) |
| `BQ769X2_OTC_C` | `45` | Charge over-temp trip (C) |
| `BQ769X2_UTC_C` | `0` | Charge under-temp trip (C) |
| `BQ769X2_OTD_C` | `60` | Discharge over-temp trip (C) |
| `BQ769X2_UTD_C` | `-20` | Discharge under-temp trip (C) |
| `BQ769X2_T_HYST_C` | `5` | Temperature hysteresis (C) |
| `BQ769X2_TPROT_EN` | `0` | Temperature protection off in baseline config |
| `BQ769X2_PWR_CFG` | `10370` | Power configuration raw value |
| `BQ769X2_DIODEMA` | `500` | Body diode current threshold (mA) |
| `BQ769X2_FETOPT` | `29` | FET options raw value |
| `BQ769X2_FET_AUTO` | `1` | Auto FET policy enabled |
| `BQ769X2_FETMASK` | `5` | Main FET on-mask |
| `BQ769X2_PCHGMASK` | `8` | Precharge FET mask |
| `BQ769X2_PCHG_MS` | `0` | Minimum precharge stage (ms) |
| `BQ769X2_PTO_MS` | `2000` | Precharge timeout (ms) |
| `BQ769X2_PDV_PCT` | `5.0` | Precharge equalization threshold (% of Vpack) |
| `BQ769X2_POST_MS` | `10` | Post/precharge overlap (ms) |
| `BQ769X2_ALL_OK` | `1` | Gate FET policy by all checks |
| `BQ769X2_OW_CHK` | `1` | Open-wire check enabled |
| `BQ769X2_OW_TIME` | `10` | Open-wire hardware period (s) |
| `BQ769X2_OWTOL` | `50` | Open-wire tolerance (mV/cell) |
| `BQ769X2_VCMODE` | `515` | Cell channel mapping mode (3S custom wiring) |

To inspect these on target:

```sh
param show BAT1_*
param show BQ769X2_*
```

## 11) INA226 current monitor commands

Two INA226 monitors are wired on I2C bus `2`:

- `0x4E` ESC current shunt = `1 mOhm` (`0.001`)
- `0x4F` Servo buck current shunt = `5 mOhm` (`0.005`)

Board startup runs:

```sh
svea_ina226 -I -b 2 -a 0x4E -r 0.001 start
svea_ina226 -I -b 2 -a 0x4F -r 0.005 start
```

`-r` is the per-instance shunt override (Ohm).

Manual checks:

```sh
i2cdetect -b 2
svea_ina226 status
listener power_monitor 5
```

`svea_ina226` is treated as a rail monitor and publishes `power_monitor` (not `battery_status`).

### PX4_UORB_TUNNEL forwarding for `power_monitor`

Use these NSH commands to forward `power_monitor` over `PX4_UORB_TUNNEL`.

First verify which instances actually exist:

```sh
listener power_monitor
```

If you only see `Instance 0` and `Instance 1`, only add those.

Add forwarding entries:

```sh
mavlink uorb_tunnel add -t power_monitor -i 0 -r 10
mavlink uorb_tunnel add -t power_monitor -i 1 -r 10
```

List configured entries:

```sh
mavlink uorb_tunnel list
```

Remove one entry (example: slot 2):

```sh
mavlink uorb_tunnel remove -s 2
```

Notes:

- `add` now validates the requested uORB instance and rejects instances that are not advertised.
- Example rejection: `uorb tunnel add rejected: topic=power_monitor instance=3 is not advertised`.
- This avoids silent misconfiguration where an entry exists in the list but never produces frames.

### MAVLink-only rail telemetry (`mavlinkproxy.py`)

Enable MAVLink stream from NSH:

```sh
mavlink stream -d /dev/ttyACM0 -s DEBUG_FLOAT_ARRAY -r 20
```

Each INA226 instance publishes one `DEBUG_FLOAT_ARRAY` packet (`name` = `ina2_4E` or `ina2_4F`):

- `data[0]` = rail voltage (`Vbus+`, V)
- `data[1]` = rail current (A)
- `data[2]` = rail power (W)
- `data[3]` = raw shunt register
- `data[4]` = raw bus register
- `data[5]` = raw current register
- `data[6]` = `CVRF` flag (`1`/`0`)
- `data[7]` = `OVF` flag (`1`/`0`)

Expected address hits in `i2cdetect`: `4e` and `4f`.

## 12) PCA9685 manual servo test commands

PCA9685 is started on I2C bus `2` at address `0x61`.

Quick checks:

```sh
i2cdetect -b 2
pca9685_pwm_out status
param show PCA9685_EN_BUS
param show PCA9685_I2C_ADDR
```

Expected address hit in `i2cdetect`: `61`.

### Channel mapping in this firmware

- CH0 (`PCA9685_FUNC1`) = throttle (`101`)
- CH1 (`PCA9685_FUNC2`) = steering (`201`)
- CH2 (`PCA9685_FUNC3`) = front differential (`203`)
- CH3 (`PCA9685_FUNC4`) = rear differential (`204`)
- CH4 (`PCA9685_FUNC5`) = gear (`202`)
- CH5 (`PCA9685_FUNC6`) = misc servo (`205`)
- CH6 (`PCA9685_FUNC7`) = misc servo (`206`)

Driveline neutral/arming baseline (from SVEA LLI Zephyr behavior):

- CH2 front differential trim = `1900` us (engaged)
- CH3 rear differential trim = `1100` us (engaged)
- CH4 gear trim = `1900` us (low gear)

Verify:

```sh
param show PCA9685_FUNC1
param show PCA9685_FUNC2
param show PCA9685_FUNC3
param show PCA9685_FUNC4
param show PCA9685_FUNC5
param show PCA9685_FUNC6
param show PCA9685_FUNC7
```

### Manual pulse test from NSH (safe, disarmed path)

Use `PCA9685_DISx` to drive fixed pulse width while disarmed.

Example: test steering on CH1 (`PCA9685_DIS2`):

```sh
param set PCA9685_DIS2 1000
param set PCA9685_DIS2 1500
param set PCA9685_DIS2 2000
```

Other channels:

- CH0 throttle: `PCA9685_DIS1`
- CH2 front diff: `PCA9685_DIS3`
- CH3 rear diff: `PCA9685_DIS4`
- CH4 gear: `PCA9685_DIS5`
- CH5 misc: `PCA9685_DIS6`
- CH6 misc: `PCA9685_DIS7`

Persist after tuning:

```sh
param save
```

Notes:

- Servo-style channels are configured for `1000..2000` us.
- Center/neutral is typically `1500` us.
- If you changed `PCA9685_FUNCx`, restore the mapping above before normal operation.


## Neopixel (WS2815F on `PC9` / `MB4PWM`)

Typical NSH bring-up and test commands:

Note, the leds are driven by 12V, to turn on 12V rail run `gpio write /dev/gpio13 1`

```sh
# start driver for 1 LED
neopixel start -n 1
neopixel status

# static colors
led_control on -l 0 -c red   -p 255
led_control on -l 0 -c green -p 255
led_control on -l 0 -c blue  -p 255

# blink pattern from led_control
led_control blink -l 0 -c red -s fast -n 0

# reset/off
led_control reset
led_control off -l 0

# stop driver
neopixel stop
```
