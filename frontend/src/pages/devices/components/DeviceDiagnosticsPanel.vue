<script setup lang="ts">
import { computed, h, nextTick, onBeforeUnmount, ref, watch, type ComponentPublicInstance } from "vue"
import { defineDataGridComponent, type DataGridAppColumnInput, type DataGridProps } from "@affino/datagrid-vue-app"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"
import type { Device } from "@/types/device"
import UiAffinoDisclosure from "@/components/ui/UiAffinoDisclosure.vue"
import InlineInfoTooltip from "@/components/ui/InlineInfoTooltip.vue"
import { useAffinoDataGridTheme } from "@/components/ui/affinoDataGridTheme"
import "@/components/ui/affinoDataGridNative.css"

const props = defineProps<{ device: Device }>()
const { theme } = useAffinoDataGridTheme()
const MEM_FREE_LOW_THRESHOLD = 150000
const FAST_STALE_THRESHOLD_MS = 15_000
const DIAG_STALE_THRESHOLD_MS = 180_000

type JsonRecord = Record<string, unknown>
type DiagTreeValue = string

type DiagTreeRow = {
  value: DiagTreeValue
  parent: DiagTreeValue | null
  label: string
  valueLabel: string | null
  isLeaf: boolean
}

function asRecord(value: unknown): JsonRecord | null {
  return value && typeof value === "object" && !Array.isArray(value)
    ? (value as JsonRecord)
    : null
}

function asNumber(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null
}

function asString(value: unknown): string | null {
  return typeof value === "string" ? value : null
}

const fast = computed<JsonRecord | null>(() => asRecord(props.device.heartbeat_fast ?? null))
const diag = computed<JsonRecord | null>(() => asRecord(props.device.heartbeat_diag ?? null))

const fastMem = computed(() => asRecord(fast.value?.mem))
const diagMem = computed(() => asRecord(diag.value?.mem))
const diagMemStab = computed(() => asRecord(diag.value?.mem_stab))
const diagAlloc = computed(() => asRecord(diag.value?.alloc))
const diagMqttIn = computed(() => asRecord(diag.value?.mqtt_in))
const diagReset = computed(() => asRecord(diag.value?.reset))
const diagTasks = computed(() => asRecord(diag.value?.tasks))
const diagStack = computed(() => Array.isArray(diag.value?.stack) ? diag.value?.stack : [])

const fastSeq = computed(() => asNumber(fast.value?.seq))
const diagSeq = computed(() => asNumber(diag.value?.seq))
const stateRaw = computed(() => asNumber((fast.value?.state ?? diag.value?.state) ?? null))
const mqttRaw = computed(() => asNumber((fast.value?.mqtt ?? diag.value?.mqtt) ?? null))
const uptimeSeconds = computed(() => asNumber((fast.value?.up_s ?? diag.value?.up_s) ?? null))
const lastComponent = computed(() => asString((fast.value?.last_comp ?? diag.value?.last_comp) ?? null))
const lastReason = computed(() => asString((fast.value?.last_reason ?? diag.value?.last_reason) ?? null))
const staleTasks = computed(() => asNumber(diagTasks.value?.stale))
const faults = computed(() => asNumber((fast.value?.faults ?? diag.value?.faults) ?? null))
const degraded = computed(() => asNumber((fast.value?.degraded ?? diag.value?.degraded) ?? null))
const mqttOverflow = computed(() => asNumber(diagMqttIn.value?.ovf))
const mqttRateLimited = computed(() => asNumber(diagMqttIn.value?.rlim))
const memFree = computed(() => asNumber((diagMem.value?.free ?? fastMem.value?.free) ?? null))
const memStabilityRows = computed(() => {
  const ready = asNumber(diagMemStab.value?.ready)
  return [
    { key: "baseline", label: "Baseline", value: ready === 1 ? "captured" : "warming" },
    { key: "base_ms", label: "Baseline ms", value: asNumber(diagMemStab.value?.base_ms) ?? '—' },
    { key: "base_free", label: "Base free", value: asNumber(diagMemStab.value?.base_free) ?? '—' },
    { key: "base_largest", label: "Base largest", value: asNumber(diagMemStab.value?.base_largest) ?? '—' },
    { key: "max_d_free", label: "Max drift free", value: asNumber(diagMemStab.value?.max_d_free) ?? '—' },
    { key: "max_d_largest", label: "Max drift largest", value: asNumber(diagMemStab.value?.max_d_largest) ?? '—' },
    { key: "chg", label: "Changes", value: asNumber(diagMemStab.value?.chg) ?? '—' },
    { key: "samples", label: "Samples", value: asNumber(diagMemStab.value?.samples) ?? '—' },
  ]
})
const allocRows = computed(() => [
  { key: "boot_done", label: "Boot complete", value: asNumber(diagAlloc.value?.boot_done) === 1 ? "yes" : (diagAlloc.value ? "no" : "—") },
  { key: "boot_ms", label: "Boot ms", value: asNumber(diagAlloc.value?.boot_ms) ?? '—' },
  { key: "boot_new", label: "Boot new", value: asNumber(diagAlloc.value?.boot_new) ?? '—' },
  { key: "boot_del", label: "Boot delete", value: asNumber(diagAlloc.value?.boot_del) ?? '—' },
  { key: "run_new", label: "Run new", value: asNumber(diagAlloc.value?.run_new) ?? '—' },
  { key: "run_del", label: "Run delete", value: asNumber(diagAlloc.value?.run_del) ?? '—' },
  { key: "new_fail", label: "New fail", value: asNumber(diagAlloc.value?.new_fail) ?? '—' },
])

