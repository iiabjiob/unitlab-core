const LOCAL_SETTINGS_SCHEMA = "unitlab.local-settings"
const LOCAL_SETTINGS_VERSION = 1

export const LOCAL_SETTINGS_STORAGE_KEY = "unitlab.local-settings.v1"

type LocalSettingsDocument = {
  schema: typeof LOCAL_SETTINGS_SCHEMA
  version: typeof LOCAL_SETTINGS_VERSION
  updatedAt: string
  values: Record<string, unknown>
}

type LocalSettingsStorageLike = Pick<Storage, "getItem" | "setItem" | "removeItem">

type LocalSettingOptions<T> = {
  legacyKeys?: readonly string[]
  parseLegacy?: (raw: string) => unknown
  validate?: (value: unknown) => T | null
}

type LocalSettingsStringStorageOptions = {
  resolveLegacyKeys?: (key: string) => readonly string[]
}

export const localSettingsKeys = {
  activeWorkspaceId: "workspace.activeWorkspaceId",
  devicesSidebarOnlineOnly: "devices.sidebar.onlineOnly",
  selection: "navigation.selection",
  switchgearsActiveView: "switchgears.activeView",
  themeMode: "theme.mode",
  uiChrome: "ui.chrome",
  dataGridColumnWidths: (tableId: string, datasetKey: string) =>
    `datagrid.${normalizeSettingsSegment(tableId)}.columnWidths.${normalizeSettingsSegment(datasetKey)}`,
  dataGridDatasetKey: (tableId: string) =>
    `datagrid.${normalizeSettingsSegment(tableId)}.datasetKey`,
  dataGridGlobalSelection: (tableId: string) =>
    `datagrid.${normalizeSettingsSegment(tableId)}.selection.global`,
  dataGridSelection: (tableId: string, datasetKey: string) =>
    `datagrid.${normalizeSettingsSegment(tableId)}.selection.${normalizeSettingsSegment(datasetKey)}`,
  resizablePanelSize: (storageKey: string) =>
    `ui.resizablePanels.${normalizeSettingsSegment(storageKey)}`,
  signalExportPresets: (workspaceId: number | null | undefined) =>
    `signals.exportPresets.workspace.${normalizeSettingsSegment(workspaceId ?? "none")}`,
  signalsGridSavedView: (workspaceId: number) =>
    `signals.grid.savedView.workspace.${normalizeSettingsSegment(workspaceId)}`,
  switchgearDiagram: (workspaceId: number) =>
    `switchgears.sld.workspace.${normalizeSettingsSegment(workspaceId)}`,
} as const

export function normalizeSettingsSegment(value: string | number): string {
  const segment = String(value).trim().replace(/[^a-zA-Z0-9_-]+/g, "-")
  return segment || "default"
}

export function readLocalSetting<T>(
  key: string,
  fallback: T,
  options: LocalSettingOptions<T> = {},
): T {
  const storage = resolveLocalStorage()
  if (!storage) {
    return fallback
  }

  const document = readDocument(storage)
  if (Object.prototype.hasOwnProperty.call(document.values, key)) {
    const stored = document.values[key]
    const normalized = normalizeStoredValue(stored, options.validate)
    if (normalized.valid) {
      return normalized.value
    }
  }

  const migrated = readLegacySetting(storage, key, options)
  return migrated.valid ? migrated.value : fallback
}

export function writeLocalSetting<T>(
  key: string,
  value: T,
  options: Pick<LocalSettingOptions<T>, "legacyKeys"> = {},
): boolean {
  const storage = resolveLocalStorage()
  if (!storage) {
    return false
  }

  const document = readDocument(storage)
  document.values[key] = value
  document.updatedAt = new Date().toISOString()

  if (!writeDocument(storage, document)) {
    return false
  }

  removeLegacyKeys(storage, options.legacyKeys)
  return true
}

export function removeLocalSetting(
  key: string,
  options: Pick<LocalSettingOptions<unknown>, "legacyKeys"> = {},
): boolean {
  const storage = resolveLocalStorage()
  if (!storage) {
    return false
  }

  const document = readDocument(storage)
  delete document.values[key]
  document.updatedAt = new Date().toISOString()

  if (!writeDocument(storage, document)) {
    return false
  }

  removeLegacyKeys(storage, options.legacyKeys)
  return true
}

export function readStringLocalSetting(
  key: string,
  fallback: string | null,
  options: Omit<LocalSettingOptions<string>, "validate"> = {},
): string | null {
  return readLocalSetting<string | null>(key, fallback, {
    ...options,
    validate: value => typeof value === "string" ? value : null,
  })
}

