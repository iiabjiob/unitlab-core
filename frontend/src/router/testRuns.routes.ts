import type { RouteRecordRaw } from "vue-router"

export const testRunsRoutes: RouteRecordRaw[] = [
  {
    path: "/test-runs",
    component: () => import("@/pages/testRuns/TestRunsPage.vue"),
    meta: {
      leftAside: true,
      layout: "app",
    },
    children: [
      {
        path: "",
        name: "testRuns.home",
        component: () => import("@/pages/testRuns/TestRunPlaceholder.vue"),
      },
      {
        path: "new",
        name: "testRuns.new",
        component: () => import("@/pages/testRuns/TestRunBuilder.vue"),
      },
      {
        path: ":runId(\\d+)",
        name: "testRuns.detail",
        component: () => import("@/pages/testRuns/TestRunEditor.vue"),
        props: route => ({ runId: Number(route.params.runId) || null }),
      },
    ],
  },
]
