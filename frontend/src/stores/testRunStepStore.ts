import { defineStore } from "pinia"
import { computed, ref, watch } from "vue"
import type { TestRunStep } from "@/types/testRuns"
import { TestsAPI } from "@/api/tests.api"
import { useProjectStore } from "./projectStore"
import { useChannelStore } from "./channelStore"
import { getLogger } from "@/utils/logger"

const logger = getLogger("TEST-STEPS")

export const useTestRunStepStore = defineStore("testRunStepStore", () => {
  const steps = ref<TestRunStep[]>([])
  const loadedRuns = ref<Set<number>>(new Set())
  const activeStepId = ref<number | null>(null)
  const projectStore = useProjectStore()
  const channelStore = useChannelStore()

  const stepsByRun = (runId: number) =>
    computed(() => steps.value.filter(step => step.test_run_id === runId))

  function setActiveStep(stepId: number | null) {
    activeStepId.value = stepId
  }

  function setStepsForRun(runId: number, payload: TestRunStep[]) {
    steps.value = steps.value
      .filter(step => step.test_run_id !== runId)
      .concat([...payload].sort((a, b) => a.order_index - b.order_index))
  }

  function dropRun(runId: number) {
    steps.value = steps.value.filter(step => step.test_run_id !== runId)
    loadedRuns.value.delete(runId)
    if (activeStepId.value) {
      const exists = steps.value.some(step => step.id === activeStepId.value)
      if (!exists) {
        activeStepId.value = null
      }
    }
  }

  function describeStep(step: TestRunStep): string {
    if (!step.channel_id) {
      return `Channel #${step.order_index + 1}`
    }
    const channel = channelStore.channels.find(ch => ch.id === step.channel_id)
    if (channel) {
      return channelStore.resolveChannelFullLabel(channel)
    }
    if (step.channel?.resolved_name) {
      return `${step.channel.resolved_name}`
    }
    return `Channel ${step.channel_id}`
  }

  async function ensureSteps(runId: number) {
    if (!loadedRuns.value.has(runId)) {
      await fetchSteps(runId)
      loadedRuns.value.add(runId)
    }
  }

  async function fetchSteps(runId: number) {
    const projectId = projectStore.requireProjectId()
    const { data } = await TestsAPI.getSteps(projectId, runId)
    setStepsForRun(runId, data)
    loadedRuns.value.add(runId)
    logger.info(`📡 Loaded ${data.length} test steps for run ${runId}`)
  }

  async function bulkAdd(runId: number, channelIds: number[]) {
    if (!channelIds.length) return []
    const projectId = projectStore.requireProjectId()
    const { data } = await TestsAPI.bulkAddSteps(projectId, runId, { channel_ids: channelIds })
    setStepsForRun(runId, data)
    logger.info(`➕ Added ${channelIds.length} channels to test run ${runId}`)
    return data
  }

  async function deleteStep(runId: number, stepId: number) {
    const projectId = projectStore.requireProjectId()
    await TestsAPI.deleteStep(projectId, runId, stepId)
    steps.value = steps.value.filter(step => step.id !== stepId)
    return true
  }

  async function reorderSteps(runId: number, newOrder: number[]) {
    const projectId = projectStore.requireProjectId()
    const { data } = await TestsAPI.reorderSteps(projectId, runId, { new_order: newOrder })
    setStepsForRun(runId, data)
    return data
  }

  function hydrate(runId: number, payload: TestRunStep[] = []) {
    if (payload.length) {
      setStepsForRun(runId, payload)
      loadedRuns.value.add(runId)
    }
  }

  function resetAll() {
    steps.value = []
    loadedRuns.value = new Set()
    activeStepId.value = null
  }

  watch(
    () => projectStore.activeProjectId,
    () => resetAll(),
  )

  return {
    steps,
    stepsByRun,
    activeStepId,
    setActiveStep,
    ensureSteps,
    fetchSteps,
    bulkAdd,
    deleteStep,
    reorderSteps,
    describeStep,
    hydrate,
    dropRun,
    resetAll,
  }
})
