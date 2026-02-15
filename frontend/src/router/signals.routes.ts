import type { RouteRecordRaw } from "vue-router"
import { useWorkspaceStore } from "@/stores/workspaceStore"

export const signalsRoutes: RouteRecordRaw[] = [
  {
    path: "/signals",
    component: () => import("@/pages/signals/SignalsPage.vue"),
    beforeEnter: async () => {
      const workspaceStore = useWorkspaceStore()
      const ready = await workspaceStore.bootstrap()
      if (!ready || !workspaceStore.activeWorkspaceId) {
        return { name: "home" }
      }
      return true
    },
    meta: {
      leftAside: true,
      layout: "app",
    },
    children: [
      {
        path: "",
        name: "signals.home",
        component: () => import("@/pages/signals/components/AllocationEditor.vue"),
      },
    ],
  },
]