export function readNumberLocalSetting(
  key: string,
  fallback: number | null,
  options: Omit<LocalSettingOptions<number>, "validate"> = {},
): number | null {
  return readLocalSetting<number | null>(key, fallback, {
    ...options,
    validate: value => {
      if (value === null || value === undefined || value === "") {
        return null
      }
      const numberValue = Number(value)
      return Number.isFinite(numberValue) ? numberValue : null
    },
  })
}

export function readBooleanLocalSetting(
  key: string,
  fallback: boolean,
  options: Omit<LocalSettingOptions<boolean>, "validate"> = {},
): boolean {
  return readLocalSetting<boolean>(key, fallback, {
    ...options,
    validate: value => typeof value === "boolean" ? value : null,
  })
}

export function createLocalSettingsStringStorage(
  options: LocalSettingsStringStorageOptions = {},
): LocalSettingsStorageLike {
  return {
    getItem(key: string) {
      return readStringLocalSetting(key, null, {
        legacyKeys: options.resolveLegacyKeys?.(key) ?? [],
        parseLegacy: raw => raw,
      })
    },
    setItem(key: string, value: string) {
      writeLocalSetting(key, value, {
        legacyKeys: options.resolveLegacyKeys?.(key) ?? [],
      })
    },
    removeItem(key: string) {
      removeLocalSetting(key, {
        legacyKeys: options.resolveLegacyKeys?.(key) ?? [],
      })
    },
  }
}

export function parseLegacyJson(raw: string): unknown {
  try {
    return JSON.parse(raw)
  } catch {
    return raw
  }
}

function normalizeStoredValue<T>(
  value: unknown,
  validate?: (value: unknown) => T | null,
): { valid: true; value: T } | { valid: false } {
  if (!validate) {
    return { valid: true, value: value as T }
  }

  const normalized = validate(value)
  return normalized === null
    ? { valid: false }
    : { valid: true, value: normalized }
}

function readLegacySetting<T>(
  storage: LocalSettingsStorageLike,
  key: string,
  options: LocalSettingOptions<T>,
): { valid: true; value: T } | { valid: false } {
  for (const legacyKey of options.legacyKeys ?? []) {
    if (!legacyKey || legacyKey === LOCAL_SETTINGS_STORAGE_KEY) {
      continue
    }

    let raw: string | null = null
    try {
      raw = storage.getItem(legacyKey)
    } catch {
      raw = null
    }
    if (raw === null) {
      continue
    }

    const parsed = options.parseLegacy ? options.parseLegacy(raw) : parseLegacyJson(raw)
    const normalized = normalizeStoredValue(parsed, options.validate)
    if (!normalized.valid) {
      continue
    }

    writeLocalSetting(key, normalized.value, { legacyKeys: [legacyKey] })
    return normalized
  }

  return { valid: false }
}

function resolveLocalStorage(): LocalSettingsStorageLike | null {
  if (typeof window === "undefined") {
    return null
  }

  try {
    return window.localStorage
  } catch {
    return null
  }
}

function readDocument(storage: LocalSettingsStorageLike): LocalSettingsDocument {
  let raw: string | null = null
  try {
    raw = storage.getItem(LOCAL_SETTINGS_STORAGE_KEY)
  } catch {
    raw = null
  }

  if (!raw) {
    return createEmptyDocument()
  }

  try {
    const parsed = JSON.parse(raw)
    if (!isRecord(parsed) || parsed.schema !== LOCAL_SETTINGS_SCHEMA || !isRecord(parsed.values)) {
      return createEmptyDocument()
    }

    return {
      schema: LOCAL_SETTINGS_SCHEMA,
      version: LOCAL_SETTINGS_VERSION,
      updatedAt: typeof parsed.updatedAt === "string" ? parsed.updatedAt : "",
      values: { ...parsed.values },
    }
  } catch {
    return createEmptyDocument()
  }
}

function writeDocument(storage: LocalSettingsStorageLike, document: LocalSettingsDocument): boolean {
  try {
    storage.setItem(LOCAL_SETTINGS_STORAGE_KEY, JSON.stringify(document))
    return true
  } catch {
    return false
  }
}

function createEmptyDocument(): LocalSettingsDocument {
  return {
    schema: LOCAL_SETTINGS_SCHEMA,
    version: LOCAL_SETTINGS_VERSION,
    updatedAt: "",
    values: {},
  }
}

function removeLegacyKeys(storage: LocalSettingsStorageLike, legacyKeys: readonly string[] | undefined) {
  for (const legacyKey of legacyKeys ?? []) {
    if (!legacyKey || legacyKey === LOCAL_SETTINGS_STORAGE_KEY) {
      continue
    }
    try {
      storage.removeItem(legacyKey)
    } catch {
      // Ignore legacy cleanup failures.
    }
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value)
}
