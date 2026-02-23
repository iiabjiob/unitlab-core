import type { RouteRecordRaw } from "vue-router"

const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const settingsRoutes: RouteRecordRaw[] = [
  {
    path: "/settings",
    component: () => import("@/pages/settings/SettingsPage.vue"),
    meta: {
      ...defaultMeta,
      rightAside: false,
      bottomAside: false,
    },
    children: [
      {
        path: "",
        redirect: { name: "settings.network" },
      },
      {
        path: "network",
        name: "settings.network",
        component: () => import("@/pages/settings/SettingsCoreNetworkPage.vue"),
      },
      {
        path: "diagnostics",
        name: "settings.diagnostics",
        component: () => import("@/pages/settings/SettingsDiagnosticsPage.vue"),
      },
      {
        path: "ntp",
        name: "settings.ntp",
        component: () => import("@/pages/settings/SettingsNtpPage.vue"),
      },
      {
        path: "updates",
        name: "settings.updates",
        component: () => import("@/pages/settings/SettingsUpdatesPage.vue"),
      },
      {
        path: "provisioning",
        name: "settings.provisioning",
        component: () => import("@/pages/settings/SettingsProvisioningPage.vue"),
      },
    ],
  },
]
