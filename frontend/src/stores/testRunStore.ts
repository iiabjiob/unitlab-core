import { defineStore } from "pinia"
import { ref, watch } from "vue"
import { TestsAPI } from "@/api/tests.api"
import {
  TestRunStatusEnum,
  type TestRunCreatePayload,
  type TestRunSchema,
  type TestRunState,
  type TestRunSummary,
  type TestRunUpdatePayload,
} from "@/types/testRuns"
import { useProjectStore } from "./projectStore"
import { useTestRunStepStore } from "./testRunStepStore"
import { getLogger } from "@/utils/logger"
import { useTestRunLogStore } from "@/stores/testRunLogStore"

const logger = getLogger("TEST-RUNS")

function createEmptyState(runId: number, totalSteps: number): TestRunState {
  return {
    id: runId,
    status: TestRunStatusEnum.PENDING,
    started_at: null,
    finished_at: null,
    current_step_index: 0,
    total_steps: totalSteps,
    error_message: null,
  }
}

export const useTestRunStore = defineStore("testRunStore", () => {
  const runs = ref<TestRunSummary[]>([])
  const states = ref<Record<number, TestRunState>>({})
  const loading = ref(false)
  const loadedOnce = ref(false)

  const projectStore = useProjectStore()
  const stepStore = useTestRunStepStore()
  const logStore = useTestRunLogStore()

  function stepsCount(runId: number) {
    return stepStore.stepsByRun(runId).value.length
  }

  function ensureState(runId: number) {
    const total = stepsCount(runId)
    if (!states.value[runId]) {
      states.value[runId] = createEmptyState(runId, total)
    } else {
      states.value[runId].total_steps = total
    }
    return states.value[runId]
  }

  function resetState(runId: number) {
    states.value[runId] = createEmptyState(runId, stepsCount(runId))
    return states.value[runId]
  }

  function applySnapshot(runId: number, snapshot: TestRunState) {
    const previous = states.value[runId]
    const total = snapshot.total_steps ?? stepsCount(runId)

    const nextState: TestRunState = {
      ...createEmptyState(runId, total),
      ...snapshot,
      status: snapshot.status,
      total_steps: total,
    }

    states.value[runId] = nextState
    emitLogUpdates(runId, previous, nextState)

    return nextState
  }

  function emitLogUpdates(runId: number, previous: TestRunState | undefined, nextState: TestRunState) {
    if (!previous) return

    if (nextState.current_step_index > previous.current_step_index) {
      const humanIndex = Math.min(
        nextState.current_step_index + 1,
        Math.max(nextState.total_steps, 1),
      )
      logStore.push(runId, {
        type: "step",
        message: `Step ${humanIndex}/${Math.max(nextState.total_steps, 1)} completed`,
      })
    }

    if (nextState.status !== previous.status) {
      switch (nextState.status) {
        case TestRunStatusEnum.RUNNING:
          logStore.push(runId, { type: "info", message: "Run started" })
          break
        case TestRunStatusEnum.COMPLETED:
          logStore.push(runId, { type: "info", message: "Run completed successfully" })
          break
        case TestRunStatusEnum.FAILED:
          logStore.push(runId, { type: "error", message: "Run failed" })
          break
        case TestRunStatusEnum.CANCELLED:
          logStore.push(runId, { type: "info", message: "Run cancelled" })
          break
        case TestRunStatusEnum.PENDING:
          logStore.push(runId, { type: "info", message: "Run reset to pending" })
          break
      }
    }

    if (nextState.error_message && nextState.error_message !== previous.error_message) {
      logStore.push(runId, {
        type: "error",
        message: nextState.error_message,
      })
    }
  }

  function toSummary(payload: TestRunSchema | TestRunSummary): TestRunSummary {
    if ("steps" in payload) {
      const { steps: _ignoredSteps, ...rest } = payload as TestRunSchema
      return rest
    }
    return payload
  }

  function upsertRun(payload: TestRunSchema | TestRunSummary) {
    const idx = runs.value.findIndex(run => run.id === payload.id)
    const normalized = toSummary(payload)

    if (idx === -1) {
      runs.value.push(normalized)
    } else {
      runs.value[idx] = normalized
    }

    if ("steps" in payload && payload.steps) {
      stepStore.hydrate(payload.id, payload.steps)
    }

    ensureState(payload.id)
  }

  async function fetchRuns() {
    if (!projectStore.activeProjectId) return
    loading.value = true
    try {
      const { data } = await TestsAPI.list(projectStore.requireProjectId())
      runs.value = []
      data.forEach(upsertRun)
      loadedOnce.value = true
      logger.info(`📡 Loaded ${data.length} test runs`)
    } finally {
      loading.value = false
    }
  }

  async function ensureLoaded(force = false) {
    if (!projectStore.activeProjectId) return
    if (loadedOnce.value && !force) return
    await fetchRuns()
  }

  function nextDefaultName(): string {
    const base = "New Test Run"
    const existing = new Set(runs.value.map(run => run.name))
    let counter = 1
    let candidate = `${base} ${counter}`
    while (existing.has(candidate)) {
      counter += 1
      candidate = `${base} ${counter}`
    }
    return candidate
  }

  function nextDuplicateName(sourceName: string): string {
    const base = sourceName.trim() || "Test Run"
    const names = new Set(runs.value.map(run => run.name))
    let suffix = " copy"
    let counter = 2
    let candidate = `${base}${suffix}`

    while (names.has(candidate)) {
      candidate = `${base}${suffix} ${counter}`
      counter += 1
    }

    return candidate
  }

  async function createTestRun(payload: TestRunCreatePayload) {
    const projectId = projectStore.requireProjectId()
    const prepared = { ...payload }
    if (!prepared.name?.trim()) {
      prepared.name = nextDefaultName()
    }
    const { data } = await TestsAPI.create(projectId, prepared)
    upsertRun(data)
    await refreshState(data.id)
    return data
  }

  async function duplicateTestRun(runId: number) {
    const original = runs.value.find(run => run.id === runId)
    if (!original) {
      throw new Error(`Test run ${runId} not found`)
    }

    await stepStore.ensureSteps(runId)
    const channelIds = stepStore
      .stepsByRun(runId)
      .value
      .map(step => step.channel_id)
      .filter((id): id is number => typeof id === "number")

    const payload: TestRunCreatePayload = {
      name: nextDuplicateName(original.name),
      channel_ids: channelIds,
      settings: { delay_ms: original.settings.delay_ms },
    }

    const projectId = projectStore.requireProjectId()
    const { data } = await TestsAPI.create(projectId, payload)
    upsertRun(data)
    await refreshState(data.id)
    return data
  }

  async function updateTestRun(runId: number, payload: TestRunUpdatePayload) {
    const projectId = projectStore.requireProjectId()
    const { data } = await TestsAPI.update(projectId, runId, payload)
    upsertRun(data)
    return data
  }

  async function deleteTestRun(runId: number) {
    const projectId = projectStore.requireProjectId()
    await TestsAPI.delete(projectId, runId)
    runs.value = runs.value.filter(run => run.id !== runId)
    delete states.value[runId]
    stepStore.dropRun(runId)
    logStore.clear(runId)
  }

  async function refreshState(runId: number) {
    const projectId = projectStore.requireProjectId()
    const { data } = await TestsAPI.state(projectId, runId)
    return applySnapshot(runId, data)
  }

  async function startRun(runId: number) {
    const projectId = projectStore.requireProjectId()
    await TestsAPI.start(projectId, runId)
    return refreshState(runId)
  }

  async function cancelRun(runId: number) {
    const projectId = projectStore.requireProjectId()
    await TestsAPI.cancel(projectId, runId)
    return refreshState(runId)
  }

  function resetForProjectChange() {
    runs.value = []
    states.value = {}
    loadedOnce.value = false
    logStore.resetAll()
  }

  watch(
    () => projectStore.activeProjectId,
    (projectId) => {
      resetForProjectChange()
      if (projectId) {
        void fetchRuns()
      }
    },
  )

  return {
    runs,
    states,
    loading,
    ensureLoaded,
    fetchRuns,
    createTestRun,
    updateTestRun,
    deleteTestRun,
    refreshState,
    startRun,
    cancelRun,
    resetState,
    nextDefaultName,
    nextDuplicateName,
    duplicateTestRun,
    isRunning: (runId: number) => ensureState(runId).status === TestRunStatusEnum.RUNNING,
  }
})
