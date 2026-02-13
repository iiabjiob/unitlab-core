import { defineStore } from "pinia"

export const useRealtimeScopeStore = defineStore("realtimeScopeStore", () => {
  const unitScopes = new Map<string, Set<string>>()
  const trackedUnits = new Map<string, number>()
  const globalStateScopes = new Set<string>()

  function setRealtimeUnitScope(scopeId: string, unitIds: string[]) {
    const normalized = new Set(
      unitIds
        .map(unitId => unitId.trim())
        .filter(unitId => unitId.length > 0),
    )
    const previous = unitScopes.get(scopeId) ?? new Set<string>()

    for (const unitId of previous) {
      if (normalized.has(unitId)) {
        continue
      }
      const next = (trackedUnits.get(unitId) ?? 0) - 1
      if (next <= 0) {
        trackedUnits.delete(unitId)
      } else {
        trackedUnits.set(unitId, next)
      }
    }

    for (const unitId of normalized) {
      if (previous.has(unitId)) {
        continue
      }
      trackedUnits.set(unitId, (trackedUnits.get(unitId) ?? 0) + 1)
    }

    if (normalized.size === 0) {
      unitScopes.delete(scopeId)
      return
    }
    unitScopes.set(scopeId, normalized)
  }

  function clearRealtimeUnitScope(scopeId: string) {
    setRealtimeUnitScope(scopeId, [])
  }

  function setGlobalRealtimeScope(scopeId: string, enabled: boolean) {
    if (enabled) {
      globalStateScopes.add(scopeId)
      return
    }
    globalStateScopes.delete(scopeId)
  }

  function shouldProcessRealtimeForUnit(unitId: string): boolean {
    if (globalStateScopes.size > 0) {
      return true
    }
    return (trackedUnits.get(unitId) ?? 0) > 0
  }

  function resetScopes() {
    unitScopes.clear()
    trackedUnits.clear()
    globalStateScopes.clear()
  }

  return {
    setRealtimeUnitScope,
    clearRealtimeUnitScope,
    setGlobalRealtimeScope,
    shouldProcessRealtimeForUnit,
    resetScopes,
  }
})
