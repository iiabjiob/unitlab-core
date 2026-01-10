import type { RouteRecordRaw } from "vue-router"

export const signalsRoutes: RouteRecordRaw[] = [
  {
    path: "/signals",
    name: "signals",
    component: () => import("@/pages/signals/SignalsPage.vue"),
    meta: {
      leftAside: true,
      layout: "app",
    },
  },
]
