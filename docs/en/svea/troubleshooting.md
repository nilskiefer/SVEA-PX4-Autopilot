# Troubleshooting

## USB Not Detected

Linux:

```bash
ls /dev/serial/by-id/*SVEA*
ls /dev/ttyACM*
```

macOS:

```bash
ls /dev/cu.usbmodem*
```

If missing:

1. Press board reset button.
2. Replug CN1 cable.
3. Power-cycle board.

## NSH Access Without QGroundControl

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install mavproxy future
python3 ./Tools/mavlink_shell.py /dev/ttyACM0
```

## Connected but No QGC Link

In boot log, verify both:

- `sercon: Successfully registered the CDC/ACM serial driver`
- `INFO [mavlink] ... on /dev/ttyACM0 ...`

If missing, inspect `/etc/init.d/rc.board_mavlink` and startup log around `Board mavlink:`.

## Common Expected Errors (Bringup Without PMB3)

These can be expected when hardware is absent:

- BQ/INA/PCAL/PCA9685 probe failures
- power-gate GPIO open failures

## CPU/RAM Preflight Check Notes

If you still see `No CPU and RAM load information`, verify:

```sh
load_mon status
listener cpuload 5
```

If cpuload remains stale in this board setup, current operational default is:

- `COM_CPU_MAX=0`
- `COM_RAM_MAX=0`

Keep this as board-specific policy until cpuload publication path is fully resolved upstream or in-board config.

## Common Runtime Tuning from NSH

For frequent parameter tweaks (including accel/decel limits), use:

- [Common Adjustments (NSH)](common-adjustments.md)
