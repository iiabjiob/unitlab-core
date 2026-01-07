import type { RouteRecordRaw } from "vue-router"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useProjectStore } from "@/stores/projectStore"
import { useSelectionStore } from "@/stores/selectionStore"

// Default meta shared from index.ts
const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const switchgearsRoutes: RouteRecordRaw[] = [
  {
    path: "/switchgears",
    component: () => import("@/pages/switchgears/SwitchgearsPage.vue"),
    beforeEnter: async () => {
      const projectStore = useProjectStore()
      await projectStore.bootstrap()
      const store = useSwitchgearStore()
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
        name: "switchgears.list",
        component: () => import("@/pages/switchgears/SwitchgearPlaceholder.vue"),
        beforeEnter: () => {
          const selectionStore = useSelectionStore()
          selectionStore.restore()
          const lastId = selectionStore.lastSwitchgearId
          if (!lastId) return true

          const store = useSwitchgearStore()
          const exists = store.switchgears.some(sw => sw.id === lastId)
          if (!exists) return true

          return { name: "switchgears.detail", params: { id: lastId } }
        },
      },
      {
        path: ":id",
        name: "switchgears.detail",
        component: () => import("@/pages/switchgears/SwitchgearEditor.vue"),
        props: true,
        beforeEnter: async () => {
          const projectStore = useProjectStore()
          await projectStore.bootstrap()
          if (!projectStore.activeProjectId) {
            return { name: "home" }
          }
        },
      },
    ],
  },
]
