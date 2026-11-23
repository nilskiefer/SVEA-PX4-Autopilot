import { defineConfig } from "vitepress";

const flash = { text: "Flash firmware", link: "/flash" };
const setup = { text: "Set up the board", link: "/quick-bringup" };
const troubleshooting = { text: "Troubleshooting", link: "/troubleshooting" };
const firmware = {
  text: "Firmware development",
  collapsed: true,
  items: [
    { text: "Build environment", link: "/dev-environment" },
    { text: "Build and flash", link: "/build-and-flash" },
    { text: "Firmware inventory", link: "/firmware-inventory" }
  ]
};
const vehicle = {
  text: "Vehicle operation",
  collapsed: true,
  items: [
    { text: "Drive and arm", link: "/controlling-the-car" },
    { text: "ROS manual control", link: "/ros-manual-control" },
    { text: "Tune parameters", link: "/common-adjustments" },
    { text: "Wheel encoder bridge", link: "/peripheral-mcu-bridge" }
  ]
};
const telemetry = {
  text: "Telemetry",
  collapsed: true,
  items: [
    { text: "ROS topics and power", link: "/ros-topic-reference" },
    { text: "Sensor publish rates", link: "/current-sensor-publish-rates" },
    { text: "Battery state of charge", link: "/battery-state-of-charge" },
    { text: "MAVLink uORB tunnel", link: "/mavlink-uorb-tunnel" }
  ]
};

export default defineConfig({
  title: "SVEA PX4 Docs",
  description: "Clicker4 setup, SVEA firmware, vehicle operation, and PMB3 hardware",
  base: process.env.BRANCH_NAME ? `/${process.env.BRANCH_NAME}/` : "/",
  cleanUrls: true,
  ignoreDeadLinks: false,
  themeConfig: {
    nav: [
      flash,
      {
        text: "Guides",
        items: [
          { text: "Get started", items: [setup, troubleshooting] },
          firmware,
          vehicle
        ]
      },
      {
        text: "Reference",
        items: [telemetry, { text: "PMB3 hardware", link: "/pmb3/" }],
        activeMatch: "^/pmb3/"
      }
    ],
    sidebar: [
      {
        text: "Get started",
        collapsed: false,
        items: [
          { text: "Overview", link: "/" },
          setup,
          flash,
          troubleshooting
        ]
      },
      firmware,
      vehicle,
      telemetry,
      {
        text: "PMB3 hardware",
        collapsed: true,
        items: [
          { text: "PMB3 overview", link: "/pmb3/" },
          {
            text: "Power system",
            collapsed: true,
            items: [
              { text: "Battery and rail monitors", link: "/pmb3/powerboard" },
              { text: "Power gate", link: "/pmb3/power-gate" },
              { text: "MPPT setpoint", link: "/pmb3/mppt" }
            ]
          },
          {
            text: "I/O and actuators",
            collapsed: true,
            items: [
              { text: "Expanders and GPIO", link: "/pmb3/expanders" },
              { text: "Actuator outputs", link: "/pmb3/actuators" },
              { text: "LEDs", link: "/pmb3/leds" }
            ]
          }
        ]
      }
    ],
    socialLinks: [
      { icon: "github", link: "https://github.com/nilskiefer/SVEA-PX4-Autopilot" }
    ],
    search: { provider: "local" }
  }
});
