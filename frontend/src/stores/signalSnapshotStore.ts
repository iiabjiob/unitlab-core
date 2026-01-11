import { defineStore } from "pinia"
import { computed, reactive, ref } from "vue"

import { SignalSnapshotsAPI, TestRunsAPI } from "@/api/signal_snapshots.api"
import type {
  Allocation,
  AllocationMappingItem,
  SignalImportMeta,
  SignalSnapshot,
  SignalSnapshotSummary,
  TestRun,
  TestRunCreatePayload,
} from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { getLogger } from "@/utils/logger"

const logger = getLogger("SIGNALS")

export const useSignalSnapshotStore = defineStore("signalSnapshotStore", () => {
  const workspaceStore = useWorkspaceStore()
  const snapshots = ref<SignalSnapshotSummary[]>([])
  const runs = ref<TestRun[]>([])
  const snapshotDetails = reactive<Record<number, SignalSnapshot>>({})
  const allocations: Record<number, Allocation> = {}
  const loading = ref(false)
  const runsLoading = ref(false)

  async function refreshSnapshots() {
    const workspaceId = workspaceStore.requireWorkspaceId()
    loading.value = true
    try {
      const { data } = await SignalSnapshotsAPI.list(workspaceId)
      snapshots.value = data
      logger.debug("📸 Loaded", data.length, "snapshots")
    } finally {
      loading.value = false
    }
  }

  async function importSnapshot(file: File, metadata?: SignalImportMeta) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    const { data } = await SignalSnapshotsAPI.import(workspaceId, file, metadata)
    snapshots.value = [data, ...snapshots.value]
    snapshotDetails[data.id] = data
    return data
  }

  async function lockSnapshot(snapshotId: number) {
    const { data } = await SignalSnapshotsAPI.lock(snapshotId)
    updateSnapshot(data)
    snapshotDetails[snapshotId] = data
    return data
  }

  async function deleteSnapshot(snapshotId: number) {
    await SignalSnapshotsAPI.delete(snapshotId)
    snapshots.value = snapshots.value.filter(s => s.id !== snapshotId)
    delete snapshotDetails[snapshotId]
    delete allocations[snapshotId]
  }

  function updateSnapshot(snapshot: SignalSnapshot) {
    const idx = snapshots.value.findIndex(s => s.id === snapshot.id)
    if (idx !== -1) snapshots.value[idx] = snapshot
  }

  async function getSnapshot(snapshotId: number, force = false) {
    if (!force && snapshotDetails[snapshotId]) {
      return snapshotDetails[snapshotId]
    }
    const { data } = await SignalSnapshotsAPI.get(snapshotId)
    snapshotDetails[snapshotId] = data
    updateSnapshot(data)
    return data
  }

  async function getAllocation(snapshotId: number, force = false) {
    if (!force && allocations[snapshotId]) return allocations[snapshotId]
    const { data } = await SignalSnapshotsAPI.getAllocation(snapshotId)
    allocations[snapshotId] = data
    return data
  }

  async function saveAllocation(snapshotId: number, mapping: AllocationMappingItem[]) {
    const { data } = await SignalSnapshotsAPI.updateAllocation(snapshotId, mapping)
    allocations[snapshotId] = data
    return data
  }

  async function refreshRuns() {
    const workspaceId = workspaceStore.requireWorkspaceId()
    runsLoading.value = true
    try {
      const { data } = await TestRunsAPI.list(workspaceId)
      runs.value = data
    } finally {
      runsLoading.value = false
    }
  }

  async function createTestRun(payload: TestRunCreatePayload) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    const { data } = await TestRunsAPI.create(workspaceId, payload)
    runs.value = [data, ...runs.value]
    return data
  }

  async function repeatRun(runId: number) {
    const { data } = await TestRunsAPI.repeat(runId)
    runs.value = [data, ...runs.value]
    return data
  }

  const draftSnapshots = computed(() => snapshots.value.filter(s => s.status === "draft"))
  const lockedSnapshots = computed(() => snapshots.value.filter(s => s.status === "locked"))

  return {
    snapshots,
    runs,
    loading,
    runsLoading,
    refreshSnapshots,
    importSnapshot,
    lockSnapshot,
    deleteSnapshot,
    getSnapshot,
    snapshotDetails,
    draftSnapshots,
    lockedSnapshots,
    getAllocation,
    saveAllocation,
    refreshRuns,
    createTestRun,
    repeatRun,
  }
})
