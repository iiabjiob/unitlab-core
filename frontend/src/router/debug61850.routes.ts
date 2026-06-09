import type { RouteRecordRaw } from "vue-router"

export const debug61850Routes: RouteRecordRaw[] = [
  {
    path: "/61850-debug/native",
    name: "iec61850.debug.native",
    component: () => import("@/pages/debug61850/Iec61850NativeCompilerPage.vue"),
    meta: {
      leftAside: true,
      layout: "auto",
    },
  },
  {
    path: "/61850-debug/templates",
    name: "iec61850.debug.templates",
    component: () => import("@/pages/debug61850/Iec61850TemplatePreviewPage.vue"),
    meta: {
      leftAside: true,
      layout: "auto",
    },
  },
  {
    path: "/61850-debug/client",
    name: "iec61850.debug.client",
    component: () => import("@/pages/debug61850/Iec61850ClientTestPage.vue"),
    meta: {
      leftAside: true,
      layout: "auto",
    },
  },
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
