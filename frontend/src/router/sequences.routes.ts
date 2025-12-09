import type { RouteRecordRaw } from "vue-router"

// Default meta shared from index.ts
const defaultMeta = {
  leftAside: true,
  layout: "auto" as const,
}

export const sequencesRoutes: RouteRecordRaw[] = [
  {
    path: "/sequences",
    component: () => import("@/pages/sequences/SequencesPage.vue"),
    meta: {
      ...defaultMeta,
      rightAside: false,
      bottomAside: false,
    },
    children: [
      {
        path: "",
        name: "sequences.list",
        component: () => import("@/pages/sequences/SequencePlaceholder.vue"),
      },
      {
        path: ":id",
        name: "sequences.detail",
        component: () => import("@/pages/sequences/SequenceEditor.vue"),
        props: true,
      },
    ],
  },
]
