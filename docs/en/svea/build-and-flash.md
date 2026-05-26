# SVEA Clicker4 STM32F7 Bringup

This document captures the working bringup flow for:

- Board: `MIKROE_CLICKER4_STM32F7`
- PX4 target: `mikroe_clicker4-stm32f7`
- Probe: CODEGRIP (`CMSIS-DAP`)

This bringup uses the `noboot` firmware target only.

## 1) Prerequisites

- You are in repo root: `/workspaces/SVEA-PX4-Autopilot`
- Probe is visible in `lsusb` as `MikroElektronika CODEGRIP-OneMcu [CMSIS-DAP]`
- `openocd` is installed
- Use **USB-C CN2** (debug/CMSIS-DAP path) for OpenOCD flashing
- UART debug console is on **MB1** header: **PA2 (TX)** / **PA3 (RX)** (`ttyS0`)

## 2) Build `noboot` firmware

Build:

```bash
make mikroe_clicker4-stm32f7_noboot
```

## 3) Flash firmware with OpenOCD (CN2)

Flash app at flash base `0x08000000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_noboot/mikroe_clicker4-stm32f7_noboot.bin 0x08000000 verify; reset run; shutdown"
```
This is via the usb c port marked CN2

If you are having issues it might be because the flash chip's firmware needs to be update:

### 3.1. Update the mikroE flasher firmware
If you are having trouble flashing with openocd the likely cause is autodated flasher firmware.

If you get errors like
`CMSIS-DAP command mismatch. Sent 0x10 received 0x0`
and you see a line like
`Info : CMSIS-DAP: FW Version = 1.0`

Then this will likely solve the issue:

Install CODEGRIP from:

https://www.mikroe.com/codegrip

The download link is near the bottom of the page.

Follow these instructions [https://helpdesk.mikroe.com](https://helpdesk.mikroe.com/en-us/10-codegrip/79-how-to-update-codegrip-device-firmware)

### 3.2 Testing/troubleshooting USB connection
Connect USB cable from CN1 to a computer running Linux (eg jetson).

Running in terminal
`ls /dev/serial/by-id/*SVEA*`
Should show something like
`/dev/serial/by-id/usb-SVEA_PX4_AUTOPILOT_0-if00`

If not try pressing large RED button on the board (this resets the microcontroller but shouldn't turn off raspberry/jetson)
If not try flipping the off and on switch on the mikroe board (this will likely turn off the raspberry/jetson)

### 3.3 NSH access without QGroundControl (MAVLink shell)
If QGroundControl is not available on your PC, you can still access `nsh` and run `dmesg` over MAVLink.

Create and enter a Python virtual environment (optional):

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Install required Python packages:

```bash
pip install mavproxy future
```

Find the USB serial device:

- Linux (preferred stable path):

```bash
ls /dev/serial/by-id/*SVEA*
```

- Linux fallback:

```bash
ls /dev/ttyACM*
```

Run MAVLink shell (replace device if needed):

```bash
python3 ./Tools/mavlink_shell.py /dev/ttyACM0
```

You should now see an `nsh` console. Example:

```sh
dmesg
```

This is useful for debugging arming failures and startup issues.
