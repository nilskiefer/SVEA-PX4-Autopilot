# Set up the Clicker4 {#quick-bringup}

Use **CN2** to flash the firmware through CODEGRIP. Once PX4 is running, use **CN1** for the USB serial connection and the PX4 console.

## Flash firmware {#_2-flash-cn2-codegrip}

Connect a USB data cable to CN2 and open the [browser flasher](flash.md) in desktop Chrome or Edge. Select **Connect CODEGRIP**, then choose the release, prerelease, or your own `.bin` and select **Flash firmware**.

For a build from source, start with the [build environment](dev-environment.md), then [build and flash the firmware](build-and-flash.md#build).

## Find the runtime port {#_3-verify-usb-runtime-link-cn1}

Connect the USB data cable to CN1. On Linux or macOS, look for the serial device.

::: code-group

```bash [Linux]
ls /dev/serial/by-id/*SVEA*
ls /dev/ttyACM*
```

```bash [macOS]
ls /dev/cu.usbmodem*
```

:::

If the port is missing, see [CN1 does not appear](troubleshooting.md#usb-not-detected).

## Open the PX4 console {#_4-open-nsh-over-mavlink-shell}

Run these commands from the firmware repository on your host. Replace `/dev/ttyACM0` with the CN1 port you found above.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install mavproxy future
python3 ./Tools/mavlink_shell.py /dev/ttyACM0
```

## Check that PX4 is running {#_5-sanity-checks-in-nsh}

Once you have the `nsh>` prompt, check the boot log, MAVLink connection, and CPU load.

```sh
dmesg
mavlink status
listener cpuload -n 1
```

If you're testing a bare Clicker4, expect [warnings about missing PMB3 peripherals](troubleshooting.md#common-expected-errors-bringup-without-pmb3).
