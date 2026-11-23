<script setup>
import FirmwareFlasher from "./.vitepress/components/FirmwareFlasher.vue"
</script>

# Flash the Clicker4

Use desktop Chrome or Edge on macOS, Windows, or Linux. Plug a USB data cable into **CN2** (CODEGRIP) on the Clicker4 and close OpenOCD or the CODEGRIP utility before connecting.

Select **Connect CODEGRIP** to check the board, then choose the latest release or prerelease. The flasher verifies the selected firmware and checks saved parameters before rebooting.

**Back up parameters** downloads a copy of the saved parameter sector through CN2. The factory reset option erases that sector after the new firmware passes verification. See [how to restore a backup](build-and-flash.md#restore-a-parameter-backup).

<FirmwareFlasher />

## Linux USB permissions

If CODEGRIP appears in the chooser but the browser cannot open it, the local desktop user may need permission to open the probe. On Linux desktops that use systemd, save the following rule as `/etc/udev/rules.d/70-codegrip.rules`.

```text
KERNEL=="hidraw*", ATTRS{idVendor}=="2dbc", ATTRS{idProduct}=="1924", TAG+="uaccess"
```

Run `sudo udevadm control --reload-rules`, then unplug and reconnect the USB cable. See the [HIDAPI udev examples](https://github.com/libusb/hidapi/blob/master/udev/69-hid.rules) and [Chrome’s WebHID guide](https://developer.chrome.com/docs/capabilities/hid).
