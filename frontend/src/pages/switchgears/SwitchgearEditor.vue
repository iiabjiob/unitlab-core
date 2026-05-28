<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useViewport } from "@/composables/useViewport"

import SwitchgearEditorHeader from "./components/SwitchgearEditorHeader.vue"
import SwitchgearExecutionLog from "./components/SwitchgearExecutionLog.vue"
import SwitchgearControlToolbar from "./components/SwitchgearControlToolbar.vue"
import SwitchgearBindingsEditor from "./components/SwitchgearBindingsEditor.vue"
import SwitchgearBindingsSummary from "./components/SwitchgearBindingsSummary.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSwitchgearStore()
const selectionStore = useSelectionStore()
const { isDesktop } = useViewport()

const switchgearId = computed(() => Number(route.params.id))
const switchgear = computed(() => (
  store.switchgears.find(item => item.id === switchgearId.value) ?? null
))

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

      <div class="switchgear-editor__workspace">
        <div class="switchgear-editor__summary-column">
          <ResizablePanel
            v-if="isDesktop"
            class="switchgear-editor__summary-panel"
            :switchgear="switchgear"
            placement="left"
            storageKey="switchgear-summary-width"
            :minSize="320"
            :defaultSize="360"
            :maxSize="800"
          >
            <SwitchgearBindingsSummary :switchgear="switchgear" @edit="bindingsEditorOpen = true" />
          </ResizablePanel>

          <div
            v-else
            class="switchgear-editor__summary-card"
          >
            <div>
              <SwitchgearBindingsSummary :switchgear="switchgear" @edit="bindingsEditorOpen = true" />
            </div>
          </div>
        </div>

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
      </div>
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

.switchgear-editor__workspace {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  margin-top: 1.25rem;
  padding: 1rem;
  border-radius: var(--radius-md);
  background: var(--color-white);
  box-shadow: var(--shadow-sm);
}

.switchgear-editor__summary-column,
.switchgear-editor__main-column,
.switchgear-editor__summary-panel {
  display: flex;
  flex-direction: column;
}

.switchgear-editor__summary-panel,
.switchgear-editor__summary-card {
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.5rem;
  background: var(--color-neutral-50);
  overflow: hidden;
}

.switchgear-editor__summary-card {
  padding: 0;
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

@media (min-width: 640px) {
  .switchgear-editor__workspace {
    padding: 1.25rem;
  }
}

@media (min-width: 1024px) {
  .switchgear-editor {
    min-height: 0;
    overflow: hidden;
  }

  .switchgear-editor__workspace {
    min-height: 0;
    flex: 1 1 0;
    flex-direction: row;
    gap: 1.25rem;
    overflow: hidden;
  }

  .switchgear-editor__summary-column {
    align-self: stretch;
    height: 100%;
    min-height: 0;
    flex: 0 0 auto;
  }

  .switchgear-editor__summary-panel {
    flex: 1 1 auto;
    min-height: 0;
    height: 100%;
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

:global(.dark .switchgear-editor__workspace) {
  background: var(--color-neutral-800);
}

:global(.dark .switchgear-editor__summary-panel),
:global(.dark .switchgear-editor__summary-card) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
}
</style>
