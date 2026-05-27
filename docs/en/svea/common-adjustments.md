# Common Adjustments (NSH)

## Workflow

1. Check current value:

```sh
param show <PARAM_NAME>
```

2. Set new value:

```sh
param set <PARAM_NAME> <VALUE>
```

3. Persist:

```sh
param save
```

## Acceleration and Deceleration Limits (Rover)

Use these parameters to tune longitudinal behavior:

```sh
param show RO_ACCEL_LIM
param show RO_DECEL_LIM
```

Example tuning:

```sh
# softer launch
param set RO_ACCEL_LIM 0.8

# softer braking/decel
param set RO_DECEL_LIM 0.8

param save
```

Higher values -> more aggressive response.  
Lower values -> smoother response.

## Speed Limit (Rover)

```sh
param show RO_SPEED_LIM
param set RO_SPEED_LIM 1.5
param save
```

## Restore a Parameter to Default

```sh
param reset RO_ACCEL_LIM
param reset RO_DECEL_LIM
param save
```

## Reset All Parameters (Use Carefully)

```sh
param reset_all
param save
reboot
```

This resets the full parameter set to defaults.

## Reboot After Tuning Changes

```sh
reboot
```
