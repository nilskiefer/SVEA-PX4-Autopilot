# Current Sensor Publish Rates

A PX4 command changes the rate at which telemetry is **forwarded** to ROS. In this firmware, raising the actual INA226/INA3221 acquisition rate above about 4 Hz also requires a source change, rebuild and flash. There is no runtime sample-rate command or parameter for these SVEA drivers.

Use the [ROS topic lookup](ros-topic-reference.md) to select the physical branch first.

## The Three Rate Controls

A measurement travels from sensor conversion, to driver/uORB publication, to MAVLink forwarding and the ROS bridge. Increasing one stage cannot make a slower stage produce more fresh measurements.

| Stage / source | Current configuration | How to change it |
| --- | --- | --- |
| INA226 driver, ESC and servo | `INA226_SAMPLE_FREQUENCY_HZ=4`, nominally 250 ms between publications | Edit `svea_ina226.h`, rebuild/flash; check conversion timing too. |
| INA3221 driver, six connector branches | `INA3221_SAMPLE_FREQUENCY_HZ=4`, nominally 250 ms plus read/work-queue time | Edit `svea_ina3221.h`, rebuild/flash; check conversion and averaging settings. All three channels on each chip share the polling cadence. |
| BQ769x2 battery driver | `SAMPLE_INTERVAL_US=100_ms`, nominally 10 Hz | Source change in `bq769x2.h` for faster driver polling; separate from chip CC2 measurement timing. |
| Per-instance uORB tunnel forwarding | Eight `power_monitor` entries, each capped at 4 Hz | `mavlink uorb_tunnel add -t ... -i ... -r ...` in NSH. |
| MAVLink tunnel stream scheduler | Depends on the active MAVLink link configuration | `mavlink stream -d ... -s PX4_UORB_TUNNEL -r ...`. Must be enabled on the link used by MAVROS. |
| Standard MAVLink battery stream | `BATTERY_STATUS` defaults to 0.5 Hz in the configured default stream sets | `mavlink stream -d ... -s BATTERY_STATUS -r ...`. |
| uXRCE-DDS battery output | `/fmu/out/battery_status` has `rate_limit: 1.` in `dds_topics.yaml` | Edit the YAML and rebuild if using DDS; MAVLink commands do not alter it. |

The tunnel sends only when a uORB update exists and its per-entry rate gate permits it. It forwards the latest available sample; it does not queue every historical sensor sample or synthesize measurements. Achievable ROS rate is bounded by acquisition, entry cap, stream scheduling and available link bandwidth. Scheduling alignment can reduce the achieved rate further.

## Change Forwarding from the PX4 Console

Open a PX4 NSH console, such as QGroundControl's MAVLink Console. These are firmware commands, not ROS shell commands.

First inspect the active configuration:

```sh
mavlink status
mavlink uorb_tunnel list
svea_ina226 status
svea_ina3221 status
uorb top
```

Identify the **PX4-side** MAVLink device connected to the companion. The commands below assume `mavlink status` confirms `/dev/ttyACM0`; replace it with the device actually shown. A Linux companion path such as `/dev/serial/by-id/...` is not a PX4-side device name. For a UDP MAVLink link use `-u <local-port>` instead of `-d <device>`.

Example: request a 20 Hz forwarding cap for the ESC and servo instances and poll the tunnel stream at 50 Hz:

```sh
mavlink uorb_tunnel add -t power_monitor -i 0 -r 20
mavlink uorb_tunnel add -t power_monitor -i 1 -r 20
mavlink stream -d /dev/ttyACM0 -s PX4_UORB_TUNNEL -r 50
mavlink uorb_tunnel list
mavlink status
```

Adding an existing topic/instance updates its rate; removing it first is unnecessary. The per-entry accepted range is `0 < rate <= 100 Hz`. Without `-f`, the command rejects an unadvertised instance. Keep that check for interactive use; `-f` is intended for boot scripts that register topics before publishers appear. Registry changes are picked up by the stream on its roughly one-second refresh cycle.

