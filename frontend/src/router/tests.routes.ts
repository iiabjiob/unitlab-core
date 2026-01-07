import type { RouteRecordRaw } from "vue-router"
import { useProjectStore } from "@/stores/projectStore"
import { useTestRunStore } from "@/stores/testRunStore"
import { useTestRunStepStore } from "@/stores/testRunStepStore"

const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const testsRoutes: RouteRecordRaw[] = [
  {
    path: "/tests",
    component: () => import("@/pages/tests/TestsPage.vue"),
    beforeEnter: async () => {
      const projectStore = useProjectStore()
      await projectStore.bootstrap()
      if (!projectStore.activeProjectId) {
        return { name: "home" }
      }
      const runStore = useTestRunStore()
      await runStore.ensureLoaded()
    },
    meta: {
      ...defaultMeta,
    },
    children: [
      {
        path: "",
        name: "tests.list",
        component: () => import("@/pages/tests/TestRunPlaceholder.vue"),
      },
      {
        path: ":id",
        name: "tests.detail",
        props: true,
        component: () => import("@/pages/tests/TestRunEditor.vue"),
        beforeEnter: async (to) => {
          const projectStore = useProjectStore()
          await projectStore.bootstrap()
          if (!projectStore.activeProjectId) {
            return { name: "home" }
          }
          const runId = Number(to.params.id)
          if (Number.isNaN(runId)) {
            return { name: "tests.list" }
          }
          const runStore = useTestRunStore()
          const stepStore = useTestRunStepStore()
          await runStore.ensureLoaded()
          await stepStore.ensureSteps(runId)
          await runStore.refreshState(runId)
        },
      },
    ],
  },
]
