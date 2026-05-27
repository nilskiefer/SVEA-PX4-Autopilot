import { defineConfig } from "vitepress";

export default defineConfig({
  title: "SVEA PX4 Docs",
  description: "SVEA bringup, flashing, and troubleshooting documentation",
  base: process.env.BRANCH_NAME ? `/${process.env.BRANCH_NAME}/` : "/",
  cleanUrls: true,
  ignoreDeadLinks: false,
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
          { text: "Build and Flash", link: "/build-and-flash" },
          { text: "Troubleshooting", link: "/troubleshooting" },
          { text: "Powerboard and Expanders", link: "/powerboard-and-expanders" },
          { text: "Actuators and LEDs", link: "/actuators-and-leds" }
        ]
      }
    ],
    socialLinks: [
      { icon: "github", link: "https://github.com/nilskiefer/SVEA-PX4-Autopilot" }
    ],
    search: { provider: "local" }
  }
});