**With the stock INA drivers, this example still delivers at most about 4 Hz per rail.** It is useful after a faster acquisition build, or to restore a forwarding rate that was set below the acquisition rate. Other instances retain their existing caps; configure only the branches your experiment requires. A shared stream scheduler must run fast enough for the fastest selected entry, with sufficient bandwidth for all registered topics.

To restore the ESC and servo forwarding caps:

```sh
mavlink uorb_tunnel add -t power_monitor -i 0 -r 4
mavlink uorb_tunnel add -t power_monitor -i 1 -r 4
```

Choose the stream scheduler rate according to every topic on that link; lowering it to 4 Hz would also limit any faster battery or wheel topics added to the tunnel.

## Faster Battery Telemetry Without an INA Rebuild

The BQ driver already polls nominally at 10 Hz. For the normal MAVROS battery topic, raise the standard battery stream on the companion link:

```sh
mavlink stream -d /dev/ttyACM0 -s BATTERY_STATUS -r 10
```

This changes `/mavros/battery` delivery when its plugin is enabled. It does not change the eight rail monitors. Alternatively, to forward raw PX4 battery fields through the SVEA tunnel:

```sh
listener battery_status -n 1 -i 0
mavlink uorb_tunnel add -t battery_status -i 0 -r 10
mavlink stream -d /dev/ttyACM0 -s PX4_UORB_TUNNEL -r 50
```