const keyAlertTooltipByKey: Record<string, string> = {
  faults: "Firmware-reported active fault count.",
  degraded: "Non-fatal degraded mode indicator count.",
  ovf: "Inbound MQTT queue overflow counter (dropped due to queue full).",
  rlim: "Messages dropped by ingress rate limiter.",
  stale: "Tasks that missed expected heartbeat/window.",
  mem: "Current free heap words/bytes from telemetry snapshot.",
}

const freshnessTooltipByKey: Record<string, string> = {
  fast: "Age since last fast heartbeat (/h) seen by frontend.",
  diag: "Age since last diagnostic heartbeat (/hd) seen by frontend.",
}

const summaryTooltipByKey: Record<string, string> = {
  State: "Normalized firmware runtime state.",
  MQTT: "Normalized MQTT connectivity status.",
  "Fast seq": "Sequence number of fast heartbeat packets.",
  "Diag seq": "Sequence number of diagnostic heartbeat packets.",
  Uptime: "Firmware uptime from heartbeat payload.",
  "Last component": "Last component that reported status/reason.",
  "Last reason": "Last reported reason code/label from firmware.",
  "Diag stale tasks": "Count of stale tasks from diag.tasks.stale.",
}

const memStabilityTooltipByKey: Record<string, string> = {
  baseline: "Whether baseline memory snapshot is captured.",
  base_ms: "Timestamp/ms when baseline was captured.",
  base_free: "Baseline free memory value.",
  base_largest: "Baseline largest free block.",
  max_d_free: "Maximum observed drift of free memory from baseline.",
  max_d_largest: "Maximum observed drift of largest free block from baseline.",
  chg: "Detected memory change events count.",
  samples: "Number of samples used in stability estimate.",
}

const allocTooltipByKey: Record<string, string> = {
  boot_done: "Allocator boot initialization completed.",
  boot_ms: "Allocator boot initialization duration in ms.",
  boot_new: "new() calls during boot phase.",
  boot_del: "delete() calls during boot phase.",
  run_new: "new() calls during runtime phase.",
  run_del: "delete() calls during runtime phase.",
  new_fail: "Failed new() allocations count.",
}

function tooltipFromMap(map: Record<string, string>, key: string): string {
  return map[key] ?? ""
}

const saveButtonText = ref("Save JSON")
let saveFeedbackTimeout: number | null = null
const nowMs = ref(Date.now())
let nowTickerId: number | null = null
const fastReceivedAtMs = ref<number | null>(props.device.last_seen ?? null)
const diagReceivedAtMs = ref<number | null>(props.device.last_seen ?? null)

watch(
  () => fast.value,
  (next, prev) => {
    if (next && next !== prev) {
      fastReceivedAtMs.value = Date.now()
    }
  }
)

watch(
  () => diag.value,
  (next, prev) => {
    if (next && next !== prev) {
      diagReceivedAtMs.value = Date.now()
    }
  }
)

if (typeof window !== "undefined") {
  nowTickerId = window.setInterval(() => {
    nowMs.value = Date.now()
  }, 1000)
}

function formatUptime(value: number | null): string {
  if (value == null || value < 0) return "—"
  const total = Math.floor(value)
  const h = Math.floor(total / 3600)
  const m = Math.floor((total % 3600) / 60)
  const s = total % 60
  if (h > 0) return `${h}h ${m}m ${s}s`
  if (m > 0) return `${m}m ${s}s`
  return `${s}s`
}

function formatAge(ms: number | null): string {
  if (ms == null) return "—"
  const age = Math.max(0, Math.floor(ms / 1000))
  if (age < 60) return `${age}s`
  const min = Math.floor(age / 60)
  const sec = age % 60
  if (min < 60) return `${min}m ${sec}s`
  const h = Math.floor(min / 60)
  const remMin = min % 60
  return `${h}h ${remMin}m`
}

function normalizeState(value: number | null): string {
  if (value == null) return "—"
  if (value === 1) return "RUNNING"
  if (value === 0) return "STOPPED"
  return `STATE_${value}`
}

