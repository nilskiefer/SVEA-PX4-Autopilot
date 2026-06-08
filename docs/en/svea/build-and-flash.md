# Build and Flash

## Hardware Paths

- `CN2`: CODEGRIP / OpenOCD flash path
- `CN1`: runtime USB CDC (`/dev/ttyACM*` / `/dev/cu.usbmodem*`)

## Prerequisites

- Repo checked out locally
- `openocd` installed
- CODEGRIP visible as CMSIS-DAP

## Build

```bash
make mikroe_clicker4-stm32f7_noboot
```

## Flash

For macOS/Windows flashing with OpenOCD:

- Run the command on the host (not inside the devcontainer).
- Run it from the repository root on the host.

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_noboot/mikroe_clicker4-stm32f7_noboot.bin 0x08000000 verify; reset run; shutdown"
```

## CODEGRIP Firmware Pitfall

If OpenOCD fails with CMSIS-DAP command mismatch and CODEGRIP reports old firmware (for example `FW Version = 1.0`), update CODEGRIP firmware first.

- Tool: [mikroE CODEGRIP](https://www.mikroe.com/codegrip)
- Guide: [CODEGRIP firmware update](https://helpdesk.mikroe.com/en-us/10-codegrip/79-how-to-update-codegrip-device-firmware)

## Post-Flash Validation

1. Check runtime USB device on CN1.
2. Open NSH using [Quick Bringup](quick-bringup.md).
3. Run:

```sh
dmesg
mavlink status
```
