import type { RouteRecordRaw } from "vue-router"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSelectionStore } from "@/stores/selectionStore"

// Default meta shared from index.ts
const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const devicesRoutes: RouteRecordRaw[] = [
  {
    path: "/devices",
    component: () => import("@/pages/devices/DevicesPage.vue"),
    beforeEnter: async () => {
      const store = useDeviceStore()
      await store.ensureLoaded()
    },
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
        beforeEnter: () => {
          const deviceStore = useDeviceStore()
          const selectionStore = useSelectionStore()
          selectionStore.restore()
          const lastId = selectionStore.lastDeviceId
          if (!lastId) return true

          const exists = deviceStore.devices.some(device => device.id === lastId)
          if (!exists) return true

          return { name: "devices.detail", params: { id: lastId } }
        },
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
