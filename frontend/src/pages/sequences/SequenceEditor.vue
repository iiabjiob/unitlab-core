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
  <div class="h-full flex flex-col pe-4">

    <!-- HEADER -->
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

    <div class="mt-5 flex flex-col gap-4 rounded bg-white p-4 shadow dark:bg-neutral-800 sm:p-5 lg:flex-1 lg:min-h-0 lg:flex-row lg:gap-5 lg:overflow-hidden">

      <div
        v-if="sequence"
        class="flex flex-col lg:min-h-0 lg:flex-none"
      >
        <ResizablePanel
          v-if="isDesktop"
          class="flex flex-col min-h-0 h-full"
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
          class="rounded-lg border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900"
        >
          <div>
            <SequenceStepsList :sequence="sequence" />
          </div>
        </div>
      </div>

      <!-- PANEL -->
      <div v-if="sequence && state" class="flex flex-col lg:flex-1 lg:min-h-0 lg:overflow-hidden">

        <!-- EDITOR -->
        <SequenceStepEditor
          :sequence="sequence"
          :step="selectedStep"
          @close="exitStepEdit"
        />

        <!-- LOG -->
        <div class="mt-4 lg:flex-1 lg:min-h-0 lg:overflow-hidden">
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
