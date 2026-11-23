# LEDs {#pmb3-leds}

To enable Neopixel commands, set `CONFIG_DRIVERS_LIGHTS_NEOPIXEL` and
`CONFIG_SYSTEMCMDS_LED_CONTROL` in the Clicker4 board config. Then
[build and flash](../build-and-flash.md) that image.

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
