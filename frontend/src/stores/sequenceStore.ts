import { defineStore } from "pinia"
import { ref, watch } from "vue"
import { SequencesAPI } from "@/api/sequences.api"
import {
  SequenceStatusEnum,
  type SequenceDef,
  type SequenceRuntimeState,
  type SequenceState,
  type SequenceStep,
} from "@/types/sequences"

import type { SequenceWsEvent } from "@/types/ws/events"
import { getLogger } from "@/utils/logger"
import { useSequenceStepStore } from "./sequenceStepStore"
import { useSequenceLogStore } from "@/stores/sequenceLogStore"
import { useWorkspaceStore } from "./workspaceStore"

const logger = getLogger("SEQ")

type RuntimeNestedProgress = {
  done: number
  total: number
}

export type SequenceExecutionProgress = {
  done: number
  total: number
  percent: number
  completedTopSteps: number
}

function createEmptyState(seqId: number, totalSteps: number): SequenceState {
  return {
    sequence_id: seqId,
    status: SequenceStatusEnum.IDLE,
    run_id: null,
    current_step_index: 0,
    total_steps: totalSteps,
    completed_step_ids: [],
    last_error: null,
    started_at: null,
    finished_at: null,
    runtime: null,
  }
}

type SequenceDeleteSnapshot = {
  id: number
  sequence: SequenceDef
  state?: SequenceState
  steps: SequenceStep[]
}

function cloneSequenceState(state?: SequenceState): SequenceState | undefined {
  if (!state) {
    return undefined
  }

  return {
    ...state,
    completed_step_ids: [...(state.completed_step_ids ?? [])],
    runtime: normalizeRuntime(state.runtime),
  }
}

function restoreFailedSequences(
  previous: SequenceDef[],
  current: SequenceDef[],
  failedSnapshots: SequenceDeleteSnapshot[],
): SequenceDef[] {
  const currentById = new Map(current.map(item => [item.id, item]))
  const previousIds = new Set(previous.map(item => item.id))
  const failedById = new Map(failedSnapshots.map(snapshot => [snapshot.id, snapshot.sequence]))
  const restoredInOriginalOrder = previous
    .filter(item => failedById.has(item.id) || currentById.has(item.id))
    .map(item => currentById.get(item.id) ?? failedById.get(item.id) ?? item)
  const currentExtras = current.filter(item => !previousIds.has(item.id))
  return [...restoredInOriginalOrder, ...currentExtras]
}

const mapStatus = (raw: string): SequenceStatusEnum => {
  switch (raw.toLowerCase()) {
    case "idle": return SequenceStatusEnum.IDLE
    case "pending": return SequenceStatusEnum.PENDING
    case "running": return SequenceStatusEnum.RUNNING
    case "cancelling": return SequenceStatusEnum.CANCELLING
    case "stopped": return SequenceStatusEnum.STOPPED
    case "completed": return SequenceStatusEnum.COMPLETED
    case "error": return SequenceStatusEnum.ERROR
    default:
      console.warn("Unknown sequence status:", raw)
      return SequenceStatusEnum.IDLE
  }
}

function normalizeRuntime(runtime?: SequenceRuntimeState | null): SequenceRuntimeState | null {
  if (!runtime) {
    return null
  }

  return {
    ...runtime,
    execution_path: [...(runtime.execution_path ?? [])],
  }
}

function describeRuntime(runtime?: SequenceRuntimeState | null): string {
  if (!runtime) {
    return ""
  }

  const parts: string[] = []
  const path = runtime.execution_path?.filter(Boolean) ?? []
  if (path.length > 1) {
    parts.push(path.join(" -> "))
  } else if (runtime.active_sequence_name) {
    parts.push(runtime.active_sequence_name)
  }

  if (typeof runtime.active_step_index === "number" && typeof runtime.active_total_steps === "number") {
    parts.push(`step ${runtime.active_step_index + 1}/${runtime.active_total_steps}`)
  }

  if (runtime.repeat_mode === "times" && typeof runtime.iteration_current === "number") {
    if (typeof runtime.iteration_total === "number") {
      parts.push(`iteration ${runtime.iteration_current}/${runtime.iteration_total}`)
    } else {
      parts.push(`iteration ${runtime.iteration_current}`)
    }
  } else if (runtime.repeat_mode === "duration" && typeof runtime.iteration_current === "number") {
    parts.push(`iteration ${runtime.iteration_current}`)
    parts.push("timed run")
  } else if (runtime.repeat_mode === "until_stopped" && typeof runtime.iteration_current === "number") {
    parts.push(`iteration ${runtime.iteration_current}`)
    parts.push("until stopped")
  }

  return parts.join(" · ")
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value))
}

