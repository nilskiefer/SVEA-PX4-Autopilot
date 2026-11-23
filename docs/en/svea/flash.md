---
aside: false
---

<script setup>
import FirmwareFlasher from "./.vitepress/components/FirmwareFlasher.vue"
</script>

<div class="clicker4-flash-page">

# Flash the Clicker4

Connect a USB data cable to **CN2** (CODEGRIP) and open this page in desktop Chrome or Edge. Close OpenOCD or the CODEGRIP utility before connecting.

<FirmwareFlasher />

::: details Linux USB permissions

If CODEGRIP appears in the chooser but the browser cannot open it, the local desktop user may need permission to open the probe. On Linux desktops that use systemd, save the following rule as `/etc/udev/rules.d/70-codegrip.rules`.

```text
KERNEL=="hidraw*", ATTRS{idVendor}=="2dbc", ATTRS{idProduct}=="1924", TAG+="uaccess"
```

Run `sudo udevadm control --reload-rules`, then unplug and reconnect the USB cable. See the [HIDAPI udev examples](https://github.com/libusb/hidapi/blob/master/udev/69-hid.rules) and [Chrome's WebHID guide](https://developer.chrome.com/docs/capabilities/hid).

:::

</div>

<style scoped>
.clicker4-flash-page {
  max-width: 56rem;
}
</style>
