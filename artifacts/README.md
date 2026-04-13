# Artifacts Folder

This folder is for host-side flashing when you build inside a devcontainer, or whenever you want a stable place on the host to collect final firmware files.

## Which flashing path should I use?

There are two valid setups for this board:

1. `noboot`: no PX4 bootloader on the board.
2. `bootloader + default`: PX4 bootloader installed, then normal PX4 app installed behind it.

Use exactly one of them.

## Scenario 1: No PX4 Bootloader (`noboot`)

Use this when:

- you do not want a PX4 bootloader on the board
- you flash only with OpenOCD / SWD
- you want the PX4 app linked at flash base

Build:

```bash
make mikroe_clicker4-stm32f7_noboot
```

Flash the app directly at `0x08000000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program mikroe_clicker4-stm32f7_noboot.bin 0x08000000 verify; reset run; shutdown"
```

Notes:

- `noboot` is linked for `0x08000000`.
- Do not flash `mikroe_clicker4-stm32f7_default.bin` at `0x08000000`.
- Do not expect `make ... upload` over PX4 USB bootloader in this setup.

## Scenario 2: PX4 Bootloader + Normal App (`bootloader` + `default`)

Use this when:

- you want the PX4 bootloader on the board
- you want later app updates through PX4 USB upload
- you want the normal split layout: bootloader first, app after it

### Step 1: Build and flash the bootloader

Build:

```bash
make mikroe_clicker4-stm32f7_bootloader
```

Flash bootloader at `0x08000000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program mikroe_clicker4-stm32f7_bootloader.bin 0x08000000 verify; reset run; shutdown"
```

### Step 2: Build the normal PX4 app

Build:

```bash
make mikroe_clicker4-stm32f7_default
```

The normal app is linked for `0x08020000`.

### Step 3a: Preferred app update path

If the PX4 bootloader is installed and visible over USB CDC, use:

```bash
make mikroe_clicker4-stm32f7_default upload
```

This is the preferred update path after the bootloader is on the board.

### Step 3b: Fallback app flashing with OpenOCD

If USB upload is not working, flash the normal app directly at `0x08020000`:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program mikroe_clicker4-stm32f7_default.bin 0x08020000 verify; reset run; shutdown"
```

## Quick decision table

- No bootloader on board: build `mikroe_clicker4-stm32f7_noboot`, flash app to `0x08000000`.
- Bootloader on board and you are installing it the first time: flash `mikroe_clicker4-stm32f7_bootloader.bin` to `0x08000000`, then flash or upload `mikroe_clicker4-stm32f7_default`.
- Bootloader already installed and working: use `make mikroe_clicker4-stm32f7_default upload`.
- Bootloader installed but USB upload not working: flash `mikroe_clicker4-stm32f7_default.bin` to `0x08020000`.

## Important address summary

- `mikroe_clicker4-stm32f7_bootloader.bin` -> `0x08000000`
- `mikroe_clicker4-stm32f7_default.bin` -> `0x08020000`
- `mikroe_clicker4-stm32f7_noboot.bin` -> `0x08000000`

## USB notes

- Use the CODEGRIP / CMSIS-DAP path for OpenOCD.
- Use the target USB CDC path for `make ... upload`.
- Seeing the PX4 bootloader enumerate briefly and then disappear is normal if the app starts immediately.
