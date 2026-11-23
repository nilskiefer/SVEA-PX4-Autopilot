---
aside: false
outline: false
prev: false
next: false
---

<script setup>
import { VPButton } from "vitepress/theme"
</script>

# SVEA PX4 documentation {#svea-px4-docs}

Install SVEA firmware on the Clicker4, then bring up the PX4 console on the board's runtime USB port.

<div class="start-actions">
  <VPButton text="Flash firmware" href="/flash" />
  <VPButton text="Set up the board" href="/quick-bringup" theme="alt" />
</div>

If CODEGRIP or the console won't connect, start with [Troubleshooting](troubleshooting.md).

## Guides and reference {#quick-start}

- [Firmware development](build-and-flash.md) covers your own builds, OpenOCD, and parameter recovery.
- [Vehicle operation](controlling-the-car.md) covers arming, RC modes, and handing control to ROS.
- [Telemetry](ros-topic-reference.md) maps ROS topics to the battery, rails, and current sensors.
- [PMB3 hardware](pmb3/index.md) covers the power gate, charger, GPIO, and actuator outputs.

<style scoped>
.start-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  margin: 24px 0;
}

.start-actions :deep(.VPButton) {
  text-decoration: none;
}
</style>
