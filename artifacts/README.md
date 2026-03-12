# Artifacts Folder

This folder is intended for host-side flashing when building inside a devcontainer.

## What gets copied here

PX4 NuttX builds now auto-copy firmware outputs to:

`artifacts/<target>/`

For example:

- `artifacts/mikroe_clicker4-stm32f7_bootloader/mikroe_clicker4-stm32f7_bootloader.bin`
- `artifacts/mikroe_clicker4-stm32f7_bootloader/mikroe_clicker4-stm32f7_bootloader.elf`
- `artifacts/mikroe_clicker4-stm32f7_bootloader/mikroe_clicker4-stm32f7_bootloader.px4`
- `artifacts/mikroe_clicker4-stm32f7_default/mikroe_clicker4-stm32f7_default.bin`
- `artifacts/mikroe_clicker4-stm32f7_default/mikroe_clicker4-stm32f7_default.elf`

Depending on target/config, `.hex` may also be present.

## Why this exists

When Docker on macOS does not expose USB to the devcontainer, build inside container and flash from macOS host using files in this folder.

## Build

Bootloader:

```bash
make mikroe_clicker4-stm32f7_bootloader
```

Application:

```bash
make mikroe_clicker4-stm32f7_default
```

## Flash from host (CodeGrip + OpenOCD)

Connect through CodeGrip (CMSIS-DAP), then run OpenOCD on host macOS.

Important: use paths relative to your current directory, or absolute paths.
Do not use `build/...` paths when you are already inside `artifacts/<target>/`.

### Bootloader (address `0x08000000`)

From `artifacts/mikroe_clicker4-stm32f7_bootloader`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program mikroe_clicker4-stm32f7_bootloader.bin 0x08000000 verify; reset run; shutdown"
```

### App fallback via OpenOCD (address `0x08020000`)

From `artifacts/mikroe_clicker4-stm32f7_default`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program mikroe_clicker4-stm32f7_default.bin 0x08020000 verify; reset run; shutdown"
```

## Preferred app update path

After bootloader is installed, preferred app flashing is PX4 uploader over USB CDC:

```bash
make mikroe_clicker4-stm32f7_default upload
```
