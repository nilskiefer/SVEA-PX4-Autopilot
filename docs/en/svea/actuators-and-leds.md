# Actuators, LEDs, and Manual Tests

## 6) LED/Button test modules

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

## 6.1) Neopixel (WS2815F on PC9 / MB4PWM)

Start neopixel driver and run basic color checks:

```sh
neopixel start -n 1
neopixel status

led_control on -l 0 -c red   -p 255
led_control on -l 0 -c green -p 255
led_control on -l 0 -c blue  -p 255

led_control blink -l 0 -c red -s fast -n 0
```

Stop/reset:

```sh
led_control reset
led_control off -l 0
neopixel stop
```

## 6.2) Neopixel FX module (`neopixel_fx`)

`neopixel_fx` is a small effect module that publishes animated `led_control` patterns.

Ownership behavior:

- `neopixel_fx` requires `neopixel` driver running (`neopixel start -n 1`).
- While `neopixel_fx` runs, the neopixel driver's `led_control` intake is temporarily disabled.
- This prevents neopixel output contention, while other LED drivers on the system can still use `led_control`.
- On `neopixel_fx stop`, normal neopixel `led_control` handling is restored automatically.

Examples:

```sh
# rainbow cycle
neopixel_fx start -m rainbow -t 120 -l 0 -p 2
neopixel_fx status

# red/blue flash
neopixel_fx stop
neopixel_fx start -m police -t 90 -l 0 -p 2

# breathe while cycling colors
neopixel_fx stop
neopixel_fx start -m breathe -t 350 -l 0 -p 2

# stop effect
neopixel_fx stop
led_control off -l 0
```

## 7) GNSS on mikroBUS 2/3

- You can connect a GNSS module on **mikroBUS 2** or **mikroBUS 3**.
- Start GNSS manually from `nsh>` with:

```sh
gps stop
gps start -d /dev/ttyS0 -b 115200
gps status
listener sensor_gps 1
```


## 12) PCA9685 manual servo test commands

PCA9685 is started on I2C bus `2` at address `0x61`.

Quick checks:

```sh
i2cdetect -b 2
pca9685_pwm_out status
param show PCA9685_EN_BUS
param show PCA9685_I2C_ADDR
```

Expected address hit in `i2cdetect`: `61`.

### Channel mapping in this firmware

- CH0 (`PCA9685_FUNC1`) = throttle (`101`)
- CH1 (`PCA9685_FUNC2`) = steering (`201`)
- CH2 (`PCA9685_FUNC3`) = front differential (`203`)
- CH3 (`PCA9685_FUNC4`) = rear differential (`204`)
- CH4 (`PCA9685_FUNC5`) = gear (`202`)
- CH5 (`PCA9685_FUNC6`) = misc servo (`205`)
- CH6 (`PCA9685_FUNC7`) = misc servo (`206`)

Driveline neutral/arming baseline (from SVEA LLI Zephyr behavior):

- CH2 front differential trim = `1900` us (engaged)
- CH3 rear differential trim = `1100` us (engaged)
- CH4 gear trim = `1900` us (low gear)

Verify:

```sh
param show PCA9685_FUNC1
param show PCA9685_FUNC2
param show PCA9685_FUNC3
param show PCA9685_FUNC4
param show PCA9685_FUNC5
param show PCA9685_FUNC6
param show PCA9685_FUNC7
```

### Manual pulse test from NSH (safe, disarmed path)

Use `PCA9685_DISx` to drive fixed pulse width while disarmed.

Example: test steering on CH1 (`PCA9685_DIS2`):

```sh
param set PCA9685_DIS2 1000
param set PCA9685_DIS2 1500
param set PCA9685_DIS2 2000
```

Other channels:

- CH0 throttle: `PCA9685_DIS1`
- CH2 front diff: `PCA9685_DIS3`
- CH3 rear diff: `PCA9685_DIS4`
- CH4 gear: `PCA9685_DIS5`
- CH5 misc: `PCA9685_DIS6`
- CH6 misc: `PCA9685_DIS7`

Persist after tuning:

```sh
param save
```

Notes:

- Servo-style channels are configured for `1000..2000` us.
- Center/neutral is typically `1500` us.
- If you changed `PCA9685_FUNCx`, restore the mapping above before normal operation.


## Neopixel (WS2815F on `PC9` / `MB4PWM`)

Typical NSH bring-up and test commands:

Note, the leds are driven by 12V, to turn on 12V rail run `gpio write /dev/gpio13 1`

```sh
# start driver for 1 LED
neopixel start -n 1
neopixel status

# static colors
led_control on -l 0 -c red   -p 255
led_control on -l 0 -c green -p 255
led_control on -l 0 -c blue  -p 255

# blink pattern from led_control
led_control blink -l 0 -c red -s fast -n 0

# reset/off
led_control reset
led_control off -l 0

# stop driver
neopixel stop
```
