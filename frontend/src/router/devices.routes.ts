import type { RouteRecordRaw } from "vue-router"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

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
      await runStoreBootstrap(
        ["route-devices"],
        [() => store.ensureLoaded()],
        { mode: "strict" },
      )
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
          const lastDevice = lastId
            ? deviceStore.devices.find(device => device.id === lastId)
            : null

          if (lastDevice?.online) {
            return { name: "devices.detail", params: { id: lastDevice.id } }
          }

          const fallback = deviceStore.devices.find(device => device.online)
          if (fallback) {
            return { name: "devices.detail", params: { id: fallback.id } }
          }

          return true
        },
      },
      {
        path: ":id",
        name: "devices.detail",
        component: () => import("@/pages/devices/DeviceEditor.vue"),
        props: true,
        beforeEnter: (to) => {
          const deviceStore = useDeviceStore()
          const selectionStore = useSelectionStore()
          selectionStore.restore()

          const requestedId = Number(to.params.id)
          const requested = deviceStore.devices.find(device => device.id === requestedId)
          if (requested?.online) {
            return true
          }

          const lastId = selectionStore.lastDeviceId
          const lastDevice = lastId
            ? deviceStore.devices.find(device => device.id === lastId && device.online)
            : null

          if (lastDevice) {
            return { name: "devices.detail", params: { id: lastDevice.id } }
          }

          const fallback = deviceStore.devices.find(device => device.online)
          if (fallback) {
            return { name: "devices.detail", params: { id: fallback.id } }
          }

          return { name: "devices.list" }
        },
      },
    ],
  },
]