Check that instance 0 is the BQ report. The compatible companion message package must include `BatteryStatus`; the bridge then exposes `/px4/uorb/battery_status/instance_0`. Standard MAVROS and raw PX4 battery current have different signs; see [Battery Power and Sign](ros-topic-reference.md#battery-power-and-sign).

## Acquire Faster INA Measurements

For a project requiring more than 4 Hz, change and validate acquisition before relying on the forwarding command:

1. In `src/drivers/power_monitor/svea_ina226/svea_ina226.h`, change `INA226_SAMPLE_FREQUENCY_HZ` from `4` to the required polling rate for ESC/servo monitoring. `INA226_SAMPLE_INTERVAL_US` and `INA226_CONVERSION_INTERVAL` derive from it.
2. In `src/drivers/power_monitor/svea_ina3221/svea_ina3221.h`, change `INA3221_SAMPLE_FREQUENCY_HZ` from `4` for the connector monitors. This affects both chips and all six channels.
3. Ensure the chip conversion/averaging configuration meets the experiment's response-time requirement. A faster read loop alone does not remove filtering or guarantee a fresh conversion per read.
4. [Build and flash](build-and-flash.md), then verify the local uORB cadence with `uorb top` and sensor status before raising the tunnel caps and scheduler.
5. Validate the received cadence, sensor response and I2C/link errors under the expected load. These docs do not establish a tested maximum acquisition rate for the board.

### INA226 Timing

The SVEA header uses continuous bus+shunt mode, 64 averages, and 588 µs for each conversion (`0x06DF`). The board disables the standard INA226 module, whose source defines `INA226_CONFIG`, and the SVEA module does not define its own copy of that parameter. For this board, tune the compiled `INA226_CONFIG` macro unless you have verified that your build exposes the parameter. The SVEA constructor checks for the parameter if present; in a build that includes it, its value overrides the header macro. Inspect:

```sh
param show INA226_CONFIG
svea_ina226 status
```

If `param show` reports the parameter is absent, use the compiled settings; that absence is expected for this board configuration. If present, the standard parameter definition defaults to `18139` (`0x46DB`), using **triggered** bus+shunt mode with the same averaging/conversion times.

With these conversion times and 64 averages, a complete averaged bus+shunt measurement nominally takes `(588 + 588) µs × 64 = 75.264 ms`. A 20 Hz driver interval is only 50 ms. In continuous mode, polling every 50 ms can reread the same averaged result. In triggered mode, shortening the wait to 50 ms would read before the nominal conversion completes. Plan both polling and sensor timing together. The [TI INA226 datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf) defines the conversion times, averaging and mode bits.

When available, the `INA226_CONFIG` parameter controls chip configuration, not the SVEA polling cadence. It is applied at driver construction, so changing it requires a restart/reboot; `param save` persists that parameter. With only the header macro, changing configuration requires a rebuild/flash. Do not alter shunt calibration options to change rate: driver `-r` means shunt resistance in ohms, whereas `mavlink uorb_tunnel ... -r` means hertz.

### INA3221 Timing

The compiled default is three channels enabled, continuous shunt+bus measurements, 1.1 ms for each conversion, and averaging setting 64. An unaveraged scan of all six signals nominally takes `3 × (1.1 + 1.1) ms = 6.6 ms`; averaging filters the response to changes. Do not interpret the polling rate as unfiltered measurement bandwidth. Consult the [TI INA3221 datasheet](https://www.ti.com/lit/ds/symlink/ina3221.pdf), particularly averaging and multichannel timing, when selecting settings.

These settings are compiled in `INA3221_CONFIG_DEFAULT`; this SVEA driver exposes no runtime configuration/rate parameter. Changing them requires rebuilding. The bus and shunt registers are read separately, so voltage/current pairs and different channels are not synchronized samples. Verify settling and response against a known load transition for transient research.

## Persist the Configuration

Interactive `mavlink uorb_tunnel` and `mavlink stream` changes are runtime state and are not saved by `param save`.

For a firmware-managed setup, edit the eight registrations in `boards/mikroe/clicker4-stm32f7/init/rc.board_extras` for the selected instances. Keep `-f` on the boot-time registrations. Ensure the stream command runs in the startup configuration **after the companion's MAVLink instance is running**, using its actual device/port. Rebuild and flash to persist those script changes. If the companion configures stream rates during connection, update that setup too so reconnection does not restore a different scheduler rate.

## Verify on the Companion

Start the matching SVEA uORB bridge if it is not already launched by the car setup:

```sh
ros2 run svea_core px4_uorb_tunnel
ros2 topic hz /px4/uorb/power_monitor/instance_0
ros2 topic echo /px4/uorb/power_monitor/instance_0
```

Run rate/echo commands from another terminal when launching the node in the foreground. Check `timestamp` increments (nominally 250,000 µs for stock INA polling) and compare observed rate with the local uORB rate. Those timestamps describe readout/publication, not proof that the chip performed a new conversion. An unchanged current can also be a genuinely steady load.

For batteries use `ros2 topic hz /mavros/battery` or the dedicated raw battery topic, depending on the transport selected. With sensor-data QoS, use `--qos-reliability best_effort` for `ros2 topic echo` if necessary to match the publisher.

If results miss the requested rate, inspect driver polling/conversion timing, the instance cap, the active stream on the correct link, and MAVLink bandwidth/errors in that order. Stop the experiment if instances are missing, readings carry the invalid sentinel combination, or rail identification is unverified. The aggregate `/px4/uorb/power_monitor` rate cannot verify an individual rail's sampling frequency.

## Implementation References

Checked against firmware `261e1fb178` on 2026-10-01:

- [INA226 cadence and configuration](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/src/drivers/power_monitor/svea_ina226/svea_ina226.h), [INA226 parameter definition](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/src/drivers/power_monitor/ina226/ina226_params.c).
- [INA3221 cadence and configuration](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/src/drivers/power_monitor/svea_ina3221/svea_ina3221.h), [BQ battery cadence](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/src/drivers/smart_battery/bq769x2/bq769x2.h).
- [Tunnel CLI](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/src/modules/mavlink/mavlink_main.cpp), [tunnel send/rate gate](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/src/modules/mavlink/streams/PX4_UORB_TUNNEL.hpp), [boot registrations](https://github.com/nilskiefer/SVEA-PX4-Autopilot/blob/261e1fb178/boards/mikroe/clicker4-stm32f7/init/rc.board_extras).

See [MAVLink uORB Tunnel](mavlink-uorb-tunnel.md) for the protocol.
