<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSelectionStore } from "@/stores/selectionStore"

import SwitchgearEditorHeader from "./components/SwitchgearEditorHeader.vue"
import SwitchgearExecutionLog from "./components/SwitchgearExecutionLog.vue"
import SwitchgearControlToolbar from "./components/SwitchgearControlToolbar.vue"
import SwitchgearBindingsEditor from "./components/SwitchgearBindingsEditor.vue"
import SwitchgearBindingsSummary from "./components/SwitchgearBindingsSummary.vue"
import EditorWorkspaceLayout from "@/components/layout/EditorWorkspaceLayout.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSwitchgearStore()
const selectionStore = useSelectionStore()
const switchgearId = computed(() => Number(route.params.id))
const switchgear = computed(() => (
  store.switchgears.find(item => item.id === switchgearId.value) ?? null
))
const BINDINGS_EDITOR_QUERY_VALUE = "edit"

const deleteModalOpen = ref(false)
const bindingsEditorOpen = ref(false)

const deleteMessage = computed(() => (
  switchgear.value
    ? `Switchgear "${switchgear.value.name}" will be deleted.`
    : ""
))

watch(
  () => switchgear.value?.id ?? null,
  (id) => {
    selectionStore.selectSwitchgear(id)
    bindingsEditorOpen.value = false
  },
  { immediate: true },
)

watch(
  () => [route.params.id, route.query.bindings] as const,
  () => {
    syncBindingsEditorFromRoute()
  },
  { immediate: true },
)

function isBindingsEditorQueryRequested(raw: unknown): boolean {
  const values = Array.isArray(raw) ? raw : [raw]
  return values.some((value) => {
    const normalized = String(value ?? "").trim().toLowerCase()
    return normalized === BINDINGS_EDITOR_QUERY_VALUE
  })
}

function clearBindingsEditorQueryFlag() {
  if (!("bindings" in route.query)) {
    return
  }
  const nextQuery = { ...route.query }
  delete nextQuery.bindings
  void router.replace({ query: nextQuery }).catch(() => {
    return
  })
}

function syncBindingsEditorFromRoute() {
  if (!isBindingsEditorQueryRequested(route.query.bindings)) {
    return
  }
  bindingsEditorOpen.value = true
  clearBindingsEditorQueryFlag()
}

async function handleDuplicate() {
  if (!switchgear.value) return
  const duplicated = await store.duplicate(switchgear.value.id)
  await router.push({ name: "switchgears.detail", params: { id: duplicated.id } })
}

function requestDelete() {
  deleteModalOpen.value = true
}

function cancelDelete() {
  deleteModalOpen.value = false
}

async function confirmDelete() {
  if (!switchgear.value) return
  const deletingId = switchgear.value.id
  const before = [...store.switchgears]
  const currentIndex = before.findIndex(item => item.id === deletingId)

  bindingsEditorOpen.value = false
  await store.remove(deletingId)
  deleteModalOpen.value = false

  const after = store.switchgears
  if (after.length === 0) {
    await router.push({ name: "switchgears.list" })
    return
  }

  const fallbackIndex = currentIndex < 0
    ? 0
    : Math.min(currentIndex, after.length - 1)
  const fallback = after[fallbackIndex]

  await router.push({ name: "switchgears.detail", params: { id: fallback.id } })
}
</script>

<template>
  <div class="switchgear-editor">
    <template v-if="switchgear">
      <SwitchgearEditorHeader
        :switchgear="switchgear"
        @duplicate="handleDuplicate"
        @delete="requestDelete"
      />

      <SwitchgearControlToolbar :switchgear="switchgear" />

      <EditorWorkspaceLayout
        storage-key="switchgear-summary-width"
        :min-size="320"
        :default-size="360"
        :max-size="800"
      >
        <template #sidebar>
          <SwitchgearBindingsSummary :switchgear="switchgear" @edit="bindingsEditorOpen = true" />
        </template>

        <template #main>
          <div class="switchgear-editor__main-column">
            <SwitchgearBindingsEditor
              v-if="bindingsEditorOpen"
              class="switchgear-editor__bindings-editor"
              :switchgear="switchgear"
              @close="bindingsEditorOpen = false"
            />

            <div class="switchgear-editor__log">
              <SwitchgearExecutionLog :switchgear="switchgear" />
            </div>
          </div>
        </template>
      </EditorWorkspaceLayout>
    </template>

    <div v-else class="switchgear-editor__not-found">
      Switchgear not found.
    </div>

    <ConfirmModal
      v-if="switchgear"
      :open="deleteModalOpen"
      title="Delete switchgear"
      :message="deleteMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @confirm="confirmDelete"
      @cancel="cancelDelete"
    />
  </div>
</template>

<style scoped>
.switchgear-editor {
  display: flex;
  height: 100%;
  flex-direction: column;
  padding-inline-end: 1rem;
}

.switchgear-editor__main-column {
  display: flex;
  min-width: 0;
  min-height: 0;
  flex-direction: column;
}

.switchgear-editor__log {
  display: flex;
  min-height: 12rem;
  max-height: 40vh;
}

.switchgear-editor__bindings-editor {
  flex: 0 0 auto;
}

.switchgear-editor__not-found {
  display: flex;
  flex: 1 1 auto;
  align-items: center;
  justify-content: center;
  color: var(--color-neutral-500);
}

@media (min-width: 1500px) {
  .switchgear-editor {
    min-height: 0;
    overflow: hidden;
  }

  .switchgear-editor__main-column,
  .switchgear-editor__log {
    display: flex;
    flex-direction: column;
    min-height: 0;
    flex: 1 1 0;
    overflow: hidden;
  }

  .switchgear-editor__main-column {
    gap: 1rem;
  }
}

</style>
