# PMB3: Expanders and GPIO

PCAL6524 expanders expose pin devices as `/dev/gpio*`.

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

## Important Rails

- `/dev/gpio9`  ESC enable
- `/dev/gpio10` Servo TPS enable
- `/dev/gpio13` 12V buck enable

## Arming-Driven Power Gate

`svea_power_gate` controls ESC/servo rails.

- disarmed -> `/dev/gpio9=0`, `/dev/gpio10=0`
- armed -> `/dev/gpio9=1`, `/dev/gpio10=1`

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
