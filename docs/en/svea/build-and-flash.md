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

If OpenOCD fails with a CMSIS-DAP command mismatch or cannot talk to the CODEGRIP probe reliably, update the CODEGRIP firmware first. This has been the most common flashing issue with this board.

Do not rely on the displayed CODEGRIP `FW Version` string alone to decide whether the update worked; it may continue to report version `1` after the updater has completed.

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

## Release versions

The migration base is PX4 `v1.18.0-rc1` (`fca3df865af36124a28c9d607e850f111dbaaea9`).
Use immutable annotated downstream tags such as `v1.18.0-1.1.0-rc1`:
PX4 1.18.0, SVEA 1.1.0, release candidate 1. Record the exact upstream base
in the release description; the custom tag does not encode the upstream RC number.
Keep releases based on an upstream candidate marked as prereleases.

`v1.18.0-rc1-1.1.0` is not accepted by PX4's version validator.
The moving `svea-pmb-mikroe-latest` tag is for development builds only.

The upstream NuttX pin is used without local USB patches. Clicker4 owns I2C
startup through `BOARD_I2C_LATEINIT`: bus recovery is retained, while the
general-call device reset is omitted to preserve power-expander outputs.

SVEA TRX4 uses airframe ID `51004`. On Clicker4, the former saved SVEA ID
`51003` is migrated to `51004` at startup. Other saved airframe selections
are retained. Upstream `51003` remains the Hiwonder profile.