function normalizeMqtt(value: number | null): string {
  if (value == null) return "—"
  return value === 1 ? "connected" : "disconnected"
}

function asPositiveNumber(value: number | null): number {
  return value != null && value > 0 ? value : 0
}

function isFreshnessStale(ageMs: number | null, thresholdMs: number): boolean {
  if (ageMs == null) return true
  return ageMs > thresholdMs
}

const fastAgeMs = computed(() => {
  if (fastReceivedAtMs.value == null) return null
  return Math.max(0, nowMs.value - fastReceivedAtMs.value)
})

const diagAgeMs = computed(() => {
  if (diagReceivedAtMs.value == null) return null
  return Math.max(0, nowMs.value - diagReceivedAtMs.value)
})

const summaryRows = computed(() => [
  { key: 'State', value: normalizeState(stateRaw.value) },
  { key: 'MQTT', value: normalizeMqtt(mqttRaw.value) },
  { key: 'Fast seq', value: fastSeq.value ?? '—' },
  { key: 'Diag seq', value: diagSeq.value ?? '—' },
  { key: 'Uptime', value: formatUptime(uptimeSeconds.value) },
  { key: 'Last component', value: lastComponent.value ?? '—' },
  { key: 'Last reason', value: lastReason.value ?? '—' },
  { key: 'Diag stale tasks', value: staleTasks.value ?? '—' },
])

const keyAlerts = computed(() => {
  const faultsCount = asPositiveNumber(faults.value)
  const degradedCount = asPositiveNumber(degraded.value)
  const overflowCount = asPositiveNumber(mqttOverflow.value)
  const rateLimitDrops = asPositiveNumber(mqttRateLimited.value)
  const staleCount = asPositiveNumber(staleTasks.value)
  const free = memFree.value
  const memLow = free != null && free < MEM_FREE_LOW_THRESHOLD

  return [
    { key: "faults", label: "Faults", value: faultsCount, isAlert: faultsCount > 0 },
    { key: "degraded", label: "Degraded", value: degradedCount, isAlert: degradedCount > 0 },
    { key: "ovf", label: "MQTT overflow", value: overflowCount, isAlert: overflowCount > 0 },
    { key: "rlim", label: "MQTT rate drops", value: rateLimitDrops, isAlert: rateLimitDrops > 0 },
    { key: "stale", label: "Stale tasks", value: staleCount, isAlert: staleCount > 0 },
    {
      key: "mem",
      label: "Mem free",
      value: free == null ? "—" : free,
      isAlert: memLow,
      hint: free == null ? "" : memLow ? "low" : "ok",
    },
  ]
})

const freshnessRows = computed(() => [
  {
    key: "fast",
    label: "Fast age",
    value: formatAge(fastAgeMs.value),
    stale: isFreshnessStale(fastAgeMs.value, FAST_STALE_THRESHOLD_MS),
  },
  {
    key: "diag",
    label: "Diag age",
    value: formatAge(diagAgeMs.value),
    stale: isFreshnessStale(diagAgeMs.value, DIAG_STALE_THRESHOLD_MS),
  },
])

type StackRow = {
  rowId: string
  task: string
  minWords: number | null
  lastSeenMs: number | null
}

const DataGrid = defineDataGridComponent<StackRow>()

const stackRows = computed<StackRow[]>(() =>
  diagStack.value.map((entry, index) => {
    const row = asRecord(entry)
    return {
      rowId: `stack-${index}-${String(row?.task ?? "unknown")}`,
      task: asString(row?.task) ?? "—",
      minWords: asNumber(row?.min_words),
      lastSeenMs: asNumber(row?.last_seen_ms),
    }
  })
)

const stackGridColumns = computed<DataGridAppColumnInput<StackRow>[]>(() => [
  {
    key: "task",
    label: "task",
    flex: 1,
    minWidth: 180,
    initialState: { width: 220 },
    capabilities: { editable: false },
    cellRenderer: ({ row }) => h("span", { class: "font-mono text-[11px]" }, String(row?.task ?? "—")),
  },
  {
    key: "minWords",
    label: "min_words",
    minWidth: 120,
    initialState: { width: 140 },
    capabilities: { editable: false },
    presentation: { align: "right", headerAlign: "right" },
    cellRenderer: ({ row }) => h("span", { class: "font-mono text-[11px]" }, String(row?.minWords ?? "—")),
  },
  {
    key: "lastSeenMs",
    label: "last_seen_ms",
    minWidth: 140,
    initialState: { width: 160 },
    capabilities: { editable: false },
    presentation: { align: "right", headerAlign: "right" },
    cellRenderer: ({ row }) => h("span", { class: "font-mono text-[11px]" }, String(row?.lastSeenMs ?? "—")),
  },
])

