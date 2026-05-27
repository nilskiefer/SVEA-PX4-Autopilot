# Manual Control Gating (ROS vs RC)

## What Is Gated

In this fork, MAVLink `MANUAL_CONTROL` acceptance is gated by RC mode slot.

Source path:

- `src/modules/mavlink/mavlink_receiver.cpp`

## Gate Rule

`MANUAL_CONTROL` is accepted only when:

- `manual_control_switches` is valid, and
- `mode_slot == MODE_SLOT_1`
- RC remains fallback (via `COM_RC_IN_MODE=6`)

If not, MAVLink manual-control packets are rejected.

## RC_CHANNELS_OVERRIDE

In this fork, `RC_CHANNELS_OVERRIDE` is also accepted only in `MODE_SLOT_1`.

Other mode slots reject MAVLink manual input paths.

## What Counts As Valid (Timeout + Rate)

Manual input validity is timeout-based via `COM_RC_LOSS_T` (seconds), not a fixed hardcoded Hz.

- input is considered valid only while fresh within `COM_RC_LOSS_T`
- if no new command arrives before timeout, MAVLink manual input is considered stale
- when stale, control falls back to RC (with `COM_RC_IN_MODE=6`)

Board default:

- `COM_RC_LOSS_T=0.5`

This means a new MAVLink manual command must arrive at least every `0.5 s`.

- theoretical minimum to avoid fallback: `> 2 Hz`
- practical recommendation: `20 Hz` (use `10-20 Hz` minimum in real operation)

## Board Defaults That Pair With This

`rc.board_defaults` configures:

- `COM_RC_IN_MODE=6`
- flight mode mapping intended as:
  - low switch -> `MODE_SLOT_1` -> MAVLink manual control allowed
  - middle -> `MODE_SLOT_2` -> RC-only behavior
  - high -> kill path

## Practical Check

```sh
param show COM_RC_IN_MODE
param show COM_RC_LOSS_T
param show COM_FLTMODE1
param show COM_FLTMODE2
param show COM_FLTMODE3
listener manual_control_switches 1
```

Board defaults keep RC as fallback when MAVLink manual input is stale/lost.

In practice:

- `MODE_SLOT_1`: MAVLink `MANUAL_CONTROL` accepted, RC fallback active
- other mode slots: MAVLink `MANUAL_CONTROL` rejected, RC path active
