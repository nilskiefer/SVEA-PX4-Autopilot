import { defineConfig } from "vitepress";

export default defineConfig({
  title: "SVEA PX4 Docs",
  description: "SVEA bringup, flashing, and troubleshooting documentation",
  base: process.env.BRANCH_NAME ? `/${process.env.BRANCH_NAME}/` : "/",
  cleanUrls: true,
  ignoreDeadLinks: false,
  mermaid: {},
  themeConfig: {
    nav: [
      { text: "SVEA Docs", link: "/" },
      { text: "Repository", link: "https://github.com/nilskiefer/SVEA-PX4-Autopilot" }
    ],
    sidebar: [
      {
        text: "SVEA",
        items: [
          { text: "Overview", link: "/" },
          { text: "Dev Environment", link: "/dev-environment" },
          { text: "Quick Bringup", link: "/quick-bringup" },
          { text: "Build and Flash", link: "/build-and-flash" },
          { text: "Troubleshooting", link: "/troubleshooting" },
          { text: "Common Adjustments", link: "/common-adjustments" },
          { text: "Firmware Inventory", link: "/firmware-inventory" },
          { text: "Controlling the Car", link: "/controlling-the-car" },
          { text: "Peripheral MCU Bridge", link: "/peripheral-mcu-bridge" },
          { text: "MAVLink uORB Tunnel", link: "/mavlink-uorb-tunnel" }
        ]
      },
      {
        text: "PMB3",
        items: [
          { text: "Overview", link: "/pmb3/" },
          { text: "Powerboard Baseline", link: "/pmb3/powerboard" },
          { text: "Expanders and GPIO", link: "/pmb3/expanders" },
          { text: "Power Gate", link: "/pmb3/power-gate" },
          { text: "Actuators", link: "/pmb3/actuators" },
          { text: "LEDs", link: "/pmb3/leds" }
        ]
      }
    ],
    socialLinks: [
      { icon: "github", link: "https://github.com/nilskiefer/SVEA-PX4-Autopilot" }
    ],
    search: { provider: "local" }
  }
});
