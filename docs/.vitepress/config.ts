import { defineConfig } from "vitepress"

export default defineConfig({
  lang: "en-US",
  title: "Affino Operator Guide",
  description: "Simple practical guide for daily system operations",
  lastUpdated: true,
  themeConfig: {
    nav: [
      { text: "Guide", link: "/guide/quick-start" },
      { text: "FAQ", link: "/guide/faq" },
    ],
    sidebar: [
      {
        text: "Quick Start",
        items: [
          { text: "Getting Started", link: "/guide/quick-start" },
          { text: "Field Workflow (Engineer)", link: "/guide/field-workflow" },
          { text: "Interface", link: "/guide/interface" },
        ],
      },
      {
        text: "Working in the System",
        items: [
          { text: "Signals", link: "/guide/signals" },
          { text: "Switchgear", link: "/guide/switchgears" },
          { text: "Instructions (Sequences)", link: "/guide/sequences" },
        ],
      },
      {
        text: "Support",
        items: [
          { text: "Troubleshooting", link: "/guide/troubleshooting" },
          { text: "FAQ", link: "/guide/faq" },
        ],
      },
      {
        text: "Engineering Notes",
        items: [
          { text: "Signal List Migration Plan", link: "/architecture/signal-list-allocation-migration-plan" },
          { text: "IEC 61850 MMS Client Plan", link: "/architecture/iec61850-self-owned-mms-client-plan" },
        ],
      },
    ],
    search: {
      provider: "local",
    },
  },
})
