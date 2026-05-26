# Troubleshooting and Connectivity

## USB Paths

- `CN2`: CODEGRIP debug/programming path (OpenOCD)
- `CN1`: PX4 USB CDC path (runtime MAVLink/console)

`noboot` firmware uses OpenOCD flashing and runtime USB CDC for host connection.

## Quick USB Checks (Linux)

```bash
ls /dev/serial/by-id/*SVEA*
ls /dev/ttyACM*
```

If no device appears:

1. Press the red reset button.
2. Power-cycle board if needed.

## NSH Without QGroundControl

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install mavproxy future
python3 ./Tools/mavlink_shell.py /dev/ttyACM0
```

Then in `nsh`:

```sh
dmesg
mavlink status
```

## MAVLink on USB CDC

Board startup uses explicit USB CDC bringup in `rc.board_mavlink`:

1. `sercon`
2. wait for `/dev/ttyACM0`
3. `mavlink start -d /dev/ttyACM0 ...`

Verify from boot log:

- `sercon: Successfully registered the CDC/ACM serial driver`
- `INFO [mavlink] ... on /dev/ttyACM0 ...`

## Typical Startup Failures

- Missing sensor hardware: preflight accel/gyro failures.
- Missing powerboard hardware: I2C probe failures for BQ/INA/PCAL/PCA9685.
- Serial tool mismatch: host sees USB device but no MAVLink heartbeat.
