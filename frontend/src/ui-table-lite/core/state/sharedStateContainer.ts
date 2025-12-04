import type { WritableSignal } from "@/ui-table/core/runtime/signals"

export type SharedStateListener<State extends Record<string, any>> = (
  state: State,
  changedKeys: readonly (keyof State)[],
) => void

export type SharedStateKeyListener<State extends Record<string, any>, K extends keyof State> = (
  value: State[K],
  previous: State[K],
) => void

export interface SharedStateContainer<State extends Record<string, any>> {
  readonly state: State
  getSnapshot(): State
  patch(partial: Partial<State>): void
  update(updater: (draft: State) => void): void
  getSignal<K extends keyof State>(key: K): WritableSignal<State[K]>
  subscribe(listener: SharedStateListener<State>): () => void
  subscribeKey<K extends keyof State>(key: K, listener: SharedStateKeyListener<State, K>): () => void
  dispose(): void
}

export interface CreateSharedStateContainerOptions<State extends Record<string, any>> {
  equals?: (current: State[keyof State], next: State[keyof State]) => boolean
  freezeSnapshot?: boolean
}

function defaultEquality<State extends Record<string, any>>(
  current: State[keyof State],
  next: State[keyof State],
): boolean {
  return Object.is(current, next)
}

export function createSharedStateContainer<State extends Record<string, any>>(
  initialState: State,
  options: CreateSharedStateContainerOptions<State> = {},
): SharedStateContainer<State> {
  type Key = keyof State

  const equality = options.equals ?? defaultEquality<State>
  const listeners = new Set<SharedStateListener<State>>()
  const keyListeners = new Map<Key, Set<SharedStateKeyListener<State, Key>>>()
  const signals = new Map<Key, WritableSignal<State[Key]>>()
  const definedKeys = new Set<Key>()

  let disposed = false
  let current = { ...initialState }

  const stateView = Object.create(null) as State

  const ensureActive = () => {
    if (disposed) {
      throw new Error("SharedStateContainer has been disposed")
    }
  }

  const createSignalForKey = <K extends Key>(key: K): WritableSignal<State[K]> => {
    const signal = {} as WritableSignal<State[K]>
    Object.defineProperty(signal, "value", {
      get: () => current[key],
      set: value => {
        ensureActive()
        patch({ [key]: value } as Partial<State>)
      },
      enumerable: true,
      configurable: true,
    })
    return signal
  }

  const ensureKey = (key: Key) => {
    if (definedKeys.has(key)) return
    definedKeys.add(key)

    Object.defineProperty(stateView, key, {
      get: () => current[key],
      enumerable: true,
      configurable: false,
    })

    if (!signals.has(key)) {
      signals.set(key, createSignalForKey(key))
    }
  }

  const emit = (changedKeys: Key[], previousValues: State[Key][]) => {
    if (!changedKeys.length) return
    const frozenSnapshot = options.freezeSnapshot ? (Object.freeze({ ...current }) as State) : undefined

    for (let index = 0; index < changedKeys.length; index += 1) {
      const key = changedKeys[index]
      const prev = previousValues[index]
      const next = current[key]
      const subscribers = keyListeners.get(key)
      if (subscribers) {
        for (const listener of subscribers) {
          listener(next, prev)
        }
      }
    }

    if (!listeners.size) return

    const statePayload = frozenSnapshot ?? stateView
    for (const listener of listeners) {
      listener(statePayload, changedKeys)
    }
  }

  const patch = (partial: Partial<State>): void => {
    ensureActive()
    if (!partial) return
    const keys = Object.keys(partial) as Key[]
    if (!keys.length) return

    const changedKeys: Key[] = []
    const previousValues: State[Key][] = []
    let nextState = current

    for (const key of keys) {
      ensureKey(key)
      const nextValue = partial[key] as State[Key]
      const previousValue = current[key]
      if (equality(previousValue, nextValue)) continue
      if (nextState === current) {
        nextState = { ...current }
      }
      nextState[key] = nextValue
      changedKeys.push(key)
      previousValues.push(previousValue)
    }

    if (changedKeys.length) {
      current = nextState
    }

    emit(changedKeys, previousValues)
  }

  const getSignal = <K extends Key>(key: K): WritableSignal<State[K]> => {
    ensureActive()
    ensureKey(key)
    let existing = signals.get(key) as WritableSignal<State[K]> | undefined
    if (!existing) {
      existing = createSignalForKey(key)
      signals.set(key, existing as WritableSignal<State[Key]>)
    }
    return existing
  }

  const subscribe = (listener: SharedStateListener<State>) => {
    ensureActive()
    listeners.add(listener)
    return () => {
      listeners.delete(listener)
    }
  }

  const subscribeKey = <K extends Key>(key: K, listener: SharedStateKeyListener<State, K>) => {
    ensureActive()
    ensureKey(key)
    let bucket = keyListeners.get(key)
    if (!bucket) {
      bucket = new Set()
      keyListeners.set(key, bucket)
    }
    bucket.add(listener as SharedStateKeyListener<State, Key>)
    return () => {
      const existing = keyListeners.get(key)
      existing?.delete(listener as SharedStateKeyListener<State, Key>)
      if (existing && existing.size === 0) {
        keyListeners.delete(key)
      }
    }
  }

  const update = (updater: (draft: State) => void) => {
    ensureActive()
    if (typeof updater !== "function") return
    const draft = { ...current }
    updater(draft)
    patch(draft)
  }

  const getSnapshot = (): State => {
    ensureActive()
    const snapshot = { ...current } as State
    if (options.freezeSnapshot) {
      return Object.freeze(snapshot) as State
    }
    return snapshot
  }

  const dispose = () => {
    if (disposed) return
    disposed = true
    listeners.clear()
    keyListeners.clear()
    signals.clear()
  }

  for (const key of Object.keys(initialState) as Key[]) {
    ensureKey(key)
  }

  return {
    get state() {
      return stateView
    },
    getSnapshot,
    patch,
    update,
    getSignal,
    subscribe,
    subscribeKey,
    dispose,
  }
}
