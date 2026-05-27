# PMB3: Expanders and GPIO

## Address and Device Ranges

- Primary expander `0x22` -> `/dev/gpio0..23`
- Secondary expander `0x23` -> `/dev/gpio24..47`

## Basic Commands

```sh
gpio read /dev/gpio0
gpio write /dev/gpio0 1
gpio write /dev/gpio0 0
listener gpio_in
```

## GPIO Mapping

### Primary expander (`0x22`) -> `/dev/gpio0..23`

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

### Secondary expander (`0x23`) -> `/dev/gpio24..47`

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

## Rail-Related GPIOs (Complete)

| Device | Signal | Role | Device Node |
| --- | --- | --- | --- |
| Primary `0x22` | Power/5V-Buck/EN# | 5V rail enable control | `/dev/gpio7` |
| Primary `0x22` | Power/5V-Buck/PGOOD | 5V rail power-good status | `/dev/gpio4` |
| Primary `0x22` | Power/12V-Buck/EN | 12V rail enable control | `/dev/gpio13` |
| Primary `0x22` | ESC/EN | ESC rail enable control | `/dev/gpio9` |
| Primary `0x22` | Servo/TPS/EN | Servo rail enable control | `/dev/gpio10` |
| Primary `0x22` | Servo/TPS/PGOOD | Servo rail power-good status | `/dev/gpio11` |
| Primary `0x22` | eFuse/TPS16630/SHDN | eFuse shutdown control | `/dev/gpio22` |
| Primary `0x22` | eFuse/TPS16630/PGOOD | eFuse power-good status | `/dev/gpio16` |
| Primary `0x22` | eFuse/TPS16630/FAULT | eFuse fault status | `/dev/gpio23` |
| Primary `0x22` | Receiver/EN | Receiver rail/control enable | `/dev/gpio17` |
| Primary `0x22` | USB-C/HUSB238A/EN# | USB-C PD path enable | `/dev/gpio2` |
| Primary `0x22` | BQ/RST-SHUT | BMS reset/shutdown control line | `/dev/gpio1` |
| Primary `0x22` | BQ/ALERT/LED | BMS alert/status line | `/dev/gpio0` |
| Secondary `0x23` | USB-C/HUSB238A/FAULT-OUT2 | USB-C PD fault status | `/dev/gpio40` |
| Secondary `0x23` | USB-C/HUSB238A/INT | USB-C PD interrupt/status | `/dev/gpio43` |
| Secondary `0x23` | INA226-ESC-0x4E/ALERT | ESC rail current monitor alert | `/dev/gpio32` |
| Secondary `0x23` | INA226-SERVO-0x4F/ALERT | Servo rail current monitor alert | `/dev/gpio33` |
| Secondary `0x23` | INA3221-CUR1/CUR2 flags | Multi-rail current monitor alert flags | `/dev/gpio24..31` |
| Secondary `0x23` | Charger/CN3072/CHG-DONE | Charger status done | `/dev/gpio37` |
| Secondary `0x23` | Charger/CN3072/CHG-ACTIVE | Charger active status | `/dev/gpio38` |

## Important Rails

- `/dev/gpio9` ESC enable
- `/dev/gpio10` Servo TPS enable
- `/dev/gpio13` 12V buck enable

## Arming-Driven Power Gate

`svea_power_gate` controls ESC/servo rails.

- armed and not killed/lockdown/termination -> servo on then ESC on
- disarmed -> servo rail off always
- disarmed with RC signal lost -> ESC rail also forced off

Checks:

```sh
svea_power_gate status
gpio read /dev/gpio9
gpio read /dev/gpio10
```

## Safe Toggle Examples

```sh
# ESC rail
gpio write /dev/gpio9 1
gpio write /dev/gpio9 0

# 12V rail
gpio write /dev/gpio13 1
gpio write /dev/gpio13 0
```
