import type { RouteRecordRaw } from "vue-router"
import { useWorkspaceStore } from "@/stores/workspaceStore"

export const signalsRoutes: RouteRecordRaw[] = [
  {
    path: "/signals",
    name: "signals.home",
    component: () => import("@/pages/signals/SignalsPage.vue"),
    beforeEnter: async () => {
      const workspaceStore = useWorkspaceStore()
      await workspaceStore.bootstrap()
      return true
    },
    meta: {
      leftAside: true,
      layout: "auto",
    },
  },
]