function resolveCompletedTopStepCount(state: SequenceState): number {
  const total = Math.max(0, Number(state.total_steps ?? 0))
  const uniqueCompleted = new Set(
    (state.completed_step_ids ?? [])
      .map(stepId => Number(stepId))
      .filter(stepId => Number.isFinite(stepId)),
  )
  return clamp(uniqueCompleted.size, 0, total)
}

function isSequenceInFlight(status: SequenceStatusEnum): boolean {
  return status === SequenceStatusEnum.PENDING
    || status === SequenceStatusEnum.RUNNING
    || status === SequenceStatusEnum.CANCELLING
}

function resolveRuntimeNestedProgress(runtime?: SequenceRuntimeState | null): RuntimeNestedProgress | null {
  if (!runtime) {
    return null
  }

  const activeStepIndex = Math.max(0, Number(runtime.active_step_index ?? 0))
  const activeTotalSteps = Math.max(0, Number(runtime.active_total_steps ?? 0))
  if (activeTotalSteps <= 0) {
    return null
  }

  if (runtime.repeat_mode === "times") {
    const iterationCurrent = Math.max(0, Number(runtime.iteration_current ?? 0))
    const iterationTotal = Math.max(0, Number(runtime.iteration_total ?? 0))
    if (iterationCurrent <= 0 || iterationTotal <= 0) {
      return null
    }

    const total = iterationTotal * activeTotalSteps
    const done = ((iterationCurrent - 1) * activeTotalSteps) + clamp(activeStepIndex + 1, 0, activeTotalSteps)
    return { done: clamp(done, 0, total), total }
  }

  if ((runtime.execution_path?.length ?? 0) > 1) {
    return {
      done: clamp(activeStepIndex + 1, 0, activeTotalSteps),
      total: activeTotalSteps,
    }
  }

  return null
}

function resolveRuntimeStartFraction(runtime?: SequenceRuntimeState | null): number {
  if (!runtime) {
    return 0
  }

  const activeStepIndex = Math.max(0, Number(runtime.active_step_index ?? 0))
  const activeTotalSteps = Math.max(0, Number(runtime.active_total_steps ?? 0))
  if (activeTotalSteps <= 0) {
    return 0
  }

  if (runtime.repeat_mode === "times") {
    const iterationCurrent = Math.max(0, Number(runtime.iteration_current ?? 0))
    const iterationTotal = Math.max(0, Number(runtime.iteration_total ?? 0))
    if (iterationCurrent <= 0 || iterationTotal <= 0) {
      return 0
    }

    const total = iterationTotal * activeTotalSteps
    const doneAtStepStart = ((iterationCurrent - 1) * activeTotalSteps) + clamp(activeStepIndex, 0, activeTotalSteps)
    return clamp(doneAtStepStart / total, 0, 0.999)
  }

  if ((runtime.execution_path?.length ?? 0) > 1) {
    return clamp(activeStepIndex / activeTotalSteps, 0, 0.999)
  }

  return 0
}


