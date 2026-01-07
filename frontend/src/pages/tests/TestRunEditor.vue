<template>
  <div v-if="!run" class="rounded-2xl border border-dashed border-neutral-300/80 bg-white/60 px-6 py-12 text-center text-neutral-600 dark:border-neutral-700 dark:bg-neutral-900/60">
    <p class="text-sm text-neutral-500 dark:text-neutral-400">Test run not found.</p>
    <button
      type="button"
      class="mt-4 rounded-md border border-neutral-300 px-4 py-2 text-sm text-neutral-700 transition hover:border-neutral-900 hover:text-neutral-900 dark:border-neutral-700 dark:text-neutral-300 dark:hover:border-neutral-500"
      @click="goBack"
    >
      Back to list
    </button>
  </div>

  <div v-else class="h-full flex flex-col">
    <TestRunEditorHeader :run="run" @duplicate="handleDuplicate" @delete="requestDelete" />

    <TestRunRunControls
      :run="run"
      :state="state"
      :total-steps="steps.length"
      :starting="starting"
      :cancelling="cancelling"
      @start="startRun"
      @cancel="cancelRun"
    />

    <div class="flex flex-1 overflow-hidden bg-white dark:bg-neutral-800 shadow rounded p-5">
      <ResizablePanel
        placement="left"
        storageKey="test-steps-list-width"
        :minSize="360"
        :defaultSize="380"
        :maxSize="700"
      >
        <TestRunStepsList :run="run" />
      </ResizablePanel>

      <div class="flex flex-col flex-1 overflow-hidden">
        <div class="flex-1 overflow-y-auto">
          <TestRunExecutionLog :run="run" :state="state" />
        </div>
      </div>
    </div>

    <ConfirmModal
      :open="deleteModalOpen"
      title="Delete test run"
      message="Deleting this test run removes all of its channel steps."
      confirm-label="Delete"
      cancel-label="Cancel"
      @confirm="confirmDelete"
      @cancel="cancelDelete"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { useRouter } from "vue-router"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import { useTestRunStore } from "@/stores/testRunStore"
import { useTestRunStepStore } from "@/stores/testRunStepStore"
import TestRunEditorHeader from "./components/TestRunEditorHeader.vue"
import TestRunRunControls from "./components/TestRunRunControls.vue"
import TestRunStepsList from "./components/TestRunStepsList.vue"
import TestRunExecutionLog from "./components/TestRunExecutionLog.vue"

const props = defineProps<{ id: string }>()
const router = useRouter()
const runStore = useTestRunStore()
const stepStore = useTestRunStepStore()

const runId = computed(() => Number(props.id))
const run = computed(() => runStore.runs.find(r => r.id === runId.value))
const steps = computed(() => stepStore.stepsByRun(runId.value).value)
const state = computed(() => runStore.states[runId.value])

const deleteModalOpen = ref(false)
const starting = ref(false)
const cancelling = ref(false)
const deleting = ref(false)

async function handleDuplicate() {
  if (!run.value) return
  const duplicated = await runStore.duplicateTestRun(run.value.id)
  stepStore.setActiveStep(null)
  await router.push({ name: "tests.detail", params: { id: duplicated.id } })
}

function requestDelete() {
  deleteModalOpen.value = true
}

function cancelDelete() {
  deleteModalOpen.value = false
}

async function confirmDelete() {
  if (!run.value || deleting.value) return
  deleting.value = true
  try {
    await runStore.deleteTestRun(run.value.id)
    stepStore.setActiveStep(null)
    router.replace({ name: "tests.list" })
  } finally {
    deleting.value = false
    deleteModalOpen.value = false
  }
}

async function startRun() {
  if (!run.value || starting.value) return
  starting.value = true
  try {
    await runStore.startRun(run.value.id)
  } finally {
    starting.value = false
  }
}

async function cancelRun() {
  if (!run.value || cancelling.value) return
  cancelling.value = true
  try {
    await runStore.cancelRun(run.value.id)
  } finally {
    cancelling.value = false
  }
}

function goBack() {
  router.replace({ name: "tests.list" })
}

watch(
  () => props.id,
  () => {
    if (run.value) {
      void runStore.refreshState(run.value.id)
    }
  },
)

onMounted(() => {
  if (run.value) {
    void runStore.refreshState(run.value.id)
  }
})
</script>
