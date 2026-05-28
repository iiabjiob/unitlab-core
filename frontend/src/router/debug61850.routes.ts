import type { RouteRecordRaw } from "vue-router"

export const debug61850Routes: RouteRecordRaw[] = [
  {
    path: "/61850-debug",
    name: "iec61850.debug",
    component: () => import("@/pages/debug61850/Iec61850DebugPage.vue"),
    meta: {
      leftAside: true,
      layout: "auto",
    },
  },
]
