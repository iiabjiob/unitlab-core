import type { RouteRecordRaw } from "vue-router"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useProjectStore } from "@/stores/projectStore"

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
