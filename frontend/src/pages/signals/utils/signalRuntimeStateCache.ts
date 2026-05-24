export type SignalRuntimeState = {
  testedAt?: string | null
  status?: string | null
  value?: unknown
  updatedAt?: string | null
  reason?: string | null
}

export type SignalRuntimeStatePatch = {
  signalId: number
  state: SignalRuntimeState
}

export type SignalRuntimeStatePatchResult = {
  changed: number
  signalIds: number[]
}

function normalizeSignalId(value: unknown): number | null {
  const signalId = Number(value)
  return Number.isFinite(signalId) && signalId > 0 ? signalId : null
}

function normalizeText(value: unknown): string | null {
  const text = String(value ?? "").trim()
  return text || null
}

function normalizeState(state: SignalRuntimeState): SignalRuntimeState {
  const normalized: SignalRuntimeState = {}
  if ("testedAt" in state) normalized.testedAt = normalizeText(state.testedAt)
  if ("status" in state) normalized.status = normalizeText(state.status)
  if ("value" in state) normalized.value = state.value
  if ("updatedAt" in state) normalized.updatedAt = normalizeText(state.updatedAt)
  if ("reason" in state) normalized.reason = normalizeText(state.reason)
  return normalized
}

function stateChanged(current: SignalRuntimeState | undefined, patch: SignalRuntimeState): boolean {
  return Object.entries(patch).some(([key, value]) => (
    !Object.is(current?.[key as keyof SignalRuntimeState], value)
  ))
}

export function createSignalRuntimeStateCache() {
  const statesBySignalId = new Map<number, SignalRuntimeState>()
  let version = 0

  function patchStates(patches: readonly SignalRuntimeStatePatch[]): SignalRuntimeStatePatchResult {
    let changed = 0
    const signalIds: number[] = []

    patches.forEach((patch) => {
      const signalId = normalizeSignalId(patch.signalId)
      if (signalId === null) {
        return
      }
      const normalized = normalizeState(patch.state)
      if (Object.keys(normalized).length === 0) {
        return
      }
      const current = statesBySignalId.get(signalId)
      if (!stateChanged(current, normalized)) {
        return
      }
      statesBySignalId.set(signalId, {
        ...(current ?? {}),
        ...normalized,
      })
      changed += 1
      signalIds.push(signalId)
    })

    if (changed > 0) {
      version += 1
    }

    return { changed, signalIds }
  }

  function patchTestedAtBySignal(testedAtBySignal: Record<number, string> | Record<string, string>): SignalRuntimeStatePatchResult {
    return patchStates(Object.entries(testedAtBySignal ?? {}).map(([rawSignalId, testedAt]) => ({
      signalId: Number(rawSignalId),
      state: { testedAt },
    })))
  }

  function clear() {
    if (statesBySignalId.size === 0) {
      return
    }
    statesBySignalId.clear()
    version += 1
  }

  return {
    get version() {
      return version
    },
    get size() {
      return statesBySignalId.size
    },
    patchStates,
    patchTestedAtBySignal,
    getState: (signalId: number) => statesBySignalId.get(signalId) ?? null,
    getTestedAt: (signalId: number | null | undefined) => {
      const normalizedSignalId = normalizeSignalId(signalId)
      if (normalizedSignalId === null) {
        return null
      }
      return statesBySignalId.get(normalizedSignalId)?.testedAt ?? null
    },
    clear,
  }
}