const stackGridRowModelOptions: NonNullable<DataGridProps<StackRow>["clientRowModelOptions"]> = {
  resolveRowId: (row: StackRow) => row.rowId,
}

const diagnosticsPayload = computed(() => {
  const nowIso = new Date().toISOString()
  return {
    exported_at: nowIso,
    device: {
      id: props.device.id,
      unit_id: props.device.unit_id,
      display_name: props.device.display_name,
      device_type: props.device.device_type,
      online: props.device.online,
    },
    heartbeat_fast: fast.value,
    heartbeat_diag: diag.value,
    diagnostics_view: {
      summary: summaryRows.value,
      key_alerts: keyAlerts.value,
      freshness: freshnessRows.value,
      mem_stability: memStabilityRows.value,
      allocation_counters: allocRows.value,
      stack: stackRows.value,
      reset: {
        code: asNumber(diagReset.value?.code),
        label: asString(diagReset.value?.label),
        boot: asNumber(diagReset.value?.boot),
      },
      raw_sections: {
        mem: diagMem.value,
        mem_stab: diagMemStab.value,
        alloc: diagAlloc.value,
        mqtt_in: diagMqttIn.value,
        tasks: diagTasks.value,
        reset: diagReset.value,
        stack: diagStack.value,
      },
      thresholds: {
        mem_free_low: MEM_FREE_LOW_THRESHOLD,
        fast_stale_ms: FAST_STALE_THRESHOLD_MS,
        diag_stale_ms: DIAG_STALE_THRESHOLD_MS,
      },
      exported_from_ui_at_ms: Date.now(),
    },
  }
})

const hasDiagnostics = computed(() => Boolean(fast.value || diag.value))

function setSaveButtonFeedback(text: string, durationMs = 1500): void {
  saveButtonText.value = text
  if (saveFeedbackTimeout !== null) {
    window.clearTimeout(saveFeedbackTimeout)
  }
  saveFeedbackTimeout = window.setTimeout(() => {
    saveButtonText.value = "Save JSON"
    saveFeedbackTimeout = null
  }, durationMs)
}

function toSafeFilePart(value: string): string {
  return value.replace(/[^a-zA-Z0-9_-]/g, "_")
}

function formatPrimitive(value: unknown): string {
  if (value == null) return "—"
  if (typeof value === "boolean") return value ? "true" : "false"
  if (typeof value === "number") return Number.isFinite(value) ? String(value) : "—"
  if (typeof value === "string") return value
  return String(value)
}

function buildTreeRows(rootLabel: string, rootValue: unknown, rootKey: string): DiagTreeRow[] {
  const rows: DiagTreeRow[] = []

  const pushRow = (
    value: DiagTreeValue,
    parent: DiagTreeValue | null,
    label: string,
    valueLabel: string | null,
    isLeaf: boolean,
  ) => {
    rows.push({ value, parent, label, valueLabel, isLeaf })
  }

  const walk = (value: unknown, parent: DiagTreeValue | null, key: string, label: string) => {
    if (Array.isArray(value)) {
      pushRow(key, parent, label, value.length === 0 ? "[]" : null, value.length === 0)
      value.forEach((item, index) => {
        walk(item, key, `${key}[${index}]`, `[${index}]`)
      })
      return
    }

    const record = asRecord(value)
    if (record) {
      const entries = Object.entries(record)
      pushRow(key, parent, label, entries.length === 0 ? "{}" : null, entries.length === 0)
      entries.forEach(([childKey, childValue]) => {
        walk(childValue, key, `${key}.${childKey}`, childKey)
      })
      return
    }

    pushRow(key, parent, label, formatPrimitive(value), true)
  }

  walk(rootValue, null, rootKey, rootLabel)
  return rows
}

const telemetryTreeRows = computed<DiagTreeRow[]>(() => [
  ...buildTreeRows("fast (/h)", fast.value, "fast"),
  ...buildTreeRows("diag (/hd)", diag.value, "diag"),
])

const telemetryTreeNodes = computed<TreeviewNode<DiagTreeValue>[]>(() =>
  telemetryTreeRows.value.map((row) => ({ value: row.value, parent: row.parent }))
)

const telemetryNodeMeta = computed(() => {
  const map = new Map<DiagTreeValue, DiagTreeRow>()
  telemetryTreeRows.value.forEach((row) => map.set(row.value, row))
  return map
})

const telemetryTree = useTreeviewController<DiagTreeValue>({
  nodes: [],
  loop: true,
})
const telemetryDefaultExpansionApplied = ref(false)

watch(
  telemetryTreeNodes,
  (nodes) => {
    telemetryTree.registerNodes(nodes)
    if (!telemetryDefaultExpansionApplied.value && nodes.length > 0) {
      if (nodes.some((node) => node.value === "fast")) {
        telemetryTree.expand("fast")
      }
      if (nodes.some((node) => node.value === "diag")) {
        telemetryTree.expand("diag")
      }
      telemetryDefaultExpansionApplied.value = true
    }
  },
  { immediate: true }
)

