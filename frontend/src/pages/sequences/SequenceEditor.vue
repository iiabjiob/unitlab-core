<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSequenceStore } from "@/stores/sequenceStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import { useSelectionStore } from "@/stores/selectionStore"
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

function requestDelete() {
  deleteModalOpen.value = true
}

function cancelDelete() {
  deleteModalOpen.value = false
}

async function confirmDelete() {
  if (!sequence.value) return
  await store.deleteSequence(sequence.value.id)
  deleteModalOpen.value = false
  await router.push({ name: "instructions.list" })
}

function exitStepEdit() {
  stepStore.setActiveStep(null)
}

const { isDesktop } = useViewport()
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <SequenceEditorHeader
      v-if="sequence"
      :sequence="sequence"
      @duplicate="handleDuplicate"
      @delete="requestDelete"
    />

    <SequenceRunControls
      v-if="sequence && state"
      :sequence="sequence"
      :state="state"
    />

    <div class="mt-5 flex flex-1 min-h-0 flex-col gap-4 overflow-hidden rounded bg-white p-4 shadow dark:bg-neutral-800 sm:p-5 lg:flex-row lg:gap-5">

      <div
        v-if="sequence"
        class="flex flex-col min-h-0 lg:flex-none"
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
          class="flex-1 min-h-0 overflow-hidden rounded-lg border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900"
        >
          <div class="h-full min-h-0 overflow-y-auto">
            <SequenceStepsList :sequence="sequence" />
          </div>
        </div>
      </div>

      <!-- PANEL -->
      <div v-if="sequence && state" class="flex flex-1 min-h-0 flex-col overflow-hidden">

        <!-- EDITOR -->
        <SequenceStepEditor
          :sequence="sequence"
          :step="selectedStep"
          @close="exitStepEdit"
        />

        <!-- LOG -->
        <div class="mt-4 flex-1 min-h-0 overflow-hidden">
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
