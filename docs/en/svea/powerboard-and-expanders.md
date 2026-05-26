# Powerboard and Expanders

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

