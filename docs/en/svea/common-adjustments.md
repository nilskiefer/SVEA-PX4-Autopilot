# Tune parameters in NSH {#common-adjustments-nsh}

Open the [PX4 console](quick-bringup.md#_4-open-nsh-over-mavlink-shell) on CN1 to inspect or change rover settings. To copy settings before a reset, use **Back up parameters** in the [browser flasher](flash.md).

## Set and save a parameter {#workflow}

Check the current value before changing it. Run `param save` to keep the change after reboot.

```sh
param show <PARAM_NAME>
param set <PARAM_NAME> <VALUE>
param save
```

## Acceleration and braking limits {#acceleration-and-deceleration-limits-rover}

Read the current launch and braking limits.

```sh
param show RO_ACCEL_LIM
param show RO_DECEL_LIM
```

Lower values soften the response. This example sets both limits to `0.8`.

```sh
# softer launch
param set RO_ACCEL_LIM 0.8

# softer braking/decel
param set RO_DECEL_LIM 0.8

param save
```

## Rover speed limit {#speed-limit-rover}

```sh
param show RO_SPEED_LIM
param set RO_SPEED_LIM 1.5
param save
```

## Restore selected defaults {#restore-a-parameter-to-default}

```sh
param reset RO_ACCEL_LIM
param reset RO_DECEL_LIM
param save
```

## Reset all parameters {#reset-all-parameters-use-carefully}

This resets saved settings and calibration to defaults. Back up the board first if you need to restore them.

```sh
param reset_all
param save
reboot
```

## Reboot after tuning changes

```sh
reboot
```