const telemetryParentByValue = computed(() => {
  const map = new Map<DiagTreeValue, DiagTreeValue | null>()
  telemetryTreeNodes.value.forEach((node) => map.set(node.value, node.parent))
  return map
})

const telemetryExpandedSet = computed(() => new Set(telemetryTree.state.value.expanded))

const telemetryChildrenByParent = computed(() => {
  const map = new Map<DiagTreeValue | null, DiagTreeValue[]>()
  telemetryTreeNodes.value.forEach((node) => {
    const siblings = map.get(node.parent) ?? []
    siblings.push(node.value)
    map.set(node.parent, siblings)
  })
  return map
})

const telemetryItemElements = new Map<DiagTreeValue, HTMLButtonElement>()

function telemetryNodeLevel(value: DiagTreeValue): number {
  let level = 1
  let cursor = telemetryParentByValue.value.get(value) ?? null
  const visited = new Set<DiagTreeValue>()
  while (cursor && !visited.has(cursor)) {
    visited.add(cursor)
    level += 1
    cursor = telemetryParentByValue.value.get(cursor) ?? null
  }
  return level
}

function isTelemetryNodeVisible(value: DiagTreeValue): boolean {
  let cursor = telemetryParentByValue.value.get(value) ?? null
  while (cursor) {
    if (!telemetryExpandedSet.value.has(cursor)) return false
    cursor = telemetryParentByValue.value.get(cursor) ?? null
  }
  return true
}

const visibleTelemetryRows = computed(() =>
  telemetryTreeRows.value.filter((row) => isTelemetryNodeVisible(row.value))
)

function onTelemetryRowClick(value: DiagTreeValue): void {
  const meta = telemetryNodeMeta.value.get(value)
  telemetryTree.focus(value)
  if (!meta || meta.isLeaf) return
  telemetryTree.toggle(value)
}

function isTelemetryExpanded(value: DiagTreeValue): boolean {
  return telemetryTree.isExpanded(value)
}

function isTelemetryLeaf(value: DiagTreeValue): boolean {
  return telemetryNodeMeta.value.get(value)?.isLeaf ?? true
}

function bindTelemetryItemElement(value: DiagTreeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element
      ? element
      : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLButtonElement) {
      telemetryItemElements.set(value, resolved)
      return
    }
    telemetryItemElements.delete(value)
  }
}

function focusTelemetryNodeElement(value: DiagTreeValue): void {
  const element = telemetryItemElements.get(value)
  if (!element) return
  element.focus({ preventScroll: true })
  element.scrollIntoView({ block: "nearest" })
}

watch(
  () => telemetryTree.state.value.active,
  async (active) => {
    if (!active) return
    await nextTick()
    focusTelemetryNodeElement(active)
  }
)

function onTelemetryRowKeydown(event: KeyboardEvent, value: DiagTreeValue): void {
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      event.stopPropagation()
      telemetryTree.focusNext()
      return
    case "ArrowUp":
      event.preventDefault()
      event.stopPropagation()
      telemetryTree.focusPrevious()
      return
    case "Home":
      event.preventDefault()
      event.stopPropagation()
      telemetryTree.focusFirst()
      return
    case "End":
      event.preventDefault()
      event.stopPropagation()
      telemetryTree.focusLast()
      return
    case "ArrowRight":
      if (!isTelemetryLeaf(value)) {
        event.preventDefault()
        event.stopPropagation()
        if (!telemetryTree.isExpanded(value)) {
          telemetryTree.expand(value)
          return
        }
        const firstChild = telemetryChildrenByParent.value.get(value)?.[0]
        if (firstChild) {
          telemetryTree.focus(firstChild)
        }
      }
      return
    case "ArrowLeft":
      if (!isTelemetryLeaf(value) && telemetryTree.isExpanded(value)) {
        event.preventDefault()
        event.stopPropagation()
        telemetryTree.collapse(value)
        return
      }
      {
        const parent = telemetryParentByValue.value.get(value)
        if (parent) {
          event.preventDefault()
          event.stopPropagation()
          telemetryTree.focus(parent)
        }
      }
      return
    case "Enter":
    case " ":
      event.preventDefault()
      event.stopPropagation()
      onTelemetryRowClick(value)
      return
    default:
      return
  }
}

