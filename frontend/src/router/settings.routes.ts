import type { RouteRecordRaw } from "vue-router"
import { useSelectionStore } from "@/stores/selectionStore"

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
        redirect: () => {
          const selectionStore = useSelectionStore()
          selectionStore.restore()
          const routeName = selectionStore.lastSettingsRouteName
          const allowedRouteNames = new Set([
            "settings.diagnostics",
            "settings.ntp",
            "settings.network",
            "settings.updates",
            "settings.provisioning",
          ])
          if (routeName && allowedRouteNames.has(routeName)) {
            return { name: routeName }
          }
          return { name: "settings.diagnostics" }
        },
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
