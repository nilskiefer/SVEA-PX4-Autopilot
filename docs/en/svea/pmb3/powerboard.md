# PMB3: Powerboard Baseline

## Battery Baseline

Defaults from board startup:

- `BAT1_SOURCE=1`
- `BAT1_CAPACITY=9000`
- `BAT1_N_CELLS=3`
- `BAT1_V_CHARGED=4.2`
- `BAT1_V_EMPTY=3.2`

Inspect:

```sh
param show BAT1_*
```

## BQ769x2 Baseline

Driver and address:

- `SENS_EN_BQ769X2=1`
- `BQ769X2_ADDR=8` (`0x08`)
- `BQ769X2_CELLS=3`

Inspect all BQ params:

```sh
param show BQ769X2_*
```

## INA226 Rail Monitors

Expected instances:

- Bus `2`, addr `0x4E`, shunt `0.001` (ESC rail)
- Bus `2`, addr `0x4F`, shunt `0.005` (servo rail)

Checks:

```sh
i2cdetect -b 2
svea_ina226 status
listener power_monitor 5
```

## MAVLink Tunnel for `power_monitor`

Add only existing instances:

```sh
listener power_monitor
mavlink uorb_tunnel add -t power_monitor -i 0 -r 10
mavlink uorb_tunnel add -t power_monitor -i 1 -r 10
mavlink uorb_tunnel list
```

If invalid instance is requested, it is rejected.
