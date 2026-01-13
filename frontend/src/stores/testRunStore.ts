import { defineStore } from "pinia"
import { computed, reactive, ref } from "vue"

import { TestRunsAPI } from "@/api/test_runs.api"
import type {
  TestRunCreatePayloadV2,
  TestRunRecord,
  TestRunSignalSnapshot,
} from "@/types/signal"
import type { SequenceState } from "@/types/sequences"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { getLogger } from "@/utils/logger"

const logger = getLogger("TEST_RUNS")

export const useTestRunStore = defineStore("testRunStore", () => {
  const workspaceStore = useWorkspaceStore()
  const testRuns = ref<TestRunRecord[]>([])
  const runSnapshots = reactive<Record<number, TestRunSignalSnapshot>>({})
  const snapshotLoading = reactive<Record<number, boolean>>({})
  const loading = ref(false)
  const initializedWorkspaceId = ref<number | null>(null)

  function upsertRun(run: TestRunRecord) {
    const idx = testRuns.value.findIndex(item => item.id === run.id)
    if (idx === -1) {
      testRuns.value = [run, ...testRuns.value]
    } else {
      testRuns.value[idx] = run
    }
  }

  async function refreshRuns(force = false) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    if (!force && testRuns.value.length && initializedWorkspaceId.value === workspaceId) {
      return
    }
    loading.value = true
    try {
      const { data } = await TestRunsAPI.list(workspaceId)
      testRuns.value = data
      initializedWorkspaceId.value = workspaceId
      logger.debug("🧪 Loaded", data.length, "test runs")
    } finally {
      loading.value = false
    }
  }

  async function getTestRun(runId: number, force = false) {
    if (!force) {
      const cached = testRuns.value.find(run => run.id === runId)
      if (cached) return cached
    }
    const { data } = await TestRunsAPI.get(runId)
    upsertRun(data)
    return data
  }

  async function createTestRun(payload: Omit<TestRunCreatePayloadV2, "workspace_id">) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    const { data } = await TestRunsAPI.create(workspaceId, payload)
    upsertRun(data)
    return data
  }

  async function repeatTestRun(runId: number) {
    const { data } = await TestRunsAPI.repeat(runId)
    upsertRun(data)
    return data
  }

  async function startTestRun(runId: number): Promise<SequenceState[]> {
    const { data } = await TestRunsAPI.start(runId)
    await getTestRun(runId, true)
    return data
  }

  async function stopTestRun(runId: number): Promise<SequenceState[]> {
    const { data } = await TestRunsAPI.stop(runId)
    await getTestRun(runId, true)
    return data
  }

  async function getRunSnapshot(runId: number, force = false) {
    if (!force && runSnapshots[runId]) {
      return runSnapshots[runId]
    }
    snapshotLoading[runId] = true
    try {
      const { data } = await TestRunsAPI.getSignalsSnapshot(runId)
      runSnapshots[runId] = data
      return data
    } finally {
      snapshotLoading[runId] = false
    }
  }

  const runningRuns = computed(() => testRuns.value.filter(run => run.status === "running"))
  const completedRuns = computed(() => testRuns.value.filter(run => run.status === "completed"))

  return {
    testRuns,
    runSnapshots,
    snapshotLoading,
    loading,
    runningRuns,
    completedRuns,
    refreshRuns,
    getTestRun,
    createTestRun,
    repeatTestRun,
    startTestRun,
    stopTestRun,
    getRunSnapshot,
  }
})
