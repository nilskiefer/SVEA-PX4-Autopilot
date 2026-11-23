# Troubleshooting

Use **CN2** for CODEGRIP and **CN1** for PX4 serial. Each port appears as a different USB device.

## CODEGRIP connection and startup

If Chrome or Edge can't open the probe, check that the cable carries data and is plugged into CN2. Close OpenOCD or the CODEGRIP utility. On Linux, see **Linux USB permissions** in the [browser flasher](flash.md).

When the flasher says **Startup unconfirmed**, select **Check board** if CODEGRIP stayed connected. If the probe disconnected, reconnect CN2 and select **Connect CODEGRIP**. You can also check startup from the PX4 console on CN1.

For a CMSIS-DAP command mismatch in OpenOCD, see [Update CODEGRIP](build-and-flash.md#codegrip-firmware-pitfall).

## CN1 does not appear {#usb-not-detected}

Use the [runtime port commands](quick-bringup.md#_3-verify-usb-runtime-link-cn1) to check that your host sees the serial device. If the port is still missing, try the following in order.

1. Press board reset button.
2. Replug CN1 cable.
3. Power-cycle board.

## Open NSH directly {#nsh-access-without-qgroundcontrol}

Use the [MAVLink shell on CN1](quick-bringup.md#_4-open-nsh-over-mavlink-shell) to open NSH from a terminal.

## QGroundControl sees no telemetry {#connected-but-no-qgc-link}

Check the boot log for both messages.

- `sercon: Successfully registered the CDC/ACM serial driver`
- `INFO [mavlink] ... on /dev/ttyACM0 ...`

If a message is missing, inspect `/etc/init.d/rc.board_mavlink` and the startup log around `Board mavlink:`.

## Probe errors on a bare Clicker4 {#common-expected-errors-bringup-without-pmb3}

Without PMB3 attached, BQ, INA, PCAL, and PCA9685 probes can fail. The power gate can also report that its GPIO devices failed to open.

## Missing CPU and RAM load data {#cpu-ram-preflight-check-notes}

For `No CPU and RAM load information`, check the monitor and look for fresh `cpuload` samples.

```sh
load_mon status
listener cpuload -n 5
```

The board defaults are `COM_CPU_MAX=0` and `COM_RAM_MAX=0` while load reporting on this target is unresolved.

## Change a runtime parameter {#common-runtime-tuning-from-nsh}

[Tune parameters in NSH](common-adjustments.md) covers rover speed, acceleration and braking limits, and default values.
