import type { RouteRecordRaw } from "vue-router"

// Default meta shared from index.ts
const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const devicesRoutes: RouteRecordRaw[] = [
  {
    path: "/devices",
    component: () => import("@/pages/devices/DevicesPage.vue"),
    meta: {
      ...defaultMeta,
      rightAside: false,
      bottomAside: false,
    },
    children: [
      {
        path: "",
        name: "devices.list",
        component: () => import("@/pages/devices/DevicePlaceholder.vue"),
      },
      {
        path: ":id",
        name: "devices.detail",
        component: () => import("@/pages/devices/DeviceEditor.vue"),
        props: true,
      },
    ],
  },
]
