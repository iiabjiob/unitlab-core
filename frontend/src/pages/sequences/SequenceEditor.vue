<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSequenceStore } from "@/stores/sequenceStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useToastStore } from "@/stores/toastStore"
import { useViewport } from "@/composables/useViewport"

import SequenceEditorHeader from "./components/SequenceEditorHeader.vue"
import SequenceStepsList from "./components/SequenceStepsList.vue"
import SequenceExecutionLog from "./components/SequenceExecutionLog.vue"
import SequenceStepEditor from "./components/SequenceStepEditor.vue"
import SequenceRunControls from "./components/SequenceRunControls.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSequenceStore()
const stepStore = useSequenceStepStore()
const selectionStore = useSelectionStore()
const toastStore = useToastStore()

const sequenceId = computed(() => Number(route.params.id))

const sequence = computed(() =>
  store.sequences.find(s => s.id === sequenceId.value)
)

watch(
  () => sequence.value?.id ?? null,
  (id) => {
    selectionStore.selectSequence(id)
  },
  { immediate: true }
)

const state = computed(() => store.states[sequenceId.value])
const selectedStep = computed(() => {
  if (!sequence.value) return null
  const activeId = stepStore.activeStepId
  if (!activeId) return null
  return stepStore.steps.find(
    step => step.sequence_id === sequence.value?.id && step.id === activeId,
  ) ?? null
})
const deleteModalOpen = ref(false)
const deleteMessage = computed(() =>
  sequence.value ? `Instruction "${sequence.value.name}" will be deleted with all steps.` : ""
)
const deleteConfirmLabel = "Delete"
const deleteCancelLabel = "Cancel"

async function handleDuplicate() {
  if (!sequence.value) return
  const duplicated = await store.duplicateSequence(sequence.value.id)
  await router.push({ name: "instructions.detail", params: { id: duplicated.id } })
}

async function handleExport() {
  if (!sequence.value) return
  try {
    const payload = await store.exportSequenceFile(sequence.value.id)
    const json = JSON.stringify(payload, null, 2)
    const blob = new Blob([json], { type: "application/json;charset=utf-8" })
    const url = URL.createObjectURL(blob)
    const link = document.createElement("a")
    link.href = url
    const safeName = sequence.value.name.replace(/[^a-zA-Z0-9_-]/g, "_")
    link.download = `${safeName || "sequence"}_${sequence.value.id}.json`
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    URL.revokeObjectURL(url)
    toastStore.success("Instruction exported")
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to export instruction")
  }
}

function requestDelete() {
  deleteModalOpen.value = true
}

function cancelDelete() {
  deleteModalOpen.value = false
}

async function confirmDelete() {
  if (!sequence.value) return
  const deletingId = sequence.value.id
  const before = [...store.sequences]
  const currentIndex = before.findIndex(item => item.id === deletingId)

  await store.deleteSequence(deletingId)
  deleteModalOpen.value = false

  const after = store.sequences
  if (after.length === 0) {
    await router.push({ name: "instructions.list" })
    return
  }

  const fallbackIndex = currentIndex < 0
    ? 0
    : Math.min(currentIndex, after.length - 1)
  const fallback = after[fallbackIndex]

  await router.push({ name: "instructions.detail", params: { id: fallback.id } })
}

function exitStepEdit() {
  stepStore.setActiveStep(null)
}

const { isDesktop } = useViewport()
</script>

<template>
  <div class="sequence-editor">
    <SequenceEditorHeader
      v-if="sequence"
      :sequence="sequence"
      @export="handleExport"
      @duplicate="handleDuplicate"
      @delete="requestDelete"
    />

    <SequenceRunControls
      v-if="sequence && state"
      :sequence="sequence"
      :state="state"
    />

    <div class="sequence-editor__workspace">
      <div
        v-if="sequence"
        class="sequence-editor__steps-column"
      >
        <ResizablePanel
          v-if="isDesktop"
          class="sequence-editor__steps-panel"
          :sequence="sequence"
          placement="left"
          storageKey="sequence-steps-list-width"
          :minSize="380"
          :defaultSize="380"
          :maxSize="800"
        >
          <SequenceStepsList :sequence="sequence" />
        </ResizablePanel>

        <div
          v-else
          class="sequence-editor__steps-card"
        >
          <div>
            <SequenceStepsList :sequence="sequence" />
          </div>
        </div>
      </div>

      <div v-if="sequence && state" class="sequence-editor__main-column">
        <SequenceStepEditor
          :sequence="sequence"
          :step="selectedStep"
          @close="exitStepEdit"
        />

        <div class="sequence-editor__log">
          <SequenceExecutionLog :sequence="sequence" :state="state" />
        </div>

      </div>

    </div>

    <ConfirmModal
      v-if="sequence"
      :open="deleteModalOpen"
      title="Delete instruction"
      :message="deleteMessage"
      :confirm-label="deleteConfirmLabel"
      :cancel-label="deleteCancelLabel"
      @confirm="confirmDelete"
      @cancel="cancelDelete"
    />
  </div>
</template>

<style scoped>
.sequence-editor {
  display: flex;
  height: 100%;
  flex-direction: column;
  padding-inline-end: 1rem;
}

.sequence-editor__workspace {
  display: flex;
  min-height: 0;
  flex-direction: column;
  flex: 1 1 0;
  gap: 1rem;
  margin-top: 1.25rem;
  overflow: hidden;
  padding: 1rem;
  border-radius: var(--radius-md);
  background: var(--color-white);
  box-shadow: var(--shadow-sm);
}

.sequence-editor__steps-column,
.sequence-editor__steps-panel,
.sequence-editor__main-column {
  display: flex;
  flex-direction: column;
}

.sequence-editor__steps-card {
  padding: 1rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.5rem;
  background: var(--color-neutral-50);
}

.sequence-editor__log {
  margin-top: 1rem;
}

@media (min-width: 640px) {
  .sequence-editor__workspace {
    padding: 1.25rem;
  }
}

@media (min-width: 1024px) {
  .sequence-editor {
    min-height: 0;
    overflow: hidden;
  }

  .sequence-editor__workspace {
    flex: 1 1 0;
    flex-direction: row;
    gap: 1.25rem;
  }

  .sequence-editor__steps-column {
    align-self: stretch;
    height: 100%;
    min-height: 0;
    flex: 0 0 auto;
  }

  .sequence-editor__steps-panel {
    flex: 1 1 auto;
    min-height: 0;
    height: 100%;
  }

  .sequence-editor__main-column,
  .sequence-editor__log {
    min-height: 0;
    flex: 1 1 auto;
    overflow: hidden;
  }
}

:global(.dark .sequence-editor__workspace) {
  background: var(--color-neutral-800);
}

:global(.dark .sequence-editor__steps-card) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
}
</style>
