import type { RouteRecordRaw } from "vue-router"
import { useProjectStore } from "@/stores/projectStore"
import { useTestRunStore } from "@/stores/testRunStore"
import { useTestRunStepStore } from "@/stores/testRunStepStore"
import { useSelectionStore } from "@/stores/selectionStore"

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
        beforeEnter: () => {
          const selectionStore = useSelectionStore()
          selectionStore.restore()
          const lastId = selectionStore.lastTestRunId
          if (!lastId) return true

          const runStore = useTestRunStore()
          const exists = runStore.runs.some(run => run.id === lastId)
          if (!exists) return true

          return { name: "tests.detail", params: { id: lastId } }
        },
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