function onTelemetryTreeRootKeydown(event: KeyboardEvent): void {
  const active = telemetryTree.state.value.active
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      event.stopPropagation()
      if (active) {
        telemetryTree.focusNext()
      } else {
        telemetryTree.focusFirst()
      }
      return
    case "ArrowUp":
      event.preventDefault()
      event.stopPropagation()
      if (active) {
        telemetryTree.focusPrevious()
      } else {
        telemetryTree.focusLast()
      }
      return
    case "Home":
      event.preventDefault()
      event.stopPropagation()
      telemetryTree.focusFirst()
      return
    case "End":
      event.preventDefault()
      event.stopPropagation()
      telemetryTree.focusLast()
      return
    case "ArrowRight":
      if (!active || isTelemetryLeaf(active)) return
      event.preventDefault()
      event.stopPropagation()
      if (!telemetryTree.isExpanded(active)) {
        telemetryTree.expand(active)
        return
      }
      {
        const firstChild = telemetryChildrenByParent.value.get(active)?.[0]
        if (firstChild) {
          telemetryTree.focus(firstChild)
        }
      }
      return
    case "ArrowLeft":
      if (!active) return
      event.preventDefault()
      event.stopPropagation()
      if (!isTelemetryLeaf(active) && telemetryTree.isExpanded(active)) {
        telemetryTree.collapse(active)
        return
      }
      {
        const parent = telemetryParentByValue.value.get(active)
        if (parent) {
          telemetryTree.focus(parent)
        }
      }
      return
    case "Enter":
    case " ":
      if (!active) return
      event.preventDefault()
      event.stopPropagation()
      onTelemetryRowClick(active)
      return
    default:
      return
  }
}

function saveDiagnosticsJson(): void {
  if (!hasDiagnostics.value) {
    setSaveButtonFeedback("No data")
    return
  }

  try {
    const json = JSON.stringify(diagnosticsPayload.value, null, 2)
    const blob = new Blob([json], { type: "application/json;charset=utf-8" })
    const url = URL.createObjectURL(blob)
    const link = document.createElement("a")
    const safeUnit = toSafeFilePart(props.device.unit_id || "device")
    const stamp = new Date().toISOString().replace(/[:.]/g, "-")

    link.href = url
    link.download = `${safeUnit}_diagnostics_${stamp}.json`
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    URL.revokeObjectURL(url)
    setSaveButtonFeedback("Saved")
  } catch {
    setSaveButtonFeedback("Failed")
  }
}

onBeforeUnmount(() => {
  if (saveFeedbackTimeout !== null) {
    window.clearTimeout(saveFeedbackTimeout)
    saveFeedbackTimeout = null
  }
  if (nowTickerId !== null) {
    window.clearInterval(nowTickerId)
    nowTickerId = null
  }
})
</script>

