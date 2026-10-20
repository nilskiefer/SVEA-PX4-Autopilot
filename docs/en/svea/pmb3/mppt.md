# CN3722 MPPT Setpoint

`svea_mppt` controls the TPL0401B-10 at **I2C bus 2, 0x3E (62)**.
It starts automatically on the Clicker4 builds.

| Parameter | Default | Meaning |
| --- | --- | --- |
| `SVEA_MPPT_EN` | `0` | `0`: fixed divider; `1`: digital adjustment |
| `SVEA_MPPT_V` | `18.0` | Input-voltage target in volts, `7.66..18.98` |

This sets the CN3722 input-voltage target. It does not run an adaptive MPPT algorithm or switch charging on/off.

## Wiring and polarity

Primary expander `0x22`, P0_6 (`/dev/gpio6`) controls Q601 through a 1 kΩ series resistor.

- **Released/input:** R610 pulls Q601's gate up; Q601 bypasses the digipot. Fixed target: approximately **19.1 V**.
- **Driven low:** Q601 turns off; the digipot adjusts the divider.

The driver uses output-low/input to emulate open drain. It never drives this signal high.
Charger enable is separate: `/dev/gpio14`.

At 25°C, the nominal target is:

```text
Vin_target ≈ 1.04 × (1 + 100000 / (5760 + Rdigipot))
Rlo = code × 10000 / 128 Ω
Rhi = 100 + 10000 - Rlo Ω
Rdigipot ≈ Rlo + (35 × Rhi) / (35 + Rhi) Ω
```

H and W are tied together. The nominal model includes TI's typical 100 Ω H-terminal and 35 Ω wiper resistance; these are not calibration values. The digipot's end-to-end resistance tolerance is ±20%.
An 18 V request selects code `4`, nominally **18.069 V**. Code `5` is nominally **17.854 V**.
Code `0` gives roughly 19 V; code `64` roughly 10.7 V; code `127` roughly 7.7 V.
The CN3722 is a buck charger: the chosen input target must leave sufficient headroom above the battery voltage.

## Input eFuse limits

The schematic's TPS16630 UVLO divider (`R1904=442 kΩ`, `R1905=11 kΩ`, `R1906=20 kΩ`) gives approximately **18.31 V turn-on** and **17.12 V turn-off**, before tolerances.
An 18 V MPPT target provides limited undervoltage margin; input droop or transients can cause cutouts.
The default open `JP1902` gives a nominal **3.27 A** input-current limit (`3.92 kΩ + 1.58 kΩ`); closed gives **4.59 A**.

The MPPT target is an input-voltage regulation point, not a guaranteed measured input or charger output voltage. A stiff 20 V supply may stay at 20 V while the charger draws more current.
Confirm U62 input/output voltages and FAULT before attributing a cutout to undervoltage or overcurrent. The parameter range describes the digipot model, not a guaranteed operating range through the eFuse.

## Set and save

Set an 18 V target:

```sh
param set SVEA_MPPT_V 18
param set SVEA_MPPT_EN 1
param save
svea_mppt status
```

The driver calculates the nearest nominal wiper setting. `status` shows the requested and quantized voltage targets; neither is a voltage measurement. Component tolerances require verification on the board.

20 V is outside this divider's range and is rejected; it requires a hardware change.

Changes apply within one second. The driver writes register `0x00` and checks its readback before requesting digital adjustment.
It checks the wiper every second. Invalid parameters, I2C/readback errors or GPIO request failures latch a fault and request bypass; restart the driver after correcting the cause.
GPIO requests are asynchronous; `status` reports the requested state, not confirmation from the circuit.

Restore the fixed divider:

```sh
param set SVEA_MPPT_EN 0
param save
```

Restart after a fault:

```sh
svea_mppt stop
svea_mppt -I -b 2 -a 0x3e start
```

The chip's wiper is volatile and powers up at code `64`; saved PX4 parameters reapply the setting at boot.

## Electrical limits

TI requires VDD before SDA, SCL, H and W; analog terminals must remain between GND and VDD during operation.
The VIN pull-up, zener and Q601 provide a default bypass, but the schematic alone does not prove compliant startup/shutdown sequencing.
Measure U601 VDD and H/W through power-up, power-down and loss of 3.3 V with charger input present before relying on digital adjustment.

Sources: [TI TPL0401A/B datasheet](https://www.ti.com/lit/ds/symlink/tpl0401a-10.pdf), [CN3722 datasheet](https://consonance-elec.com/static/upload/file/20231218/1702867485736775.pdf), [TPS1663 datasheet](https://www.ti.com/lit/ds/symlink/tps1663.pdf).
