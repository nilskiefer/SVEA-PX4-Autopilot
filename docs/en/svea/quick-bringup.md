# Quick Bringup

## 0) Environment First (Required)

Complete this before anything else:

- [Dev Environment (VS Code + Devcontainer)](dev-environment.md)

Especially on Linux: wait for and accept all trust/unsafe-repository prompts in VS Code.

## 1) Build

```bash
make mikroe_clicker4-stm32f7_noboot
```

## 2) Flash (CN2 / CODEGRIP)

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/stm32f7x.cfg \
  -c "init; reset halt; program build/mikroe_clicker4-stm32f7_noboot/mikroe_clicker4-stm32f7_noboot.bin 0x08000000 verify; reset run; shutdown"
```

## 3) Verify USB runtime link (CN1)

Linux:

```bash
ls /dev/serial/by-id/*SVEA*
ls /dev/ttyACM*
```

macOS:

```bash
ls /dev/cu.usbmodem*
```

## 4) Open NSH over MAVLink shell

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install mavproxy future
python3 ./Tools/mavlink_shell.py /dev/ttyACM0
```

Replace device path for macOS if needed.

## 5) Sanity checks in NSH

```sh
dmesg
mavlink status
listener cpuload 1
```

## 6) If you run without PMB3 hardware

Missing PMB3 peripherals will produce expected I2C/probe errors. That is normal for bringup without those boards.

## 7) Next

- For stable flashing flow details: [Build and Flash](build-and-flash.md)
- For failure diagnosis: [Troubleshooting](troubleshooting.md)
- For PMB3 mapping and commands: [PMB3 Overview](pmb3/index.md)