<template>
  <div class="flex flex-col select-none lg:h-full lg:min-h-0">
    <div class="p-3 text-xs uppercase tracking-wider text-neutral-500 border-b dark:text-neutral-400 border-neutral-200 dark:border-neutral-800 flex items-center justify-between gap-3">
      <span>Device Diagnostics</span>
      <div class="flex items-center gap-2">
        <span class="text-[10px] normal-case tracking-normal text-neutral-400 dark:text-neutral-500">
          Source: heartbeat /hd
        </span>
        <button
          type="button"
          class="relative text-[10px] px-2 py-1 rounded border border-neutral-300 text-neutral-600 hover:bg-neutral-100 disabled:opacity-60 disabled:cursor-not-allowed dark:border-neutral-700 dark:text-neutral-300 dark:hover:bg-neutral-800/60 transition-colors"
          :disabled="!hasDiagnostics"
          @click="saveDiagnosticsJson"
        >
          <span class="invisible">Save JSON</span>
          <span class="absolute inset-0 flex items-center justify-center">
            {{ saveButtonText }}
          </span>
        </button>
      </div>
    </div>

    <div class="p-4 space-y-4 text-sm text-neutral-700 dark:text-neutral-200 lg:flex-1 lg:min-h-0 lg:overflow-y-auto">
      <div v-if="!fast && !diag" class="rounded-lg border border-dashed border-neutral-300 dark:border-neutral-700 px-4 py-3 text-sm italic text-neutral-500 dark:text-neutral-400">
        Waiting for heartbeat diagnostics. Firmware should publish fast <code>/h</code> and diagnostic <code>/hd</code> heartbeats.
      </div>

      <template v-else>
        <UiAffinoDisclosure title="Key alerts">
          <div class="grid gap-2 [grid-template-columns:repeat(auto-fit,minmax(180px,1fr))]">
            <div
              v-for="item in keyAlerts"
              :key="item.key"
              class="rounded-md border px-3 py-2"
              :class="item.isAlert
                ? 'border-red-300 bg-red-50 text-red-800 dark:border-red-800 dark:bg-red-950/30 dark:text-red-200'
                : 'border-neutral-200 bg-neutral-50 text-neutral-700 dark:border-neutral-700 dark:bg-neutral-900/60 dark:text-neutral-200'"
            >
              <div class="text-[10px] uppercase tracking-wide opacity-80 flex items-center gap-1">
                <span>{{ item.label }}</span>
                <InlineInfoTooltip
                  v-if="tooltipFromMap(keyAlertTooltipByKey, item.key)"
                  :text="tooltipFromMap(keyAlertTooltipByKey, item.key)"
                  placement="top"
                  align="start"
                />
              </div>
              <div class="mt-1 text-sm font-semibold break-all">{{ item.value }}</div>
              <div v-if="item.hint" class="text-[10px] mt-0.5 uppercase tracking-wide opacity-80">{{ item.hint }}</div>
            </div>
          </div>
        </UiAffinoDisclosure>

        <UiAffinoDisclosure title="Freshness">
          <div class="grid grid-cols-1 gap-2 sm:grid-cols-2">
            <div
              v-for="row in freshnessRows"
              :key="row.key"
              class="rounded-md border px-3 py-2"
              :class="row.stale
                ? 'border-amber-300 bg-amber-50 text-amber-900 dark:border-amber-800 dark:bg-amber-950/30 dark:text-amber-200'
                : 'border-neutral-200 bg-neutral-50 text-neutral-700 dark:border-neutral-700 dark:bg-neutral-900/60 dark:text-neutral-200'"
            >
              <div class="text-[10px] uppercase tracking-wide opacity-80 flex items-center gap-1">
                <span>{{ row.label }}</span>
                <InlineInfoTooltip
                  v-if="tooltipFromMap(freshnessTooltipByKey, row.key)"
                  :text="tooltipFromMap(freshnessTooltipByKey, row.key)"
                  placement="top"
                  align="start"
                />
              </div>
              <div class="mt-1 text-sm font-semibold">{{ row.value }}</div>
            </div>
          </div>
        </UiAffinoDisclosure>

        <UiAffinoDisclosure title="Summary">
          <div class="grid grid-cols-1 gap-2 sm:grid-cols-2">
          <div
            v-for="row in summaryRows"
            :key="row.key"
            class="rounded-md border border-neutral-200 bg-neutral-50 px-3 py-2 dark:border-neutral-700 dark:bg-neutral-900/60"
          >
            <div class="text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400 flex items-center gap-1">
              <span>{{ row.key }}</span>
              <InlineInfoTooltip
                v-if="tooltipFromMap(summaryTooltipByKey, String(row.key))"
                :text="tooltipFromMap(summaryTooltipByKey, String(row.key))"
                placement="top"
                align="start"
              />
            </div>
            <div class="mt-1 text-sm font-medium break-all">{{ row.value }}</div>
          </div>
          </div>
        </UiAffinoDisclosure>

        <UiAffinoDisclosure
          title="Telemetry tree"
          containerClass="rounded-lg border border-neutral-200 dark:border-neutral-700 overflow-hidden"
          headerClass="w-full px-3 py-2 text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400 bg-neutral-50 dark:bg-neutral-900/70 border-b border-neutral-200 dark:border-neutral-700"
        >
          <div class="max-h-72 overflow-auto p-1.5 text-[11px]" tabindex="0" @keydown="onTelemetryTreeRootKeydown">
            <button
              v-for="row in visibleTelemetryRows"
              :key="row.value"
              :ref="bindTelemetryItemElement(row.value)"
              type="button"
              class="w-full cursor-default flex items-center gap-2 rounded px-2 py-1 text-left hover:bg-neutral-100 dark:hover:bg-neutral-800/60"
              :aria-expanded="row.isLeaf ? undefined : isTelemetryExpanded(row.value)"
              :style="{ paddingLeft: `${Math.max(6, telemetryNodeLevel(row.value) * 12)}px` }"
              @click="onTelemetryRowClick(row.value)"
              @keydown="onTelemetryRowKeydown($event, row.value)"
            >
              <span class="w-3 text-[10px] text-neutral-500 dark:text-neutral-400">
                {{ row.isLeaf ? '•' : (isTelemetryExpanded(row.value) ? '▾' : '▸') }}
              </span>
              <span class="font-mono text-neutral-600 dark:text-neutral-300">{{ row.label }}</span>
              <span v-if="row.valueLabel !== null" class="ml-auto font-mono text-neutral-800 dark:text-neutral-100 break-all">{{ row.valueLabel }}</span>
            </button>
          </div>
        </UiAffinoDisclosure>

        <div class="grid grid-cols-1 gap-4 xl:grid-cols-1">
          <UiAffinoDisclosure title="Memory Stability">
            <div class="grid grid-cols-1 gap-2 sm:grid-cols-2 xl:grid-cols-4">
              <div
                v-for="row in memStabilityRows"
                :key="row.key"
                class="rounded-md border border-neutral-200 bg-neutral-50 px-3 py-2 dark:border-neutral-700 dark:bg-neutral-900/60"
              >
                <div class="text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400 flex items-center gap-1">
                  <span>{{ row.label }}</span>
                  <InlineInfoTooltip
                    v-if="tooltipFromMap(memStabilityTooltipByKey, row.key)"
                    :text="tooltipFromMap(memStabilityTooltipByKey, row.key)"
                    placement="top"
                    align="start"
                  />
                </div>
                <div class="mt-1 text-sm font-medium break-all">{{ row.value }}</div>
              </div>
            </div>
            <div class="mt-2 text-[11px] text-neutral-500 dark:text-neutral-400">
              Proxy telemetry for heap plateau after warmup (`mem_stab` from diagnostic heartbeat).
            </div>
          </UiAffinoDisclosure>

          <UiAffinoDisclosure title="Allocation Counters">
            <div class="grid grid-cols-1 gap-2 sm:grid-cols-2 xl:grid-cols-4">
              <div
                v-for="row in allocRows"
                :key="row.key"
                class="rounded-md border border-neutral-200 bg-neutral-50 px-3 py-2 dark:border-neutral-700 dark:bg-neutral-900/60"
              >
                <div class="text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400 flex items-center gap-1">
                  <span>{{ row.label }}</span>
                  <InlineInfoTooltip
                    v-if="tooltipFromMap(allocTooltipByKey, row.key)"
                    :text="tooltipFromMap(allocTooltipByKey, row.key)"
                    placement="top"
                    align="start"
                  />
                </div>
                <div class="mt-1 text-sm font-medium break-all">{{ row.value }}</div>
              </div>
            </div>
            <div class="mt-2 text-[11px] text-neutral-500 dark:text-neutral-400">
              Best-effort C++ new/delete counters from firmware (`diag.alloc`); use with `mem_stab` because third-party malloc/free may be outside coverage.
            </div>
          </UiAffinoDisclosure>

          <UiAffinoDisclosure title="Tasks / Stack">
            <div class="mb-2 rounded-md border border-neutral-200 bg-white/80 px-2 py-1.5 text-[11px] text-neutral-700 dark:border-neutral-700 dark:bg-neutral-950/40 dark:text-neutral-200">
              <span class="uppercase tracking-wide text-neutral-500 dark:text-neutral-400">stale</span>
              <span class="ml-2 font-semibold">{{ staleTasks ?? '—' }}</span>
            </div>

            <div
              v-if="stackRows.length"
              class="overflow-hidden rounded-md border border-neutral-200 bg-white shadow-sm dark:border-neutral-700 dark:bg-neutral-950/60"
            >
              <div class="flex flex-wrap items-center gap-x-4 gap-y-1 border-b border-neutral-200 bg-neutral-50/90 px-2 py-1.5 text-[10px] uppercase tracking-wide text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/80 dark:text-neutral-400">
                <span class="inline-flex items-center gap-1">
                  <span>task</span>
                  <InlineInfoTooltip text="RTOS/firmware task name from stack diagnostics." placement="top" align="start" />
                </span>
                <span class="inline-flex items-center gap-1">
                  <span>min_words</span>
                  <InlineInfoTooltip text="Minimum observed stack free words for the task." placement="top" align="start" />
                </span>
                <span class="inline-flex items-center gap-1">
                  <span>last_seen_ms</span>
                  <InlineInfoTooltip text="Timestamp (ms) of last scheduler/task heartbeat observation." placement="top" align="start" />
                </span>
              </div>
              <div class="affino-native-data-grid h-[220px] min-h-[120px] bg-white dark:bg-neutral-950/60">
                <div class="affino-native-data-grid__shell bg-white dark:bg-neutral-950/60">
                  <DataGrid
                    :rows="stackRows"
                    :columns="stackGridColumns"
                    :client-row-model-options="stackGridRowModelOptions"
                    :virtualization="{ rowOverscan: 4, columnOverscan: 1 }"
                    :base-row-height="30"
                    :show-row-index="false"
                    :row-selection="false"
                    layout-mode="fill"
                    :theme="theme"
                  />
                </div>
              </div>
            </div>
            <div v-else class="text-[11px] italic text-neutral-500 dark:text-neutral-400">
              No stack data
            </div>

            <div class="mt-2">
              <h5 class="text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400 mb-1">Reset</h5>
              <div class="text-[11px] text-neutral-700 dark:text-neutral-200">
                <span class="font-mono">code:</span> {{ asNumber(diagReset?.code) ?? '—' }}
                <span class="mx-2 text-neutral-400">|</span>
                <span class="font-mono">label:</span> {{ asString(diagReset?.label) ?? '—' }}
                <span class="mx-2 text-neutral-400">|</span>
                <span class="font-mono">boot:</span> {{ asNumber(diagReset?.boot) ?? '—' }}
              </div>
            </div>
          </UiAffinoDisclosure>
        </div>
      </template>
    </div>
  </div>
</template>
