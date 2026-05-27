# PMB3: LEDs

This page covers neopixel and pattern modules.

## Modules

Startup includes:

- `led_bus_worker`
- `led_pattern`
- `button_led_mirror`

Checks:

```sh
led_bus_worker status
button_led_mirror status
```

## Neopixel Bringup

If your LED rail depends on 12V, enable rail first:

```sh
gpio write /dev/gpio13 1
```

Then:

```sh
neopixel start -n 1
neopixel status
led_control on -l 0 -c red -p 255
led_control on -l 0 -c green -p 255
led_control on -l 0 -c blue -p 255
```

Blink/reset:

```sh
led_control blink -l 0 -c red -s fast -n 0
led_control reset
led_control off -l 0
neopixel stop
```

## `neopixel_fx` Effects

`neopixel_fx` owns neopixel output while active.

```sh
neopixel_fx start -m rainbow -t 120 -l 0 -p 2
neopixel_fx status
neopixel_fx stop
```
