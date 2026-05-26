import { afterEach, beforeEach, describe, expect, it, vi } from "vitest"

import {
  createLocalSettingsStringStorage,
  LOCAL_SETTINGS_STORAGE_KEY,
  localSettingsKeys,
  readLocalSetting,
  writeLocalSetting,
} from "./localSettingsStorage"

class MemoryStorage implements Storage {
  private readonly items = new Map<string, string>()

  get length(): number {
    return this.items.size
  }

  clear(): void {
    this.items.clear()
  }

  getItem(key: string): string | null {
    return this.items.get(key) ?? null
  }

  key(index: number): string | null {
    return Array.from(this.items.keys())[index] ?? null
  }

  removeItem(key: string): void {
    this.items.delete(key)
  }

  setItem(key: string, value: string): void {
    this.items.set(key, value)
  }
}

function installStorage(): MemoryStorage {
  const storage = new MemoryStorage()
  vi.stubGlobal("window", { localStorage: storage })
  return storage
}

function readRoot(storage: Storage): Record<string, unknown> {
  const raw = storage.getItem(LOCAL_SETTINGS_STORAGE_KEY)
  expect(raw).toBeTruthy()
  return JSON.parse(raw ?? "{}") as Record<string, unknown>
}

describe("localSettingsStorage", () => {
  let storage: MemoryStorage

  beforeEach(() => {
    storage = installStorage()
  })

  afterEach(() => {
    vi.unstubAllGlobals()
  })

  it("stores values in one structured local settings document", () => {
    writeLocalSetting(localSettingsKeys.themeMode, "dark")

    const root = readRoot(storage)

    expect(root).toMatchObject({
      schema: "unitlab.local-settings",
      version: 1,
      values: {
        [localSettingsKeys.themeMode]: "dark",
      },
    })
  })

  it("reads and migrates legacy JSON keys into the structured document", () => {
    storage.setItem("unitlab.selection", JSON.stringify({
      lastDeviceId: 42,
      lastSwitchgearId: null,
      lastSequenceId: 7,
      lastSettingsRouteName: "settings.devices",
    }))

    const value = readLocalSetting<Record<string, unknown> | null>(localSettingsKeys.selection, null, {
      legacyKeys: ["unitlab.selection"],
      validate: item => item && typeof item === "object" ? item as Record<string, unknown> : null,
    })

    expect(value).toMatchObject({
      lastDeviceId: 42,
      lastSequenceId: 7,
      lastSettingsRouteName: "settings.devices",
    })
    expect(storage.getItem("unitlab.selection")).toBeNull()
    expect(readRoot(storage)).toMatchObject({
      values: {
        [localSettingsKeys.selection]: value,
      },
    })
  })

  it("exposes a string storage adapter while keeping localStorage structured", () => {
    const key = localSettingsKeys.signalsGridSavedView(2)
    const legacyKey = "unitlab.signals-grid:workspace:2"
    const savedView = JSON.stringify({ state: { columns: { order: ["tag"] } } })
    const stringStorage = createLocalSettingsStringStorage({
      resolveLegacyKeys: item => item === key ? [legacyKey] : [],
    })

    storage.setItem(legacyKey, savedView)

    expect(stringStorage.getItem(key)).toBe(savedView)
    expect(storage.getItem(legacyKey)).toBeNull()

    const nextSavedView = JSON.stringify({ state: { columns: { order: ["signal"] } } })
    stringStorage.setItem(key, nextSavedView)

    expect(storage.getItem(key)).toBeNull()
    expect(readRoot(storage)).toMatchObject({
      values: {
        [key]: nextSavedView,
      },
    })
  })

  it("falls back when the structured document is malformed", () => {
    storage.setItem(LOCAL_SETTINGS_STORAGE_KEY, "{")

    const value = readLocalSetting<"light" | "dark" | "auto">(localSettingsKeys.themeMode, "auto", {
      validate: item => item === "light" || item === "dark" || item === "auto" ? item : null,
    })

    expect(value).toBe("auto")
  })
})
