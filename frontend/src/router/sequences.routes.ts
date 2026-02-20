import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
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
    beforeEnter: async () => {
      const workspaceStore = useWorkspaceStore()
      await workspaceStore.bootstrap()
      const store = useSequenceStore()
      await runStoreBootstrap(
        ["route-sequences", workspaceStore.activeWorkspaceId],
        [() => store.ensureLoaded()],
        { mode: "strict" },
      )
    },
    meta: {
      ...defaultMeta,
    },
    children: [
      {
        path: "",
        name: "instructions.list",
        component: () => import("@/pages/sequences/SequencePlaceholder.vue"),
        beforeEnter: () => {
          const selectionStore = useSelectionStore()
          selectionStore.restore()
          const lastId = selectionStore.lastSequenceId
          if (!lastId) return true

          const seqStore = useSequenceStore()
          const exists = seqStore.sequences.some(sequence => sequence.id === lastId)
          if (!exists) return true

          return { name: "instructions.detail", params: { id: lastId } }
        },
      },
      {
        path: ":id",
        name: "instructions.detail",
        component: () => import("@/pages/sequences/SequenceEditor.vue"),
        props: true,
        beforeEnter: async (to) => {
          const workspaceStore = useWorkspaceStore()
          await workspaceStore.bootstrap()
          if (!workspaceStore.activeWorkspaceId) {
            return { name: "home" }
          }
          const seqId = Number(to.params.id)
          const seqStore = useSequenceStore()
          const stepStore = useSequenceStepStore()

          await runStoreBootstrap(
            ["route-sequences-detail", workspaceStore.activeWorkspaceId, seqId],
            [
              () => seqStore.ensureLoaded(),
              () => stepStore.ensureSteps(seqId),
              () => seqStore.refreshState(seqId),
            ],
            { mode: "strict" },
          )
        }
      },
      
    ],
  },
]
