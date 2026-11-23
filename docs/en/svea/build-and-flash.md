# Build and flash firmware {#build-and-flash}

Build the `noboot` target to flash your own Clicker4 image. For a published release or prerelease, open the [browser flasher](flash.md).

## Build

### Prerequisites

Set up the [build environment](dev-environment.md), then run this in the devcontainer from the firmware repository.

```bash
make mikroe_clicker4-stm32f7_noboot
```

The raw image is written to the following path.

```text
build/mikroe_clicker4-stm32f7_noboot/mikroe_clicker4-stm32f7_noboot.bin
```

## Flash

### USB ports {#hardware-paths}

Connect **CN2** to your host using a USB data cable. For the runtime connection after flashing, [use CN1](quick-bringup.md#_3-verify-usb-runtime-link-cn1).

### Chrome or Edge

In the [browser flasher](flash.md), select **Custom .bin** and choose the image from the build directory. The application may occupy up to 768 KiB. The final flash sector holds saved parameters.

### OpenOCD

Install OpenOCD on the host and check that CODEGRIP appears as a CMSIS-DAP probe. Run the command from the firmware repository on the host.

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_noboot/mikroe_clicker4-stm32f7_noboot.bin 0x08000000 verify; reset run; shutdown"
```

### CODEGRIP command mismatch {#codegrip-firmware-pitfall}

If OpenOCD reports a CMSIS-DAP command mismatch or keeps losing the probe, update CODEGRIP first. The reported `FW Version` can stay at `1` after a successful update. [CODEGRIP utility](https://www.mikroe.com/codegrip) · [Firmware update instructions](https://helpdesk.mikroe.com/en-us/10-codegrip/79-how-to-update-codegrip-device-firmware)

## Check the board after flashing {#post-flash-validation}

On CN2, **Check board** reads the build ID and startup status over CODEGRIP. On CN1, use the [PX4 console checks](quick-bringup.md#_5-sanity-checks-in-nsh) to confirm the runtime connection.

## Restore a parameter backup

In the [browser flasher](flash.md), connect through CN2 and check the installed firmware. Choose a 256 KiB `.bin` saved from the same board and firmware version, then select **Restore parameters**. The flasher checks the PX4 record before writing and reads the restored sector back before rebooting.

::: details Restore with OpenOCD

Use the full sector backup from the same board and firmware version. Rename it to `clicker4-parameters.bin` and run the command from the folder containing the file with CN2 connected.

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_vid_pid 0x2dbc 0x1924; cmsis_dap_backend hid; transport select swd" \
  -f target/stm32f7x.cfg \
  -c "if {[file size clicker4-parameters.bin] != 0x40000} {error {Expected a 256 KiB parameter backup}}" \
  -c "program clicker4-parameters.bin 0x080c0000 verify reset exit"
```

:::

## Release versions

The migration base is PX4 `v1.18.0-rc1` (`fca3df865af36124a28c9d607e850f111dbaaea9`).
Tag downstream releases with immutable annotated tags such as `v1.18.0-1.1.0-rc1` for PX4 1.18.0, SVEA 1.1.0, release candidate 1. Record the upstream commit in the release description and mark upstream candidate builds as prereleases.

The PX4 version validator rejects `v1.18.0-rc1-1.1.0`. Reserve the moving `svea-pmb-mikroe-latest` tag for development builds.

::: details Clicker4 changes in the v1.18 migration

The build uses the upstream NuttX pin without local USB patches. Clicker4 runs I2C startup through `BOARD_I2C_LATEINIT`. Bus recovery still runs and the power-expander outputs are preserved by skipping the general-call reset.

SVEA TRX4 uses airframe ID `51004`. Clicker4 migrates a saved SVEA ID `51003` to `51004` at startup and keeps other saved airframes. Upstream uses `51003` for Hiwonder.

When downgrading a TRX4 with saved ID `51004` from `v1.18.0-1.1.0-rc1` to `v1.17.0-1.0.0`, select the stable release's SVEA airframe in NSH after flashing.

```sh
param set SYS_AUTOSTART 51003
param save
reboot
```

:::
