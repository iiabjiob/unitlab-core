import type { RouteRecordRaw } from "vue-router"

export const signalsRoutes: RouteRecordRaw[] = [
  {
    path: "/signals",
    component: () => import("@/pages/signals/SignalsPage.vue"),
    meta: {
      leftAside: true,
      layout: "app",
    },
    children: [
      {
        path: "",
        name: "signals.home",
        component: () => import("@/pages/signals/SignalPlaceholder.vue"),
      },
      {
        path: "test-runs",
        name: "signals.testRuns",
        component: () => import("@/pages/signals/components/TestRunsTable.vue"),
      },
      {
        path: ":snapshotId(\\d+)",
        name: "signals.detail",
        component: () => import("@/pages/signals/components/AllocationEditor.vue"),
        props: route => ({ snapshotId: Number(route.params.snapshotId) || null }),
      },
    ],
  },
]
