import type { RouteRecordRaw } from "vue-router"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

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
      const workspaceStore = useWorkspaceStore()
      await workspaceStore.bootstrap()
      const store = useSwitchgearStore()
      await runStoreBootstrap(
        ["route-switchgears", workspaceStore.activeWorkspaceId],
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
        path: "sld/:id?",
        name: "switchgears.sld",
        component: () => import("@/pages/switchgears/SwitchgearSldPage.vue"),
        beforeEnter: (to) => {
          const store = useSwitchgearStore()
          const requestedId = Number(to.params.id)
          if (Number.isFinite(requestedId) && store.switchgears.some(sw => sw.id === requestedId)) {
            return true
          }
          if (to.params.id !== undefined) {
            return { name: "switchgears.sld" }
          }
          const selectionStore = useSelectionStore()
          selectionStore.restore()
          const fallbackId = selectionStore.lastSwitchgearId ?? store.switchgears[0]?.id
          return fallbackId == null
            ? true
            : { name: "switchgears.sld", params: { id: fallbackId } }
        },
      },
      {
        path: ":id",
        name: "switchgears.detail",
        component: () => import("@/pages/switchgears/SwitchgearEditor.vue"),
        props: true,
        beforeEnter: async () => {
          const workspaceStore = useWorkspaceStore()
          await workspaceStore.bootstrap()
          if (!workspaceStore.activeWorkspaceId) {
            return { name: "home" }
          }
        },
      },
    ],
  },
]