export const useSequenceStore = defineStore("sequenceStore", () => {

  const sequences = ref<SequenceDef[]>([])
  const states = ref<Record<number, SequenceState>>({})
  const loading = ref(false)
  const loadedOnce = ref(false)
  const runtimeNestedProgressBySequence = ref<Record<number, RuntimeNestedProgress | null>>({})
  let tempSequenceId = -1
  const stepStore = useSequenceStepStore()
  const logStore = useSequenceLogStore()
  const workspaceStore = useWorkspaceStore()
    
  async function ensureLoaded() {
    if (!workspaceStore.activeWorkspaceId) {
      logger.debug("⏸️ No active workspace selected, skipping sequence load")
      return
    }

    if (!loadedOnce.value) {
      await fetchSequences()
    }
  }

  async function exportSequenceFile(id: number) {
    const { data } = await SequencesAPI.export(workspaceStore.requireWorkspaceId(), id)
    return data
  }

  async function importSequencesFile(file: File) {
    const formData = new FormData()
    formData.append("file", file)

    const { data } = await SequencesAPI.import(workspaceStore.requireWorkspaceId(), formData)
    if (Array.isArray(data)) {
      data.forEach(upsertSequence)
    }
    await fetchSequences()
    return Array.isArray(data) ? data : []
  }

  function stepsCount(seqId: number): number {
    return stepStore.stepsBySequence(seqId).value.length
  }

  function ensureState(seq: SequenceDef | number): SequenceState {
    const id = typeof seq === "number" ? seq : seq.id
    const total = stepsCount(id)

    if (!states.value[id]) {
      states.value[id] = createEmptyState(id, total)
    } else {
      states.value[id].total_steps = total
    }

    return states.value[id]
  }

  function resetState(seqId: number) {
    states.value[seqId] = createEmptyState(seqId, stepsCount(seqId))
    clearRuntimeNestedProgress(seqId)
  }

  function applySnapshot(seqId: number, snapshot: SequenceState) {
    const total = snapshot.total_steps ?? stepsCount(seqId)

    states.value[seqId] = {
      ...createEmptyState(seqId, total),
      ...snapshot,
      status: mapStatus(snapshot.status as any),
      total_steps: total,
      completed_step_ids: [...(snapshot.completed_step_ids ?? [])],
      runtime: normalizeRuntime(snapshot.runtime),
    }

    if (!isSequenceInFlight(states.value[seqId].status)) {
      clearRuntimeNestedProgress(seqId)
    }

    return states.value[seqId]
  }

  function upsertSequence(seq: SequenceDef) {
    const idx = sequences.value.findIndex(s => s.id === seq.id)
    const normalized = { ...seq }
    delete normalized.steps

    if (idx === -1) sequences.value.push(normalized)
    else sequences.value[idx] = normalized

    if (seq.steps?.length) {
      stepStore.hydrateSequence(seq.id, seq.steps)
    }

    ensureState(seq.id)
  }

  function createOptimisticSequence(payload: { name: string; description?: string }): SequenceDef {
    const id = tempSequenceId--
    const now = new Date().toISOString()
    const optimistic: SequenceDef = {
      id,
      workspace_ids: workspaceStore.activeWorkspaceId ? [workspaceStore.activeWorkspaceId] : [],
      name: payload.name,
      description: payload.description ?? "",
      system_key: null,
      system_provided: false,
      read_only: false,
      created_at: now,
      updated_at: now,
      steps: [],
    }

    upsertSequence(optimistic)
    resetState(id)
    return optimistic
  }

  async function fetchSequences() {
    if (!workspaceStore.activeWorkspaceId) return
    const workspaceId = workspaceStore.requireWorkspaceId()
    loading.value = true
    try {
      const { data } = await SequencesAPI.list(workspaceId)

      sequences.value = []
      data.forEach(upsertSequence)
      loadedOnce.value = true
      logger.info(`📡 Loaded ${data.length} sequences for workspace ${workspaceId}`)
    } finally {
      loading.value = false
    }
  }

  function nextDefaultName(): string {
    const base = "New Instruction"
    const names = sequences.value.map(s => s.name)

    let n = 1
    while (names.includes(`${base} ${n}`)) {
      n++
    }

    return `${base} ${n}`
  }

  function nextDuplicateName(sourceName: string): string {
    const base = sourceName.trim() || "Instruction"
    const names = new Set(sequences.value.map(s => s.name))

    let suffix = " copy"
    let counter = 2
    let candidate = `${base}${suffix}`

    while (names.has(candidate)) {
      candidate = `${base}${suffix} ${counter}`
      counter += 1
    }

    return candidate
  }

  async function createSequenceAuto() {
    const name = nextDefaultName()
    return await createSequence({ name, description: "" })
  }

  async function createSequence(payload: { name: string; description?: string }) {
    const optimistic = createOptimisticSequence(payload)

    try {
      const { data } = await SequencesAPI.create(workspaceStore.requireWorkspaceId(), { ...payload, steps: [] })
      sequences.value = sequences.value.filter((sequence) => sequence.id !== optimistic.id)
      delete states.value[optimistic.id]
      upsertSequence(data)
      await refreshState(data.id)
      return data
    } catch (error) {
      sequences.value = sequences.value.filter((sequence) => sequence.id !== optimistic.id)
      delete states.value[optimistic.id]
      throw error
    }
  }

  async function updateSequence(id: number, payload: Partial<SequenceDef>) {
    const { data } = await SequencesAPI.update(workspaceStore.requireWorkspaceId(), id, payload)
    upsertSequence(data)
    return data
  }

  async function deleteSequence(id: number) {
    await deleteSequences([id])
  }

  async function deleteSequences(ids: number[]) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    const requestedIds = Array.from(new Set(ids.filter(id => Number.isFinite(id))))
    const requestedIdSet = new Set(requestedIds)
    const previous = [...sequences.value]
    const snapshots = previous
      .filter(sequence => requestedIdSet.has(sequence.id))
      .map(sequence => ({
        id: sequence.id,
        sequence,
        state: cloneSequenceState(states.value[sequence.id]),
        steps: [...stepStore.stepsBySequence(sequence.id).value],
      }))

    if (!snapshots.length) {
      return { deleted: 0 }
    }

    const deleteIds = new Set(snapshots.map(snapshot => snapshot.id))
    sequences.value = sequences.value.filter(sequence => !deleteIds.has(sequence.id))
    snapshots.forEach((snapshot) => {
      delete states.value[snapshot.id]
      clearRuntimeNestedProgress(snapshot.id)
      stepStore.dropSequence(snapshot.id)
    })

    const results = await Promise.allSettled(
      snapshots.map(snapshot => SequencesAPI.delete(workspaceId, snapshot.id)),
    )
    const failedSnapshots = snapshots.filter((_, index) => results[index]?.status === "rejected")
    if (failedSnapshots.length) {
      sequences.value = restoreFailedSequences(previous, sequences.value, failedSnapshots)
      failedSnapshots.forEach((snapshot) => {
        if (snapshot.state) {
          states.value[snapshot.id] = cloneSequenceState(snapshot.state) ?? snapshot.state
        }
        if (snapshot.steps.length) {
          stepStore.hydrateSequence(snapshot.id, snapshot.steps)
        }
      })
      const firstFailure = results.find((result): result is PromiseRejectedResult => result.status === "rejected")
      logger.error(`💥 Failed to delete ${failedSnapshots.length}/${snapshots.length} sequences:`, firstFailure?.reason)
      throw firstFailure?.reason ?? new Error("Failed to delete instructions")
    }

    logger.info(`🗑️ Deleted ${snapshots.length} instruction${snapshots.length === 1 ? "" : "s"}`)
    return { deleted: snapshots.length }
  }

  async function duplicateSequence(id: number) {
    const original = sequences.value.find(seq => seq.id === id)
    if (!original) {
      throw new Error(`Sequence ${id} not found`)
    }

    await stepStore.ensureSteps(id)
    const stepsPayload = stepStore.stepsBySequence(id).value
      .map(step => ({
        sequence_step_type: step.sequence_step_type,
        channel_id: step.channel_id,
        payload: step.payload ?? null,
      }))

    const { data } = await SequencesAPI.create(workspaceStore.requireWorkspaceId(), {
      name: nextDuplicateName(original.name),
      description: original.description ?? "",
      steps: stepsPayload,
    })

    upsertSequence(data)
    await refreshState(data.id)
    return data
  }

  async function refreshState(id: number) {
    const { data } = await SequencesAPI.getState(workspaceStore.requireWorkspaceId(), id)
    return applySnapshot(id, data)
  }

  async function startSequence(id: number) {
    const { data } = await SequencesAPI.start(workspaceStore.requireWorkspaceId(), id)
    clearRuntimeNestedProgress(id)
    return applySnapshot(id, data)
  }

  async function stopSequence(id: number) {
    const { data } = await SequencesAPI.stop(workspaceStore.requireWorkspaceId(), id)
    return applySnapshot(id, data)
  }

  function handleSequenceEvent(event: SequenceWsEvent) {
    if (!sequences.value.some(seq => seq.id === event.sequence_id)) {
      return
    }
    const prev = ensureState(event.sequence_id)

    switch (event.event) {
      case "started":
        clearRuntimeNestedProgress(event.sequence_id)
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.RUNNING,
          current_step_index: 0,
          completed_step_ids: [],
          last_error: null,
          runtime: normalizeRuntime(event.runtime),
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Instruction started",
          run_id: event.run_id,
        })
        break

      case "progress":
        if (event.progress_scope === "nested_step") {
          setRuntimeNestedProgress(event.sequence_id, resolveRuntimeNestedProgress(event.runtime))
        } else {
          clearRuntimeNestedProgress(event.sequence_id)
        }

        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.RUNNING,
          current_step_index: event.progress_scope === "step"
            ? Math.min(event.step_index + 1, prev.total_steps)
            : event.step_index,
          completed_step_ids: event.progress_scope === "step"
            ? [...event.completed_steps]
            : [...prev.completed_step_ids],
          last_error: null,
          runtime: normalizeRuntime(event.runtime) ?? prev.runtime ?? null,
        }

        logStore.push(event.sequence_id, {
          type: "step",
          message: event.progress_scope === "step"
            ? `Step ${event.step_index + 1}/${prev.total_steps} completed`
            : `Nested step completed${describeRuntime(event.runtime) ? ` · ${describeRuntime(event.runtime)}` : ""}`,
          run_id: event.run_id,
          ...buildStepLogMeta(event.sequence_id, {
            stepId: event.step_id,
            stepIndex: event.step_index,
            stepType: event.step_type,
          }),
        })
        break

      case "stopping":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.CANCELLING,
          current_step_index: event.current_step_index,
          total_steps: event.total_steps ?? prev.total_steps,
          runtime: normalizeRuntime(event.runtime) ?? prev.runtime ?? null,
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Stop requested, waiting for current step to finish",
          run_id: event.run_id,
        })
        break

      case "step_error":
        clearRuntimeNestedProgress(event.sequence_id)
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.ERROR,
          current_step_index: event.step_index,
          last_error: event.message,
          runtime: normalizeRuntime(event.runtime) ?? prev.runtime ?? null,
        }

        logStore.push(event.sequence_id, {
          type: "error",
          message: `Step ${event.step_index + 1} error: ${event.message}`,
          run_id: event.run_id,
          ...buildStepLogMeta(event.sequence_id, {
            stepId: event.step_id,
            stepIndex: event.step_index,
          }),
        })
        break

      case "error":
        clearRuntimeNestedProgress(event.sequence_id)
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.ERROR,
          last_error: event.message,
          runtime: normalizeRuntime(event.runtime) ?? prev.runtime ?? null,
        }

        logStore.push(event.sequence_id, {
          type: "error",
          message: `Instruction error: ${event.message}`,
          run_id: event.run_id,
        })
        break

      case "stopped":
        clearRuntimeNestedProgress(event.sequence_id)
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.STOPPED,
          runtime: normalizeRuntime(event.runtime) ?? prev.runtime ?? null,
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Instruction stopped by user",
          run_id: event.run_id,
        })
        break

      case "completed":
        clearRuntimeNestedProgress(event.sequence_id)
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.COMPLETED,
          current_step_index: prev.total_steps,
          completed_step_ids: [...prev.completed_step_ids],
          runtime: normalizeRuntime(event.runtime),
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Instruction completed successfully",
          run_id: event.run_id,
        })
        break
    }
  }

  function resetForWorkspaceChange() {
    sequences.value = []
    states.value = {}
    runtimeNestedProgressBySequence.value = {}
    loadedOnce.value = false
    stepStore.resetAll()
    logStore.resetAll()
  }

  function setRuntimeNestedProgress(seqId: number, progress: RuntimeNestedProgress | null) {
    runtimeNestedProgressBySequence.value = {
      ...runtimeNestedProgressBySequence.value,
      [seqId]: progress,
    }
  }

  function clearRuntimeNestedProgress(seqId: number) {
    if (!(seqId in runtimeNestedProgressBySequence.value)) {
      return
    }

    const next = { ...runtimeNestedProgressBySequence.value }
    delete next[seqId]
    runtimeNestedProgressBySequence.value = next
  }

  function findStepById(seqId: number, stepId?: number | null): SequenceStep | undefined {
    if (!stepId) return undefined
    return stepStore.steps.find((step) => step.sequence_id === seqId && step.id === stepId)
  }

  function findStepByIndex(seqId: number, index?: number | null): SequenceStep | undefined {
    if (index === undefined || index === null || index < 0) return undefined
    return stepStore.steps.find((step) => step.sequence_id === seqId && step.order_index === index)
  }

  function buildStepLogMeta(
    seqId: number,
    opts: { stepId?: number | null; stepIndex?: number | null; stepType?: string },
  ): Record<string, any> {
    const meta: Record<string, any> = {}

    if (opts.stepId != null) meta.step_id = opts.stepId
    if (opts.stepIndex != null) meta.step_index = opts.stepIndex
    if (opts.stepType) meta.step_type = opts.stepType

    const step = findStepById(seqId, opts.stepId) ?? findStepByIndex(seqId, opts.stepIndex)
    if (step) {
      meta.step_id = step.id
      meta.step_index = step.order_index
      meta.step_type = step.sequence_step_type
      meta.step_label = stepStore.getStepDescription(step)
      meta.step_payload = step.payload ?? null
      meta.channel_id = step.channel_id ?? null
      meta.details = meta.step_label
    }

    return meta
  }

  watch(
    () => workspaceStore.activeWorkspaceId,
    (workspaceId) => {
      resetForWorkspaceChange()
      if (workspaceId) {
        void fetchSequences()
      }
    },
  )



  function getExecutionProgress(seq: SequenceDef | number): SequenceExecutionProgress {
    const st = ensureState(seq)
    const total = Math.max(0, Number(st.total_steps ?? 0))
    if (total === 0) {
      return {
        done: 0,
        total: 0,
        percent: 0,
        completedTopSteps: 0,
      }
    }

    const completedTopSteps = resolveCompletedTopStepCount(st)
    if (st.status === SequenceStatusEnum.COMPLETED) {
      return {
        done: total,
        total,
        percent: 100,
        completedTopSteps: total,
      }
    }

    let activeFraction = 0
    const nestedProgress = runtimeNestedProgressBySequence.value[st.sequence_id] ?? null
    if (isSequenceInFlight(st.status) && completedTopSteps < total) {
      if (nestedProgress && nestedProgress.total > 0) {
        activeFraction = clamp(nestedProgress.done / nestedProgress.total, 0, 0.999)
      } else {
        activeFraction = resolveRuntimeStartFraction(st.runtime)
      }
    }

    const done = clamp(completedTopSteps + activeFraction, 0, total)
    return {
      done,
      total,
      percent: Math.max(0, Math.min(100, Math.round((done / total) * 100))),
      completedTopSteps,
    }
  }

  function getProgress(seq: SequenceDef | number) {
    return getExecutionProgress(seq).percent
  }

  return {
    sequences,
    states,
    loading,
    ensureLoaded,
    fetchSequences,
    updateSequence,
    createSequence,
    createSequenceAuto,
    deleteSequence,
    deleteSequences,
    duplicateSequence,
    resetState,

    refreshState,
    startSequence,
    stopSequence,
    exportSequenceFile,
    importSequencesFile,
    handleSequenceEvent,

    getExecutionProgress,
    getProgress,
    isRunning: (s: SequenceDef | number) => ensureState(s).status === SequenceStatusEnum.RUNNING,

  }
})
