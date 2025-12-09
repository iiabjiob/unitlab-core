import type { RouteRecordRaw } from "vue-router"

// Default meta shared from index.ts
const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const switchgearsRoutes: RouteRecordRaw[] = [
  {
    path: "/switchgears",
    component: () => import("@/pages/switchgears/SwitchgearsPage.vue"),
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
      },
      {
        path: ":id",
        name: "switchgears.detail",
        component: () => import("@/pages/switchgears/SwitchgearEditor.vue"),
        props: true,
      },
    ],
  },
]
