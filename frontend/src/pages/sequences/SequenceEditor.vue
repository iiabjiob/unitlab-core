<script setup lang="ts">
import { computed, ref } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSequenceStore } from "@/stores/sequenceStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"

import SequenceEditorHeader from "./components/SequenceEditorHeader.vue"
import SequenceRunControls from "./components/SequenceRunControls.vue"
import SequenceStepsList from "./components/SequenceStepsList.vue"
import SequenceExecutionLog from "./components/SequenceExecutionLog.vue"
import SequenceStepEditor from "./components/SequenceStepEditor.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSequenceStore()
const stepStore = useSequenceStepStore()

const sequenceId = computed(() => Number(route.params.id))

const sequence = computed(() =>
  store.sequences.find(s => s.id === sequenceId.value)
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
  sequence.value ? `Sequence "${sequence.value.name}" will be deleted with all steps.` : ""
)
const deleteConfirmLabel = "Delete"
const deleteCancelLabel = "Cancel"

async function handleDuplicate() {
  if (!sequence.value) return
  const duplicated = await store.duplicateSequence(sequence.value.id)
  await router.push({ name: "sequences.detail", params: { id: duplicated.id } })
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
  await router.push({ name: "sequences.list" })
}

function exitStepEdit() {
  stepStore.setActiveStep(null)
}
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

    <!-- RUN CONTROLS -->
    <SequenceRunControls
      v-if="sequence && state"
      :sequence="sequence"
      :state="state"
      @start="store.startSequence(sequenceId)"
      @stop="store.stopSequence(sequenceId)"
    />

    <div class="flex flex-1 overflow-hidden bg-white dark:bg-neutral-800 shadow rounded p-5">

      <!-- STEP LIST -->
      <ResizablePanel
        v-if="sequence" :sequence="sequence"
        placement="left"
        storageKey="sequence-steps-list-width"
        :minSize="380"
        :defaultSize="380"
        :maxSize="800"
        >
        <SequenceStepsList :sequence="sequence" />
      </ResizablePanel>
      
      <!-- PANEL -->
      <div v-if="sequence && state" class="flex flex-col flex-1 overflow-hidden">

        <!-- EDITOR -->
        <SequenceStepEditor          
          :sequence="sequence"
          :step="selectedStep"
          @close="exitStepEdit"
        />

        <!-- LOG -->
        <div class="flex-1 overflow-y-auto mt-2">
          <SequenceExecutionLog :sequence="sequence" :state="state" />
        </div>

      </div>

    </div>

    <ConfirmModal
      v-if="sequence"
      :open="deleteModalOpen"
      title="Delete sequence"
      :message="deleteMessage"
      :confirm-label="deleteConfirmLabel"
      :cancel-label="deleteCancelLabel"
      @confirm="confirmDelete"
      @cancel="cancelDelete"
    />
  </div>
</template>
