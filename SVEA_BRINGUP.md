# SVEA Clicker4 STM32F7 Bringup

This document captures the working bringup flow for:

- Board: `MIKROE_CLICKER4_STM32F7`
- PX4 target: `mikroe_clicker4-stm32f7`
- Probe: CODEGRIP (`CMSIS-DAP`)

## 1) Prerequisites

- You are in repo root: `/workspaces/SVEA-PX4-Autopilot`
- Probe is visible in `lsusb` as `MikroElektronika CODEGRIP-OneMcu [CMSIS-DAP]`
- `openocd` is installed
- Use **USB-C CN2** (debug/CMSIS-DAP path) for OpenOCD flashing
- Use **USB-C CN1** (target USB CDC path) for `make ... upload`
- UART debug console is on **MB1** header: **PA2 (TX)** / **PA3 (RX)** (`ttyS0`)

## 2) Build and flash bootloader (OpenOCD)

Build:

```bash
make mikroe_clicker4-stm32f7_bootloader
```

Flash bootloader at flash base `0x08000000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_bootloader/mikroe_clicker4-stm32f7_bootloader.bin 0x08000000 verify; reset run; shutdown"
```

## 3) Build and flash PX4 app

Build app:

```bash
make mikroe_clicker4-stm32f7_default
```

Preferred flash path (through PX4 bootloader over USB CDC):

```bash
make mikroe_clicker4-stm32f7_default upload
```

Expected success line:

```text
Found board 7454,0 protocol v5 ...
Uploaded in <N>s
```

## 4) Fallback: flash app with OpenOCD

If `upload` cannot find bootloader, program app directly at `0x08020000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_default/mikroe_clicker4-stm32f7_default.bin 0x08020000 verify; reset run; shutdown"
```

## 5) USB behavior notes

- This setup has two USB paths in practice:
  - CODEGRIP debug interface (`2dbc:*`)
  - PX4 bootloader/app CDC (`26ac:0050` during bootloader)
- Seeing the bootloader enumerate briefly, then disappear, is normal if app starts.

## 6) Console + MAVLink split

- Keep UART (`PA2/PA3`, `ttyS0`) for readable NSH/debug text
- Keep MAVLink on USB CDC (`/dev/ttyACM0`)

From `nsh>`:

```sh
param set MAV_0_CONFIG 0
param set MAV_1_CONFIG 0
param set MAV_2_CONFIG 0
param set SYS_USB_AUTO 2
param save
reboot
```

Check:

```sh
mavlink status
```

## 7) LED/Button test modules

Current board startup launches:

- `led_bus_worker`
- `led_pattern`
- `button_led_mirror`

Manual control from `nsh>`:

```sh
led_pattern stop
led_pattern start -t 120 -n 6
button_led_mirror status
led_bus_worker status
```
