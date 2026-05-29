<script setup lang="ts">
import { computed, nextTick, onUnmounted, ref, shallowRef, watch, type ComponentPublicInstance } from "vue"
import { RouterLink } from "vue-router"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"

import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"
import {
  mergeIec61850SignalList,
  type Iec61850SignalListMergeResult,
} from "./iec61850SignalListMerge"
import {
  runIec61850DebugSimulator,
  type Iec61850DebugSimulatorRunResult,
} from "./iec61850DebugSimulator"
import type {
  Iec61850DebugDetailAction,
  Iec61850DebugDetailRow,
  Iec61850DebugDetailSection,
  Iec61850DebugDiagnosticSummary,
  Iec61850DebugDocument,
  Iec61850DebugListDialogItem,
  Iec61850DebugReportSignalsAction,
  Iec61850DebugStats,
  Iec61850DebugTreeRow,
} from "./iec61850DebugTree"
import type {
  Iec61850DebugWorkerRequest,
  Iec61850DebugWorkerResponse,
} from "./iec61850DebugWorker"

type NodeValue = string
type LoadProgress = {
  label: string
  detail: string | null
}

type ParseResult = {
  contentHash: string
  document: Iec61850DebugDocument
  parseDurationMs: number
  treeBuildDurationMs: number
}

type SimulatorSummaryItem = {
  label: string
  count: number
  severity?: "error" | "warning" | "info"
  detail?: string | null
}

const EMPTY_STATS: Iec61850DebugStats = {
  sites: 0,
  voltageLevels: 0,
  bays: 0,
  switchgears: 0,
  ieds: 0,
  logicalDevices: 0,
  dataSets: 0,
  reports: 0,
  reportSignals: 0,
}

const EMPTY_DIAGNOSTIC_SUMMARY: Iec61850DebugDiagnosticSummary = {
  error: 0,
  warning: 0,
  info: 0,
  total: 0,
  rendered: 0,
  omitted: 0,
}

const fileInput = ref<HTMLInputElement | null>(null)
const fileName = ref<string | null>(null)
const contentHash = ref<string | null>(null)
const loading = ref(false)
const mergingSignalList = ref(false)
const simulatingReports = ref(false)
const loadProgress = ref<LoadProgress | null>(null)
const readError = ref<string | null>(null)
const debugDocument = shallowRef<Iec61850DebugDocument | null>(null)
const selectedValue = ref<NodeValue | null>(null)
const detailDialog = ref<Iec61850DebugDetailAction | null>(null)
const signalListMergeDialog = ref<Iec61850SignalListMergeResult | null>(null)
const simulatorResult = shallowRef<Iec61850DebugSimulatorRunResult | null>(null)
const pendingDefaultExpansion = ref(false)
const showReportCandidatesOnlyWithSignals = ref(false)
let loadRequestId = 0
let simulatorRequestId = 0
let activeParse: { worker: Worker; reject: (error: Error) => void } | null = null

const signalSheetStore = useSignalSheetStore()
const toastStore = useToastStore()

const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const treeRows = computed<Iec61850DebugTreeRow[]>(() => debugDocument.value?.treeRows ?? [])

const treeNodes = computed<TreeviewNode<NodeValue>[]>(() =>
  treeRows.value.map(row => ({ value: row.value, parent: row.parent })),
)

const rowByValue = computed(() => {
  const map = new Map<NodeValue, Iec61850DebugTreeRow>()
  treeRows.value.forEach(row => map.set(row.value, row))
  return map
})

const parentByValue = computed(() => {
  const map = new Map<NodeValue, NodeValue | null>()
  treeRows.value.forEach(row => map.set(row.value, row.parent))
  return map
})

const expandedSet = computed(() => new Set(tree.state.value.expanded))
const childrenByParent = computed(() => {
  const map = new Map<NodeValue | null, NodeValue[]>()
  treeRows.value.forEach((row) => {
    const children = map.get(row.parent) ?? []
    children.push(row.value)
    map.set(row.parent, children)
  })
  return map
})
const visibleRows = computed(() => treeRows.value.filter(row => isNodeVisible(row.value)))
const selectedRow = computed(() => (
  selectedValue.value ? rowByValue.value.get(selectedValue.value) ?? null : treeRows.value[0] ?? null
))

const diagnostics = computed(() => debugDocument.value?.diagnostics ?? [])
const diagnosticSummary = computed(() => debugDocument.value?.diagnosticSummary ?? EMPTY_DIAGNOSTIC_SUMMARY)

const stats = computed(() => debugDocument.value?.stats ?? EMPTY_STATS)
const mergeMatchedPreview = computed(() => signalListMergeDialog.value?.matches.slice(0, 100) ?? [])
const mergeMissPreview = computed(() => signalListMergeDialog.value?.misses.slice(0, 100) ?? [])
const mergeReportPreview = computed(() => signalListMergeDialog.value?.matchedReports.slice(0, 100) ?? [])
const simulatorReportIssueCount = computed(() => simulatorResult.value?.reports.filter(isSimulatorReportIssue).length ?? 0)
const simulatorReportPreview = computed(() => {
  const reports = simulatorResult.value?.reports ?? []
  return reports.filter(isSimulatorReportIssue).slice(0, 100)
})
const simulatorReportSummary = computed(() => summarizeSimulatorReports(simulatorResult.value?.reports ?? []))
const simulatorEventSummary = computed(() => summarizeSimulatorEvents(simulatorResult.value?.eventLog ?? []))
const simulatorDiagnosticsSummary = computed(() => summarizeSimulatorDiagnostics(simulatorResult.value?.diagnostics ?? []))
const simulatorDiagnosticCounts = computed(() => {
  const counts = { error: 0, warning: 0, info: 0, total: 0 }
  for (const diagnostic of simulatorResult.value?.diagnostics ?? []) {
    counts.total += 1
    counts[diagnostic.severity] += 1
  }
  return counts
})
const simulatorGiValueCount = computed(() => (
  simulatorResult.value?.reports.reduce((sum, report) => sum + (report.event?.values.length ?? 0), 0) ?? 0
))
const canRunSimulator = computed(() => Boolean(
  debugDocument.value
  && signalListMergeDialog.value?.matches.length
  && !loading.value
  && !mergingSignalList.value
  && !simulatingReports.value,
))

const statusLabel = computed(() => {
  if (loading.value) return loadProgress.value?.label ?? "Reading SCD"
  if (readError.value) return "Parse failed"
  if (!debugDocument.value) return "No SCD loaded"
  return `${fileName.value ?? "SCD"} · ${stats.value.sites} site · ${stats.value.ieds} IED · ${stats.value.reports} reports`
})

const itemElements = new Map<NodeValue, HTMLButtonElement>()

watch(
  treeNodes,
  (nodes) => {
    tree.registerNodes(nodes)
    if (!nodes.length) {
      selectedValue.value = null
      return
    }

    if (pendingDefaultExpansion.value) {
      applyDefaultExpansion()
      pendingDefaultExpansion.value = false
      return
    }

    if (!selectedValue.value || !rowByValue.value.has(selectedValue.value)) {
      selectNode(nodes[0].value)
    }
  },
  { immediate: true },
)

watch(
  () => tree.state.value.active,
  async (active) => {
    if (!active) return
    await nextTick()
    focusNodeElement(active)
  },
)

function openFileDialog() {
  fileInput.value?.click()
}

async function mergeWithSignalList() {
  const document = debugDocument.value
  if (!document || mergingSignalList.value) return

  mergingSignalList.value = true
  simulatorResult.value = null
  try {
    const [sheet, rows] = await Promise.all([
      signalSheetStore.ensureSheetLoaded(),
      signalSheetStore.ensureAllocationsLoaded(),
    ])
    if (!rows.length) {
      toastStore.info("Signal List is empty.")
      signalListMergeDialog.value = null
      return
    }

    const result = mergeIec61850SignalList(document, rows, sheet)
    signalListMergeDialog.value = result
    simulatorResult.value = null
    if (!result.addressColumn) {
      toastStore.warning("IEC 61850 address column was not detected in Signal List.")
      return
    }
    toastStore.success(`IEC 61850 merge: ${result.matchedRows}/${result.addressRows} addresses matched.`)
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to merge with Signal List")
  } finally {
    mergingSignalList.value = false
  }
}

async function runSimulatorReports() {
  const document = debugDocument.value
  const mergeResult = signalListMergeDialog.value
  if (!document || !mergeResult || simulatingReports.value) return

  if (!mergeResult.matches.length) {
    toastStore.info("No matched IEC 61850 signals for simulator run.")
    simulatorResult.value = null
    return
  }

  const requestId = simulatorRequestId + 1
  simulatorRequestId = requestId
  simulatingReports.value = true
  try {
    const result = await runIec61850DebugSimulator(document, mergeResult)
    if (requestId !== simulatorRequestId || debugDocument.value !== document) return
    simulatorResult.value = result
    const failedReports = result.reports.filter(report => report.errorCode).length
    if (failedReports) {
      toastStore.warning(`IEC 61850 simulator: ${failedReports}/${result.reports.length} reports failed.`)
      return
    }
    toastStore.success(`IEC 61850 simulator: ${result.reports.length} reports completed.`)
  } catch (error) {
    if (requestId === simulatorRequestId) {
      toastStore.error(error instanceof Error ? error.message : "IEC 61850 simulator failed")
    }
  } finally {
    if (requestId === simulatorRequestId) {
      simulatingReports.value = false
    }
  }
}

async function onFileSelected(event: Event) {
  const input = event.target as HTMLInputElement | null
  const file = input?.files?.[0]
  if (!file) return

  const requestId = loadRequestId + 1
  loadRequestId = requestId
  simulatorRequestId += 1
  terminateActiveParse()
  loading.value = true
  simulatingReports.value = false
  fileName.value = file.name
  contentHash.value = null
  debugDocument.value = null
  signalListMergeDialog.value = null
  simulatorResult.value = null
  selectedValue.value = null
  loadProgress.value = {
    label: "Reading SCD",
    detail: `${file.name} · ${formatBytes(file.size)}`,
  }
  readError.value = null

  try {
    loadProgress.value = {
      label: "Starting worker",
      detail: `${file.name} · ${formatBytes(file.size)}`,
    }
    const parsed = await parseScdInWorker(requestId, file)
    if (requestId !== loadRequestId) return

    contentHash.value = parsed.contentHash
    debugDocument.value = parsed.document
    loadProgress.value = {
      label: "Rendering debug tree",
      detail: `Parsed in ${parsed.parseDurationMs}ms · tree in ${parsed.treeBuildDurationMs}ms`,
    }
    pendingDefaultExpansion.value = true
  } catch (error) {
    if (error instanceof Error && error.message === "cancelled") return
    debugDocument.value = null
    fileName.value = file.name
    contentHash.value = null
    readError.value = error instanceof Error ? error.message : "Failed to read SCD file"
  } finally {
    if (requestId === loadRequestId) {
      loading.value = false
      loadProgress.value = null
    }
    if (input) input.value = ""
  }
}

function parseScdInWorker(requestId: number, selectedFile: File): Promise<ParseResult> {
  terminateActiveParse()

  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL("./iec61850DebugWorker.ts", import.meta.url), { type: "module" })
    activeParse = { worker, reject }

    worker.onmessage = (event: MessageEvent<Iec61850DebugWorkerResponse>) => {
      const message = event.data
      if (message.requestId !== requestId) return

      if (message.type === "progress") {
        loadProgress.value = {
          label: message.label,
          detail: `${selectedFile.name} · ${formatBytes(selectedFile.size)}`,
        }
        return
      }

      cleanupActiveParse(worker)
      if (message.type === "error") {
        reject(new Error(message.message))
        return
      }

      resolve({
        contentHash: message.contentHash,
        document: message.document,
        parseDurationMs: message.parseDurationMs,
        treeBuildDurationMs: message.treeBuildDurationMs,
      })
    }

    worker.onerror = (event) => {
      cleanupActiveParse(worker)
      reject(new Error(event.message || "SCD worker failed"))
    }

    worker.postMessage({
      type: "parse",
      requestId,
      file: selectedFile,
    } satisfies Iec61850DebugWorkerRequest)
  })
}

function terminateActiveParse() {
  if (!activeParse) return
  const current = activeParse
  activeParse = null
  current.worker.terminate()
  current.reject(new Error("cancelled"))
}

function cleanupActiveParse(worker: Worker) {
  if (activeParse?.worker === worker) {
    activeParse = null
  }
  worker.terminate()
}

function applyDefaultExpansion() {
  for (const row of treeRows.value) {
    if (
      row.kind === "site"
      || row.kind === "voltage-level"
      || row.kind === "ieds-group"
    ) {
      tree.expand(row.value)
    }
  }
  const first = treeRows.value[0]
  if (first) {
    selectNode(first.value)
  }
}

function selectNode(value: NodeValue) {
  selectedValue.value = value
  tree.focus(value)
  tree.select(value)
}

function onTreeRowClick(row: Iec61850DebugTreeRow) {
  selectNode(row.value)
  if (!row.isLeaf) {
    tree.toggle(row.value)
  }
}

function openDetailDialog(action: Iec61850DebugDetailAction) {
  detailDialog.value = action
}

function openDetailRowAction(row: Iec61850DebugDetailRow) {
  if (row.action) {
    openDetailDialog(row.action)
    return
  }
  if (row.reportSignalsAction) {
    openReportSignalsDialog(row.label, row.reportSignalsAction)
  }
}

function openReportSignalsDialog(reportControlName: string, action: Iec61850DebugReportSignalsAction) {
  const reportRow = rowByValue.value.get(action.reportControlValue)
  const signalRows = reportRow?.detail.sections
    .find(section => section.title === "Resolved signals")
    ?.rows ?? []

  detailDialog.value = {
    kind: "list-dialog",
    title: `${reportControlName} signals`,
    subtitle: `${action.reportKind} · ${action.dataSetRef ?? "unresolved DataSet"} · ${action.signalCount} signals`,
    emptyLabel: "No resolved report signals.",
    items: signalRowsToDialogItems(reportControlName, action, signalRows),
  }
}

function signalRowsToDialogItems(
  reportControlName: string,
  action: Iec61850DebugReportSignalsAction,
  signalRows: Iec61850DebugDetailRow[],
): Iec61850DebugListDialogItem[] {
  return signalRows.map((signalRow): Iec61850DebugListDialogItem => ({
    title: signalRow.value,
    subtitle: `${reportControlName} · ${action.dataSetRef ?? "unresolved DataSet"}`,
    rows: [
      { label: "ReportControl", value: reportControlName },
      { label: "report kind", value: action.reportKind },
      { label: "DataSet ref", value: action.dataSetRef ?? "—" },
      { label: "signal row", value: signalRow.label },
      { label: "signal ref", value: signalRow.value },
    ],
  }))
}

function closeDetailDialog() {
  detailDialog.value = null
}

function closeSignalListMergeDialog() {
  signalListMergeDialog.value = null
}

function isSimulatorReportIssue(report: Iec61850DebugSimulatorRunResult["reports"][number]): boolean {
  return Boolean(report.errorCode || report.diagnostics.some(diagnostic => diagnostic.severity === "error"))
}

function summarizeSimulatorReports(
  reports: Iec61850DebugSimulatorRunResult["reports"],
): SimulatorSummaryItem[] {
  const summaries = new Map<string, SimulatorSummaryItem>()
  for (const report of reports) {
    const label = `${report.reportKind} · ${simulatorReportStatusLabel(report)}`
    const values = report.event?.values.length ?? 0
    const existing = summaries.get(label)
    if (existing) {
      existing.count += 1
      existing.detail = `${Number.parseInt(existing.detail ?? "0", 10) + values} GI values`
      continue
    }
    summaries.set(label, {
      label,
      count: 1,
      severity: report.errorCode ? "error" : "info",
      detail: `${values} GI values`,
    })
  }
  return Array.from(summaries.values())
    .sort((left, right) => severityRank(right.severity) - severityRank(left.severity) || right.count - left.count || left.label.localeCompare(right.label))
}

function summarizeSimulatorEvents(
  events: Iec61850DebugSimulatorRunResult["eventLog"],
): SimulatorSummaryItem[] {
  const counts = new Map<string, number>()
  for (const event of events) {
    counts.set(event.kind, (counts.get(event.kind) ?? 0) + 1)
  }
  return Array.from(counts.entries())
    .map(([label, count]) => ({ label, count }))
    .sort((left, right) => right.count - left.count || left.label.localeCompare(right.label))
}

function summarizeSimulatorDiagnostics(
  diagnostics: Iec61850DebugSimulatorRunResult["diagnostics"],
): SimulatorSummaryItem[] {
  const summaries = new Map<string, SimulatorSummaryItem>()
  for (const diagnostic of diagnostics) {
    const existing = summaries.get(diagnostic.code)
    if (existing) {
      existing.count += 1
      continue
    }
    summaries.set(diagnostic.code, {
      label: diagnostic.code,
      count: 1,
      severity: diagnostic.severity,
      detail: diagnostic.message,
    })
  }
  return Array.from(summaries.values())
    .sort((left, right) => severityRank(right.severity) - severityRank(left.severity) || right.count - left.count || left.label.localeCompare(right.label))
    .slice(0, 80)
}

function severityRank(severity: SimulatorSummaryItem["severity"]): number {
  if (severity === "error") return 3
  if (severity === "warning") return 2
  if (severity === "info") return 1
  return 0
}

function simulatorReportStatusClass(report: Iec61850DebugSimulatorRunResult["reports"][number]): string {
  return report.errorCode ? "is-error" : "is-success"
}

function simulatorReportStatusLabel(report: Iec61850DebugSimulatorRunResult["reports"][number]): string {
  return report.errorCode ? report.errorCode : report.lifecycleState
}

function isReportCandidatesSection(section: Iec61850DebugDetailSection): boolean {
  return section.title === "Report candidates"
}

function visibleDetailRows(section: Iec61850DebugDetailSection) {
  if (!isReportCandidatesSection(section) || !showReportCandidatesOnlyWithSignals.value) {
    return section.rows
  }
  return section.rows.filter(row => (row.signalCount ?? 0) > 0)
}

function reportCandidatesFilterSummary(section: Iec61850DebugDetailSection): string {
  const withSignals = section.rows.filter(row => (row.signalCount ?? 0) > 0).length
  return `${withSignals} of ${section.rows.length}`
}

function bindItemElement(value: NodeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element
      ? element
      : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLButtonElement) {
      itemElements.set(value, resolved)
      return
    }
    itemElements.delete(value)
  }
}

function focusNodeElement(value: NodeValue) {
  const element = itemElements.get(value)
  if (!element) return
  element.focus({ preventScroll: true })
  element.scrollIntoView({ block: "nearest" })
}

function isNodeVisible(value: NodeValue): boolean {
  let cursor = parentByValue.value.get(value) ?? null
  while (cursor) {
    if (!expandedSet.value.has(cursor)) return false
    cursor = parentByValue.value.get(cursor) ?? null
  }
  return true
}

function nodeLevel(value: NodeValue): number {
  let level = 1
  let cursor = parentByValue.value.get(value) ?? null
  const visited = new Set<NodeValue>()
  while (cursor && !visited.has(cursor)) {
    visited.add(cursor)
    level += 1
    cursor = parentByValue.value.get(cursor) ?? null
  }
  return level
}

function isSelected(value: NodeValue): boolean {
  return selectedValue.value === value
}

function isActive(value: NodeValue): boolean {
  return tree.isActive(value)
}

function isExpanded(value: NodeValue): boolean {
  return tree.isExpanded(value)
}

function onTreeRootKeydown(event: KeyboardEvent) {
  const active = tree.state.value.active
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      if (active) tree.focusNext()
      else tree.focusFirst()
      return
    case "ArrowUp":
      event.preventDefault()
      if (active) tree.focusPrevious()
      else tree.focusLast()
      return
    case "Home":
      event.preventDefault()
      tree.focusFirst()
      return
    case "End":
      event.preventDefault()
      tree.focusLast()
      return
    case "ArrowRight":
      if (!active) return
      event.preventDefault()
      expandOrFocusFirstChild(active)
      return
    case "ArrowLeft":
      if (!active) return
      event.preventDefault()
      collapseOrFocusParent(active)
      return
    case "Enter":
    case " ":
      if (!active) return
      event.preventDefault()
      {
        const row = rowByValue.value.get(active)
        if (row) onTreeRowClick(row)
      }
      return
    default:
      return
  }
}

function expandOrFocusFirstChild(value: NodeValue) {
  const row = rowByValue.value.get(value)
  if (!row || row.isLeaf) return
  if (!tree.isExpanded(value)) {
    tree.expand(value)
    return
  }
  const firstChild = childrenByParent.value.get(value)?.[0]
  if (firstChild) {
    tree.focus(firstChild)
  }
}

function collapseOrFocusParent(value: NodeValue) {
  const row = rowByValue.value.get(value)
  if (row && !row.isLeaf && tree.isExpanded(value)) {
    tree.collapse(value)
    return
  }
  const parent = parentByValue.value.get(value)
  if (parent) {
    tree.focus(parent)
  }
}

function nodeKindLabel(kind: string): string {
  switch (kind) {
    case "site":
      return "site"
    case "voltage-level":
      return "voltage"
    case "bay":
      return "bay"
    case "switchgears-group":
      return "group"
    case "switchgear":
      return "switchgear"
    case "ieds-group":
      return "group"
    case "ied":
      return "ied"
    case "access-point":
      return "ap"
    case "server":
      return "server"
    case "logical-device":
      return "ld"
    case "logical-node":
      return "ln"
    case "datasets-group":
      return "group"
    case "dataset":
      return "dataset"
    case "dataset-member":
      return "signal"
    case "reports-group":
      return "group"
    case "report-control":
      return "rcb"
    case "report-signal":
      return "signal"
    default:
      return kind
  }
}

function shortHash(value: string | null): string {
  return value ? value.slice(0, 12) : "—"
}

function formatBytes(value: number): string {
  if (!Number.isFinite(value) || value <= 0) return "0 B"
  const units = ["B", "KB", "MB", "GB"]
  let size = value
  let unitIndex = 0
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024
    unitIndex += 1
  }
  return `${size >= 10 || unitIndex === 0 ? size.toFixed(0) : size.toFixed(1)} ${units[unitIndex]}`
}

onUnmounted(() => {
  terminateActiveParse()
})
</script>

<template>
  <div class="iec61850-debug-page">
    <header class="iec61850-debug-page__header">
      <div class="iec61850-debug-page__title-block">
        <p class="iec61850-debug-page__eyebrow">IEC 61850</p>
        <h1 class="iec61850-debug-page__title">61850 Debug</h1>
        <p class="iec61850-debug-page__status">{{ statusLabel }}</p>
      </div>

      <div class="iec61850-debug-page__actions">
        <RouterLink class="iec61850-debug-page__nav-link" to="/61850-debug/templates">
          Bay templates
        </RouterLink>
        <UiButton
          variant="secondary"
          size="sm"
          :disabled="!debugDocument || loading || mergingSignalList"
          @click="mergeWithSignalList"
        >
          {{ mergingSignalList ? "Merging..." : "Merge with Signal List" }}
        </UiButton>
        <UiButton variant="secondary" size="sm" :disabled="loading" @click="openFileDialog">
          {{ debugDocument ? "Load another SCD" : "Choose SCD" }}
        </UiButton>
        <input
          ref="fileInput"
          class="iec61850-debug-page__file-input"
          type="file"
          accept=".scd,.ssd,.xml,text/xml,application/xml"
          autocomplete="off"
          @change="onFileSelected"
        />
      </div>
    </header>

    <section v-if="readError" class="iec61850-debug-page__alert">
      {{ readError }}
    </section>

    <section v-if="loading" class="iec61850-debug-page__progress" aria-live="polite">
      <div class="iec61850-debug-page__progress-copy">
        <span class="iec61850-debug-page__progress-title">{{ loadProgress?.label ?? "Reading SCD" }}</span>
        <span v-if="loadProgress?.detail" class="iec61850-debug-page__progress-detail">{{ loadProgress.detail }}</span>
      </div>
      <div class="iec61850-debug-page__progress-track" aria-hidden="true">
        <span class="iec61850-debug-page__progress-bar"></span>
      </div>
    </section>

    <section v-if="debugDocument" class="iec61850-debug-page__summary" aria-label="SCD summary">
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Sites</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.sites }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Voltage levels</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.voltageLevels }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Bays</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.bays }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Switchgears</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.switchgears }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">IEDs</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.ieds }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Logical devices</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.logicalDevices }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">DataSets</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.dataSets }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Reports</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.reports }}</span>
      </div>
      <div class="iec61850-debug-page__metric">
        <span class="iec61850-debug-page__metric-label">Report signals</span>
        <span class="iec61850-debug-page__metric-value">{{ stats.reportSignals }}</span>
      </div>
      <div class="iec61850-debug-page__metric iec61850-debug-page__metric--wide">
        <span class="iec61850-debug-page__metric-label">Source hash</span>
        <span class="iec61850-debug-page__metric-value">{{ shortHash(contentHash) }}</span>
      </div>
    </section>

    <main class="iec61850-debug-page__workspace">
      <section class="iec61850-debug-page__tree-panel" aria-label="SCD model tree">
        <div class="iec61850-debug-page__panel-header">
          <span>SCL model</span>
          <span v-if="treeRows.length" class="iec61850-debug-page__panel-count">{{ treeRows.length }} nodes</span>
        </div>

        <div v-if="!debugDocument" class="iec61850-debug-page__empty">
          Choose an SCD file to inspect topology, IEDs, DataSets and ReportControls.
        </div>

        <div
          v-else
          class="iec61850-debug-page__tree"
          role="tree"
          tabindex="0"
          aria-label="IEC 61850 SCL model"
          @keydown="onTreeRootKeydown"
        >
          <button
            v-for="row in visibleRows"
            :key="row.value"
            :ref="bindItemElement(row.value)"
            type="button"
            class="iec61850-debug-page__tree-row"
            :class="{
              'is-selected': isSelected(row.value),
              'is-active': isActive(row.value),
            }"
            role="treeitem"
            :aria-level="nodeLevel(row.value)"
            :aria-selected="isSelected(row.value)"
            :aria-expanded="row.isLeaf ? undefined : isExpanded(row.value)"
            :tabindex="isActive(row.value) ? 0 : -1"
            :style="{ paddingLeft: `${Math.max(8, nodeLevel(row.value) * 14)}px` }"
            @click="onTreeRowClick(row)"
          >
            <span class="iec61850-debug-page__tree-toggle" aria-hidden="true">
              {{ row.isLeaf ? "•" : (isExpanded(row.value) ? "▾" : "▸") }}
            </span>
            <span class="iec61850-debug-page__tree-kind">{{ nodeKindLabel(row.kind) }}</span>
            <span class="iec61850-debug-page__tree-label">{{ row.label }}</span>
            <span v-if="row.valueLabel" class="iec61850-debug-page__tree-value">{{ row.valueLabel }}</span>
          </button>
        </div>
      </section>

      <section class="iec61850-debug-page__detail-panel" aria-label="Selected SCD element details">
        <div class="iec61850-debug-page__panel-header">
          <span>Properties</span>
          <span v-if="selectedRow" class="iec61850-debug-page__panel-count">{{ nodeKindLabel(selectedRow.kind) }}</span>
        </div>

        <div v-if="!selectedRow" class="iec61850-debug-page__empty">
          Select an item in the tree.
        </div>

        <article v-else class="iec61850-debug-page__detail">
          <div class="iec61850-debug-page__detail-heading">
            <h2>{{ selectedRow.detail.title }}</h2>
            <p>{{ selectedRow.detail.subtitle }}</p>
          </div>

          <div class="iec61850-debug-page__sections">
            <section
              v-for="section in selectedRow.detail.sections"
              :key="section.title"
              class="iec61850-debug-page__section"
            >
              <div class="iec61850-debug-page__section-heading">
                <h3>{{ section.title }}</h3>
                <label
                  v-if="isReportCandidatesSection(section)"
                  class="iec61850-debug-page__section-filter"
                >
                  <input
                    v-model="showReportCandidatesOnlyWithSignals"
                    type="checkbox"
                  />
                  <span>Show only with signals</span>
                  <span class="iec61850-debug-page__section-filter-count">
                    {{ reportCandidatesFilterSummary(section) }}
                  </span>
                </label>
              </div>
              <dl v-if="visibleDetailRows(section).length" class="iec61850-debug-page__property-list">
                <template v-for="row in visibleDetailRows(section)" :key="`${section.title}:${row.label}`">
                  <dt>{{ row.label }}</dt>
                  <dd>
                    <button
                      v-if="row.action || row.reportSignalsAction"
                      type="button"
                      class="iec61850-debug-page__property-action"
                      @click="openDetailRowAction(row)"
                    >
                      {{ row.value }}
                    </button>
                    <span v-else>{{ row.value }}</span>
                  </dd>
                </template>
              </dl>
              <div v-else class="iec61850-debug-page__section-empty">
                No report candidates with signals.
              </div>
            </section>
          </div>
        </article>
      </section>
    </main>

    <section v-if="simulatorResult" class="iec61850-debug-page__runtime" aria-label="IEC 61850 simulator runtime">
      <div class="iec61850-debug-page__panel-header">
        <span>Simulator runtime</span>
        <span class="iec61850-debug-page__panel-count">
          {{ simulatorResult.reports.length }} reports · {{ simulatorEventSummary.length }} event kinds
        </span>
      </div>

      <section class="iec61850-debug-page__runtime-summary" aria-label="Simulator summary">
        <div class="iec61850-debug-page__runtime-metric">
          <span>Plan reports</span>
          <strong>{{ simulatorResult.plan.requiredReportCount }}</strong>
        </div>
        <div class="iec61850-debug-page__runtime-metric">
          <span>Report issues</span>
          <strong>{{ simulatorReportIssueCount }}</strong>
        </div>
        <div class="iec61850-debug-page__runtime-metric">
          <span>Matched signals</span>
          <strong>{{ simulatorResult.plan.matchedSignalCount }}</strong>
        </div>
        <div class="iec61850-debug-page__runtime-metric">
          <span>GI values</span>
          <strong>{{ simulatorGiValueCount }}</strong>
        </div>
        <div class="iec61850-debug-page__runtime-metric">
          <span>Errors</span>
          <strong>{{ simulatorDiagnosticCounts.error }}</strong>
        </div>
        <div class="iec61850-debug-page__runtime-metric">
          <span>Warnings</span>
          <strong>{{ simulatorDiagnosticCounts.warning }}</strong>
        </div>
        <div class="iec61850-debug-page__runtime-metric">
          <span>Info</span>
          <strong>{{ simulatorDiagnosticCounts.info }}</strong>
        </div>
      </section>

      <div class="iec61850-debug-page__runtime-body">
        <section class="iec61850-debug-page__runtime-section">
          <h3>{{ simulatorReportIssueCount ? "Report issues" : "Report summary" }}</h3>
          <p
            v-if="simulatorReportIssueCount"
            class="iec61850-debug-page__runtime-note"
          >
            Showing first {{ simulatorReportPreview.length }} of {{ simulatorReportIssueCount }} issue reports.
          </p>
          <p
            v-else
            class="iec61850-debug-page__runtime-note"
          >
            All reports completed; grouped by report kind and final state.
          </p>
          <div v-if="simulatorReportIssueCount" class="iec61850-debug-page__runtime-list">
            <article
              v-for="report in simulatorReportPreview"
              :key="report.candidateId"
              class="iec61850-debug-page__runtime-item"
              :class="simulatorReportStatusClass(report)"
            >
              <strong>{{ report.reportControlName }}</strong>
              <span>{{ report.reportKind }} · {{ report.iedName }} / {{ report.accessPointName }}</span>
              <code>{{ report.dataSetRef ?? "unresolved DataSet" }}</code>
              <small>
                {{ simulatorReportStatusLabel(report) }} · {{ report.matchedSignalCount }} matched · {{ report.event?.values.length ?? 0 }} values
              </small>
            </article>
          </div>
          <div v-else class="iec61850-debug-page__runtime-list">
            <article
              v-for="summary in simulatorReportSummary"
              :key="summary.label"
              class="iec61850-debug-page__runtime-item"
            >
              <strong>{{ summary.label }}</strong>
              <span>{{ summary.count }} reports</span>
              <code v-if="summary.detail">{{ summary.detail }}</code>
            </article>
          </div>
        </section>

        <section class="iec61850-debug-page__runtime-section">
          <h3>Event summary</h3>
          <p
            v-if="simulatorResult.eventLog.length"
            class="iec61850-debug-page__runtime-note"
          >
            {{ simulatorResult.eventLog.length }} raw simulator events grouped by kind.
          </p>
          <div class="iec61850-debug-page__runtime-list">
            <article
              v-for="event in simulatorEventSummary"
              :key="event.label"
              class="iec61850-debug-page__runtime-item"
            >
              <strong>{{ event.label }}</strong>
              <span>{{ event.count }} events</span>
            </article>
          </div>
        </section>

        <section class="iec61850-debug-page__runtime-section">
          <h3>Advisory codes</h3>
          <p
            v-if="simulatorResult.diagnostics.length > simulatorDiagnosticsSummary.length"
            class="iec61850-debug-page__runtime-note"
          >
            {{ simulatorResult.diagnostics.length }} non-blocking diagnostics grouped into {{ simulatorDiagnosticsSummary.length }} shown codes.
          </p>
          <div v-if="!simulatorDiagnosticsSummary.length" class="iec61850-debug-page__section-empty">
            No simulator advisories.
          </div>
          <div v-else class="iec61850-debug-page__runtime-list">
            <article
              v-for="diagnostic in simulatorDiagnosticsSummary"
              :key="diagnostic.label"
              class="iec61850-debug-page__runtime-item"
              :class="`is-${diagnostic.severity}`"
            >
              <strong>{{ diagnostic.label }}</strong>
              <span>{{ diagnostic.count }} {{ diagnostic.severity ?? "info" }}</span>
              <code v-if="diagnostic.detail">{{ diagnostic.detail }}</code>
            </article>
          </div>
        </section>
      </div>
    </section>

    <section v-if="debugDocument" class="iec61850-debug-page__diagnostics" aria-label="Parser diagnostics">
      <div class="iec61850-debug-page__panel-header">
        <span>Diagnostics</span>
        <span class="iec61850-debug-page__panel-count">
          {{ diagnosticSummary.error }} errors · {{ diagnosticSummary.warning }} warnings · {{ diagnosticSummary.info }} info
        </span>
      </div>
      <div v-if="!diagnostics.length" class="iec61850-debug-page__diagnostics-empty">
        No parser diagnostics.
      </div>
      <template v-else>
        <div v-if="diagnosticSummary.omitted > 0" class="iec61850-debug-page__diagnostics-empty">
          Showing {{ diagnosticSummary.rendered }} of {{ diagnosticSummary.total }} diagnostics.
        </div>
        <div class="iec61850-debug-page__diagnostics-list">
          <div
            v-for="diagnostic in diagnostics"
            :key="`${diagnostic.stage}:${diagnostic.code}:${diagnostic.sourcePath ?? ''}:${diagnostic.message}`"
            class="iec61850-debug-page__diagnostic"
            :class="`is-${diagnostic.severity}`"
          >
            <span class="iec61850-debug-page__diagnostic-code">{{ diagnostic.stage }}.{{ diagnostic.code }}</span>
            <span class="iec61850-debug-page__diagnostic-message">{{ diagnostic.message }}</span>
            <span v-if="diagnostic.sourcePath" class="iec61850-debug-page__diagnostic-path">{{ diagnostic.sourcePath }}</span>
          </div>
        </div>
      </template>
    </section>

    <UiModal
      :open="Boolean(detailDialog)"
      :title="detailDialog?.title ?? 'Details'"
      max-width="5xl"
      desktop-height="78vh"
      :content-scroll="false"
      @close="closeDetailDialog"
    >
      <div v-if="detailDialog" class="iec61850-debug-page__list-dialog">
        <p class="iec61850-debug-page__list-dialog-subtitle">{{ detailDialog.subtitle }}</p>
        <p class="iec61850-debug-page__list-dialog-count">{{ detailDialog.items.length }} items</p>

        <div v-if="!detailDialog.items.length" class="iec61850-debug-page__empty">
          {{ detailDialog.emptyLabel }}
        </div>

        <div v-else class="iec61850-debug-page__list-dialog-items">
          <article
            v-for="item in detailDialog.items"
            :key="`${item.title}:${item.subtitle}`"
            class="iec61850-debug-page__list-dialog-item"
          >
            <h3>{{ item.title }}</h3>
            <p>{{ item.subtitle }}</p>
            <dl class="iec61850-debug-page__property-list">
              <template v-for="row in item.rows" :key="`${item.title}:${row.label}`">
                <dt>{{ row.label }}</dt>
                <dd>{{ row.value }}</dd>
              </template>
            </dl>
          </article>
        </div>
      </div>
    </UiModal>

    <UiModal
      :open="Boolean(signalListMergeDialog)"
      title="Merge with Signal List"
      max-width="6xl"
      desktop-height="78vh"
      :content-scroll="false"
      @close="closeSignalListMergeDialog"
    >
      <div v-if="signalListMergeDialog" class="iec61850-debug-page__merge-dialog">
        <section class="iec61850-debug-page__merge-summary" aria-label="Merge summary">
          <div class="iec61850-debug-page__merge-metric">
            <span>Signal rows</span>
            <strong>{{ signalListMergeDialog.rowCount }}</strong>
          </div>
          <div class="iec61850-debug-page__merge-metric">
            <span>Address column</span>
            <strong>{{ signalListMergeDialog.addressColumn ?? "not detected" }}</strong>
          </div>
          <div class="iec61850-debug-page__merge-metric">
            <span>Matched</span>
            <strong>{{ signalListMergeDialog.matchedRows }} / {{ signalListMergeDialog.addressRows }}</strong>
          </div>
          <div class="iec61850-debug-page__merge-metric">
            <span>Reports</span>
            <strong>{{ signalListMergeDialog.matchedReports.length }}</strong>
          </div>
          <div class="iec61850-debug-page__merge-metric">
            <span>IEDs</span>
            <strong>{{ signalListMergeDialog.matchedIeds.length }}</strong>
          </div>
          <div class="iec61850-debug-page__merge-action">
            <UiButton
              variant="secondary"
              size="sm"
              :disabled="!canRunSimulator"
              @click="runSimulatorReports"
            >
              {{ simulatingReports ? "Simulating..." : "Run simulator GI" }}
            </UiButton>
          </div>
        </section>

        <div class="iec61850-debug-page__merge-body">
          <section class="iec61850-debug-page__merge-section">
            <h3>Reports involved</h3>
            <p
              v-if="signalListMergeDialog.matchedReports.length > mergeReportPreview.length"
              class="iec61850-debug-page__merge-section-note"
            >
              Showing first {{ mergeReportPreview.length }} of {{ signalListMergeDialog.matchedReports.length }}.
            </p>
            <div v-if="!mergeReportPreview.length" class="iec61850-debug-page__section-empty">
              No reports matched.
            </div>
            <div v-else class="iec61850-debug-page__merge-list">
              <article
                v-for="report in mergeReportPreview"
                :key="`${report.iedName}:${report.name}:${report.dataSetRef}`"
                class="iec61850-debug-page__merge-item"
              >
                <strong>{{ report.name }}</strong>
                <span>{{ report.kind }} · {{ report.iedName ?? "unknown IED" }}</span>
                <code>{{ report.dataSetRef ?? "unresolved DataSet" }}</code>
              </article>
            </div>
          </section>

          <section class="iec61850-debug-page__merge-section">
            <h3>Matched signals</h3>
            <p
              v-if="signalListMergeDialog.matches.length > mergeMatchedPreview.length"
              class="iec61850-debug-page__merge-section-note"
            >
              Showing first {{ mergeMatchedPreview.length }} of {{ signalListMergeDialog.matches.length }}.
            </p>
            <div v-if="!mergeMatchedPreview.length" class="iec61850-debug-page__section-empty">
              No Signal List rows matched IEC 61850 model signals.
            </div>
            <div v-else class="iec61850-debug-page__merge-list">
              <article
                v-for="match in mergeMatchedPreview"
                :key="`match:${match.signalId}`"
                class="iec61850-debug-page__merge-item"
              >
                <strong>{{ match.signalName || match.signalKey }}</strong>
                <span>{{ match.address }}</span>
                <code>{{ match.modelReference }}</code>
                <small>{{ match.reports.length }} reports · {{ match.dataSets.length }} DataSets</small>
              </article>
            </div>
          </section>

          <section class="iec61850-debug-page__merge-section">
            <h3>Not found</h3>
            <p
              v-if="signalListMergeDialog.misses.length > mergeMissPreview.length"
              class="iec61850-debug-page__merge-section-note"
            >
              Showing first {{ mergeMissPreview.length }} of {{ signalListMergeDialog.misses.length }}.
            </p>
            <div v-if="!mergeMissPreview.length" class="iec61850-debug-page__section-empty">
              No unmatched IEC 61850 addresses.
            </div>
            <div v-else class="iec61850-debug-page__merge-list">
              <article
                v-for="miss in mergeMissPreview"
                :key="`miss:${miss.signalId}`"
                class="iec61850-debug-page__merge-item is-miss"
              >
                <strong>{{ miss.signalName || miss.signalKey }}</strong>
                <span>{{ miss.address }}</span>
              </article>
            </div>
          </section>
        </div>
      </div>
    </UiModal>
  </div>
</template>

<style scoped>
.iec61850-debug-page {
  display: flex;
  height: 100%;
  min-height: 100%;
  flex-direction: column;
  gap: 0.75rem;
  padding: 1rem;
  overflow-x: hidden;
  overflow-y: auto;
  color: var(--color-neutral-800);
}

.iec61850-debug-page__header,
.iec61850-debug-page__summary,
.iec61850-debug-page__tree-panel,
.iec61850-debug-page__detail-panel,
.iec61850-debug-page__runtime,
.iec61850-debug-page__diagnostics,
.iec61850-debug-page__progress,
.iec61850-debug-page__alert {
  border: 1px solid color-mix(in srgb, var(--runtime-accent) 12%, var(--color-neutral-200));
  border-radius: var(--radius-lg);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 96%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-white) 88%, var(--color-neutral-50)));
  box-shadow:
    inset 0 1px 0 rgb(255 255 255 / 0.72),
    0 14px 28px rgb(15 23 42 / 0.05);
}

.iec61850-debug-page__header {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  padding: 1rem;
}

.iec61850-debug-page__title-block {
  min-width: 0;
}

.iec61850-debug-page__eyebrow,
.iec61850-debug-page__status,
.iec61850-debug-page__panel-header,
.iec61850-debug-page__metric-label,
.iec61850-debug-page__tree-kind,
.iec61850-debug-page__diagnostic-code {
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 700;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-debug-page__eyebrow,
.iec61850-debug-page__status,
.iec61850-debug-page__title {
  margin: 0;
}

.iec61850-debug-page__title {
  margin-top: 0.125rem;
  color: var(--color-neutral-950);
  font-size: 1.5rem;
  line-height: 1.2;
}

.iec61850-debug-page__status {
  margin-top: 0.375rem;
  text-transform: none;
}

.iec61850-debug-page__actions {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 0.5rem;
}

.iec61850-debug-page__nav-link {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: 2rem;
  padding: 0.4rem 0.75rem;
  border: 1px solid color-mix(in srgb, var(--runtime-accent) 26%, var(--color-neutral-300));
  border-radius: var(--radius-sm);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 700;
  text-decoration: none;
}

.iec61850-debug-page__file-input {
  display: none;
}

.iec61850-debug-page__alert {
  flex: 0 0 auto;
  padding: 0.75rem 1rem;
  border-color: var(--color-red-300);
  color: var(--color-red-700);
  font-size: var(--text-sm);
}

.iec61850-debug-page__progress {
  display: grid;
  flex: 0 0 auto;
  gap: 0.625rem;
  padding: 0.75rem 1rem;
}

.iec61850-debug-page__progress-copy {
  display: flex;
  min-width: 0;
  align-items: baseline;
  justify-content: space-between;
  gap: 1rem;
}

.iec61850-debug-page__progress-title {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 700;
}

.iec61850-debug-page__progress-detail {
  min-width: 0;
  overflow: hidden;
  color: var(--color-neutral-500);
  font-size: 0.75rem;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-debug-page__progress-track {
  position: relative;
  overflow: hidden;
  height: 0.375rem;
  border-radius: 999px;
  background: color-mix(in srgb, var(--runtime-accent) 12%, var(--color-neutral-200));
}

.iec61850-debug-page__progress-bar {
  position: absolute;
  inset: 0 auto 0 0;
  width: 36%;
  border-radius: inherit;
  background: color-mix(in srgb, var(--runtime-accent) 82%, var(--color-blue-500));
  animation: iec61850-progress-scan 1.15s ease-in-out infinite;
}

.iec61850-debug-page__summary {
  display: grid;
  flex: 0 0 auto;
  grid-template-columns: repeat(auto-fit, minmax(7rem, 1fr));
  gap: 0.5rem;
  padding: 0.75rem;
}

.iec61850-debug-page__metric {
  min-width: 0;
  padding: 0.625rem 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 78%, transparent);
}

.iec61850-debug-page__metric--wide {
  grid-column: span 2;
}

.iec61850-debug-page__metric-value {
  display: block;
  margin-top: 0.25rem;
  overflow: hidden;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-debug-page__workspace {
  display: grid;
  flex: 0 0 auto;
  height: min(42rem, calc(100vh - 14rem));
  min-height: 26rem;
  grid-template-columns: minmax(20rem, 0.9fr) minmax(24rem, 1.4fr);
  gap: 0.75rem;
}

.iec61850-debug-page__tree-panel,
.iec61850-debug-page__detail-panel,
.iec61850-debug-page__runtime,
.iec61850-debug-page__diagnostics {
  display: flex;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
}

.iec61850-debug-page__panel-header {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.75rem 1rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 72%, transparent);
}

.iec61850-debug-page__panel-count {
  color: var(--color-neutral-400);
  font-weight: 600;
  text-transform: none;
}

.iec61850-debug-page__empty,
.iec61850-debug-page__diagnostics-empty {
  margin: 1rem;
  padding: 1rem;
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 80%, transparent);
  border-radius: var(--radius-md);
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
  line-height: 1.4;
}

.iec61850-debug-page__tree {
  flex: 1 1 auto;
  min-height: 0;
  overflow: auto;
  padding: 0.5rem;
  outline: none;
}

.iec61850-debug-page__tree-row {
  display: flex;
  width: 100%;
  align-items: center;
  gap: 0.5rem;
  min-height: 2rem;
  padding-top: 0.25rem;
  padding-right: 0.625rem;
  padding-bottom: 0.25rem;
  border: 1px solid transparent;
  border-radius: var(--radius-sm);
  background: transparent;
  color: var(--color-neutral-700);
  cursor: pointer;
  text-align: left;
}

.iec61850-debug-page__tree-row:hover {
  border-color: color-mix(in srgb, var(--runtime-accent) 12%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--color-neutral-100) 68%, transparent);
}

.iec61850-debug-page__tree-row.is-selected {
  border-color: color-mix(in srgb, var(--runtime-accent) 28%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--runtime-accent) 10%, var(--color-white));
  color: var(--color-neutral-950);
}

.iec61850-debug-page__tree-row.is-active {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--runtime-accent) 18%, transparent);
}

.iec61850-debug-page__tree-toggle {
  flex: 0 0 0.875rem;
  color: var(--color-neutral-500);
  font-size: 0.75rem;
}

.iec61850-debug-page__tree-kind {
  flex: 0 0 auto;
  min-width: 4.5rem;
  color: color-mix(in srgb, var(--runtime-accent) 58%, var(--color-neutral-500));
  font-size: 0.625rem;
}

.iec61850-debug-page__tree-label {
  min-width: 0;
  overflow: hidden;
  color: inherit;
  font-size: var(--text-sm);
  font-weight: 600;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-debug-page__tree-value {
  margin-left: auto;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
}

.iec61850-debug-page__detail {
  flex: 1 1 auto;
  min-height: 0;
  overflow: auto;
}

.iec61850-debug-page__detail-heading {
  padding: 1rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 72%, transparent);
}

.iec61850-debug-page__detail-heading h2,
.iec61850-debug-page__detail-heading p,
.iec61850-debug-page__section-heading h3 {
  margin: 0;
}

.iec61850-debug-page__detail-heading h2 {
  color: var(--color-neutral-950);
  font-size: 1.125rem;
  line-height: 1.25;
}

.iec61850-debug-page__detail-heading p {
  margin-top: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.iec61850-debug-page__sections {
  display: grid;
  gap: 0.75rem;
  padding: 1rem;
}

.iec61850-debug-page__section {
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.iec61850-debug-page__section-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 74%, transparent);
}

.iec61850-debug-page__section-heading h3 {
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-debug-page__section-filter {
  display: inline-flex;
  min-width: 0;
  align-items: center;
  gap: 0.4rem;
  color: var(--color-neutral-600);
  cursor: pointer;
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: none;
}

.iec61850-debug-page__section-filter input {
  flex: 0 0 auto;
  width: 0.875rem;
  height: 0.875rem;
  margin: 0;
  accent-color: color-mix(in srgb, var(--runtime-accent) 82%, var(--color-blue-600));
}

.iec61850-debug-page__section-filter-count {
  color: var(--color-neutral-400);
  font-family: var(--font-mono);
  font-size: 0.6875rem;
  font-weight: 600;
}

.iec61850-debug-page__section-empty {
  padding: 0.75rem;
  color: var(--color-neutral-500);
  font-size: 0.75rem;
}

.iec61850-debug-page__property-list {
  display: grid;
  grid-template-columns: minmax(9rem, 0.42fr) minmax(0, 1fr);
  margin: 0;
}

.iec61850-debug-page__property-list dt,
.iec61850-debug-page__property-list dd {
  min-width: 0;
  margin: 0;
  padding: 0.5rem 0.75rem;
  border-top: 1px solid color-mix(in srgb, var(--color-neutral-200) 54%, transparent);
  font-size: 0.75rem;
  line-height: 1.35;
}

.iec61850-debug-page__property-list dt {
  color: var(--color-neutral-500);
}

.iec61850-debug-page__property-list dd {
  overflow-wrap: anywhere;
  color: var(--color-neutral-850, var(--color-neutral-900));
  font-family: var(--font-mono);
}

.iec61850-debug-page__property-action {
  display: inline-flex;
  max-width: 100%;
  align-items: center;
  padding: 0;
  border: 0;
  background: transparent;
  color: color-mix(in srgb, var(--runtime-accent) 78%, var(--color-neutral-900));
  cursor: pointer;
  font: inherit;
  text-align: left;
  text-decoration: underline;
  text-underline-offset: 0.18em;
}

.iec61850-debug-page__property-action:hover {
  color: color-mix(in srgb, var(--runtime-accent) 92%, var(--color-neutral-950));
}

.iec61850-debug-page__list-dialog {
  display: flex;
  box-sizing: border-box;
  height: 100%;
  max-height: 100%;
  min-height: 0;
  flex-direction: column;
  gap: 0.75rem;
  overflow: hidden;
}

.iec61850-debug-page__list-dialog-subtitle,
.iec61850-debug-page__list-dialog-count {
  margin: 0;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.iec61850-debug-page__list-dialog-count {
  font-weight: 700;
}

.iec61850-debug-page__list-dialog-items {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 0.75rem;
  min-height: 0;
  overflow-y: auto;
  padding-right: 0.25rem;
}

.iec61850-debug-page__list-dialog-item {
  flex: 0 0 auto;
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 82%, transparent);
}

.iec61850-debug-page__list-dialog-item h3,
.iec61850-debug-page__list-dialog-item p {
  margin: 0;
}

.iec61850-debug-page__list-dialog-item h3 {
  padding: 0.75rem 0.75rem 0;
  color: var(--color-neutral-950);
  font-size: 0.875rem;
  line-height: 1.25;
}

.iec61850-debug-page__list-dialog-item p {
  padding: 0.25rem 0.75rem 0.75rem;
  color: var(--color-neutral-500);
  font-size: 0.75rem;
}

.iec61850-debug-page__merge-dialog {
  display: flex;
  box-sizing: border-box;
  height: 100%;
  max-height: 100%;
  min-height: 0;
  flex-direction: column;
  gap: 0.75rem;
  overflow: hidden;
}

.iec61850-debug-page__merge-summary {
  display: grid;
  flex: 0 0 auto;
  grid-template-columns: repeat(auto-fit, minmax(8.5rem, 1fr));
  gap: 0.5rem;
}

.iec61850-debug-page__merge-metric {
  min-width: 0;
  padding: 0.625rem 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 82%, transparent);
}

.iec61850-debug-page__merge-metric span {
  display: block;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 700;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-debug-page__merge-metric strong {
  display: block;
  min-width: 0;
  margin-top: 0.25rem;
  overflow: hidden;
  color: var(--color-neutral-950);
  font-size: 0.875rem;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-debug-page__merge-action {
  display: flex;
  min-width: 0;
  align-items: end;
  justify-content: flex-start;
}

.iec61850-debug-page__merge-body {
  display: grid;
  flex: 1 1 auto;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 0.75rem;
  min-height: 0;
  overflow: hidden;
}

.iec61850-debug-page__merge-section {
  display: flex;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.iec61850-debug-page__merge-section h3 {
  flex: 0 0 auto;
  margin: 0;
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 74%, transparent);
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-debug-page__merge-section-note {
  flex: 0 0 auto;
  margin: 0;
  padding: 0.5rem 0.75rem 0;
  color: var(--color-neutral-500);
  font-size: 0.75rem;
}

.iec61850-debug-page__merge-list {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 0.5rem;
  min-height: 0;
  overflow-y: auto;
  padding: 0.5rem;
}

.iec61850-debug-page__merge-item {
  display: grid;
  flex: 0 0 auto;
  gap: 0.25rem;
  padding: 0.5rem 0.625rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 72%, transparent);
  border-radius: var(--radius-sm);
  background: color-mix(in srgb, var(--color-white) 82%, transparent);
  color: var(--color-neutral-700);
  font-size: 0.75rem;
}

.iec61850-debug-page__merge-item strong {
  overflow-wrap: anywhere;
  color: var(--color-neutral-950);
  font-size: 0.8125rem;
}

.iec61850-debug-page__merge-item span,
.iec61850-debug-page__merge-item small {
  overflow-wrap: anywhere;
  color: var(--color-neutral-500);
}

.iec61850-debug-page__merge-item code {
  overflow-wrap: anywhere;
  color: var(--color-neutral-850, var(--color-neutral-900));
  font-family: var(--font-mono);
}

.iec61850-debug-page__merge-item.is-miss {
  border-color: color-mix(in srgb, var(--color-amber-300) 54%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--color-amber-50) 68%, var(--color-white));
}

.iec61850-debug-page__runtime-summary {
  display: grid;
  flex: 0 0 auto;
  grid-template-columns: repeat(auto-fit, minmax(8rem, 1fr));
  gap: 0.5rem;
  padding: 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 72%, transparent);
}

.iec61850-debug-page__runtime-metric {
  min-width: 0;
  padding: 0.5rem 0.625rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 82%, transparent);
}

.iec61850-debug-page__runtime-metric span {
  display: block;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 700;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-debug-page__runtime-metric strong {
  display: block;
  margin-top: 0.25rem;
  color: var(--color-neutral-950);
  font-size: 0.875rem;
}

.iec61850-debug-page__runtime-body {
  display: grid;
  flex: 1 1 auto;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 0.75rem;
  min-height: 0;
  overflow: hidden;
  padding: 0.75rem;
}

.iec61850-debug-page__runtime {
  order: 6;
  flex: 0 0 auto;
  height: clamp(28rem, calc(100vh - 8rem), 48rem);
  min-height: 0;
}

.iec61850-debug-page__runtime-section {
  display: flex;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.iec61850-debug-page__runtime-section h3 {
  flex: 0 0 auto;
  margin: 0;
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 74%, transparent);
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-debug-page__runtime-note {
  flex: 0 0 auto;
  margin: 0;
  padding: 0.5rem 0.75rem 0;
  color: var(--color-neutral-500);
  font-size: 0.75rem;
}

.iec61850-debug-page__runtime-list {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
  flex-direction: column;
  gap: 0.5rem;
  overflow-y: auto;
  padding: 0.5rem;
}

.iec61850-debug-page__runtime-item {
  display: grid;
  flex: 0 0 auto;
  gap: 0.25rem;
  padding: 0.5rem 0.625rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 72%, transparent);
  border-radius: var(--radius-sm);
  background: color-mix(in srgb, var(--color-white) 82%, transparent);
  color: var(--color-neutral-700);
  font-size: 0.75rem;
}

.iec61850-debug-page__runtime-item strong {
  overflow-wrap: anywhere;
  color: var(--color-neutral-950);
  font-size: 0.8125rem;
}

.iec61850-debug-page__runtime-item span,
.iec61850-debug-page__runtime-item small {
  overflow-wrap: anywhere;
  color: var(--color-neutral-500);
}

.iec61850-debug-page__runtime-item code {
  overflow-wrap: anywhere;
  color: var(--color-neutral-850, var(--color-neutral-900));
  font-family: var(--font-mono);
}

.iec61850-debug-page__runtime-item.is-success {
  border-color: color-mix(in srgb, var(--color-emerald-300) 54%, var(--color-neutral-200));
}

.iec61850-debug-page__runtime-item.is-warning {
  border-color: color-mix(in srgb, var(--color-amber-300) 54%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--color-amber-50) 68%, var(--color-white));
}

.iec61850-debug-page__runtime-item.is-error {
  border-color: color-mix(in srgb, var(--color-red-300) 54%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--color-red-100) 58%, var(--color-white));
}

.iec61850-debug-page__diagnostics-list {
  flex: 1 1 auto;
  min-height: 0;
  overflow: auto;
  padding: 0.5rem;
}

.iec61850-debug-page__diagnostics {
  order: 5;
  flex: 0 0 auto;
  height: clamp(24rem, calc(100vh - 8rem), 44rem);
  min-height: 0;
}

.iec61850-debug-page__diagnostic {
  display: grid;
  grid-template-columns: minmax(10rem, 0.3fr) minmax(14rem, 1fr) minmax(12rem, 0.7fr);
  gap: 0.75rem;
  padding: 0.5rem 0.75rem;
  border-radius: var(--radius-sm);
  color: var(--color-neutral-700);
  font-size: 0.75rem;
}

.iec61850-debug-page__diagnostic + .iec61850-debug-page__diagnostic {
  margin-top: 0.25rem;
}

.iec61850-debug-page__diagnostic.is-error {
  background: color-mix(in srgb, var(--color-red-100) 72%, transparent);
  color: var(--color-red-800);
}

.iec61850-debug-page__diagnostic.is-warning {
  background: color-mix(in srgb, var(--color-amber-50) 78%, transparent);
  color: var(--color-amber-900);
}

.iec61850-debug-page__diagnostic.is-info {
  background: color-mix(in srgb, var(--color-blue-100) 48%, transparent);
  color: var(--color-blue-900);
}

.iec61850-debug-page__diagnostic-code,
.iec61850-debug-page__diagnostic-path {
  font-family: var(--font-mono);
  text-transform: none;
}

.iec61850-debug-page__diagnostic-path {
  min-width: 0;
  overflow: hidden;
  color: currentColor;
  opacity: 0.72;
  text-overflow: ellipsis;
  white-space: nowrap;
}

:global(.dark .iec61850-debug-page) {
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-debug-page__header),
:global(.dark .iec61850-debug-page__summary),
:global(.dark .iec61850-debug-page__tree-panel),
:global(.dark .iec61850-debug-page__detail-panel),
:global(.dark .iec61850-debug-page__runtime),
:global(.dark .iec61850-debug-page__progress),
:global(.dark .iec61850-debug-page__diagnostics) {
  border-color: color-mix(in srgb, var(--runtime-accent) 14%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 88%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-neutral-950) 86%, var(--color-neutral-900)));
  box-shadow:
    inset 0 1px 0 rgb(255 255 255 / 0.04),
    0 16px 30px rgb(0 0 0 / 0.18);
}

:global(.dark .iec61850-debug-page__title),
:global(.dark .iec61850-debug-page__detail-heading h2),
:global(.dark .iec61850-debug-page__nav-link) {
  color: var(--color-neutral-50);
}

:global(.dark .iec61850-debug-page__metric),
:global(.dark .iec61850-debug-page__section),
:global(.dark .iec61850-debug-page__list-dialog-item),
:global(.dark .iec61850-debug-page__merge-metric),
:global(.dark .iec61850-debug-page__merge-section),
:global(.dark .iec61850-debug-page__merge-item),
:global(.dark .iec61850-debug-page__runtime-metric),
:global(.dark .iec61850-debug-page__runtime-section),
:global(.dark .iec61850-debug-page__runtime-item),
:global(.dark .iec61850-debug-page__nav-link) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 58%, transparent);
}

:global(.dark .iec61850-debug-page__metric-value),
:global(.dark .iec61850-debug-page__progress-title),
:global(.dark .iec61850-debug-page__list-dialog-item h3),
:global(.dark .iec61850-debug-page__merge-metric strong),
:global(.dark .iec61850-debug-page__merge-item strong),
:global(.dark .iec61850-debug-page__merge-item code),
:global(.dark .iec61850-debug-page__runtime-metric strong),
:global(.dark .iec61850-debug-page__runtime-item strong),
:global(.dark .iec61850-debug-page__runtime-item code),
:global(.dark .iec61850-debug-page__property-list dd) {
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-debug-page__property-action) {
  color: color-mix(in srgb, var(--runtime-accent) 72%, var(--color-neutral-100));
}

:global(.dark .iec61850-debug-page__panel-header),
:global(.dark .iec61850-debug-page__detail-heading),
:global(.dark .iec61850-debug-page__section-heading),
:global(.dark .iec61850-debug-page__merge-section h3),
:global(.dark .iec61850-debug-page__runtime-summary),
:global(.dark .iec61850-debug-page__runtime-section h3) {
  border-color: var(--color-neutral-800);
}

:global(.dark .iec61850-debug-page__section-filter),
:global(.dark .iec61850-debug-page__section-empty),
:global(.dark .iec61850-debug-page__merge-metric span),
:global(.dark .iec61850-debug-page__merge-section-note),
:global(.dark .iec61850-debug-page__merge-item span),
:global(.dark .iec61850-debug-page__merge-item small),
:global(.dark .iec61850-debug-page__runtime-metric span),
:global(.dark .iec61850-debug-page__runtime-note),
:global(.dark .iec61850-debug-page__runtime-item span),
:global(.dark .iec61850-debug-page__runtime-item small) {
  color: var(--color-neutral-400);
}

:global(.dark .iec61850-debug-page__merge-item.is-miss) {
  border-color: color-mix(in srgb, var(--color-amber-800) 62%, var(--color-neutral-800));
  background: color-mix(in srgb, var(--color-amber-900) 18%, var(--color-neutral-950));
}

:global(.dark .iec61850-debug-page__runtime-item.is-success) {
  border-color: color-mix(in srgb, var(--color-emerald-900) 62%, var(--color-neutral-800));
}

:global(.dark .iec61850-debug-page__runtime-item.is-warning) {
  border-color: color-mix(in srgb, var(--color-amber-800) 62%, var(--color-neutral-800));
  background: color-mix(in srgb, var(--color-amber-900) 18%, var(--color-neutral-950));
}

:global(.dark .iec61850-debug-page__runtime-item.is-error) {
  border-color: color-mix(in srgb, var(--color-red-800) 62%, var(--color-neutral-800));
  background: color-mix(in srgb, var(--color-red-900) 18%, var(--color-neutral-950));
}

:global(.dark .iec61850-debug-page__empty),
:global(.dark .iec61850-debug-page__diagnostics-empty) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-400);
}

:global(.dark .iec61850-debug-page__tree-row) {
  color: var(--color-neutral-200);
}

:global(.dark .iec61850-debug-page__tree-row:hover) {
  border-color: color-mix(in srgb, var(--runtime-accent) 16%, var(--color-neutral-700));
  background: color-mix(in srgb, var(--color-neutral-800) 58%, transparent);
}

:global(.dark .iec61850-debug-page__tree-row.is-selected) {
  border-color: color-mix(in srgb, var(--runtime-accent) 34%, var(--color-neutral-700));
  background: color-mix(in srgb, var(--runtime-accent) 14%, var(--color-neutral-900));
  color: var(--color-neutral-50);
}

:global(.dark .iec61850-debug-page__property-list dt),
:global(.dark .iec61850-debug-page__property-list dd) {
  border-color: color-mix(in srgb, var(--color-neutral-800) 74%, transparent);
}

:global(.dark .iec61850-debug-page__diagnostic.is-error) {
  background: color-mix(in srgb, var(--color-red-900) 32%, transparent);
  color: var(--color-red-100);
}

:global(.dark .iec61850-debug-page__diagnostic.is-warning) {
  background: color-mix(in srgb, var(--color-amber-900) 26%, transparent);
  color: var(--color-amber-100);
}

:global(.dark .iec61850-debug-page__diagnostic.is-info) {
  background: color-mix(in srgb, var(--color-blue-900) 28%, transparent);
  color: var(--color-blue-100);
}

@keyframes iec61850-progress-scan {
  0% {
    transform: translateX(-100%);
  }

  50% {
    transform: translateX(120%);
  }

  100% {
    transform: translateX(280%);
  }
}

@media (max-width: 1023px) {
  .iec61850-debug-page__header {
    align-items: flex-start;
    flex-direction: column;
  }

  .iec61850-debug-page__summary,
  .iec61850-debug-page__workspace {
    grid-template-columns: minmax(0, 1fr);
  }

  .iec61850-debug-page__workspace {
    height: auto;
    min-height: 0;
  }

  .iec61850-debug-page__merge-body {
    grid-template-columns: minmax(0, 1fr);
  }

  .iec61850-debug-page__runtime {
    height: clamp(28rem, calc(100vh - 6rem), 44rem);
  }

  .iec61850-debug-page__runtime-body {
    grid-template-columns: minmax(0, 1fr);
  }

  .iec61850-debug-page__metric--wide {
    grid-column: span 1;
  }

  .iec61850-debug-page__tree-panel,
  .iec61850-debug-page__detail-panel {
    min-height: 22rem;
  }

  .iec61850-debug-page__diagnostics {
    height: clamp(24rem, calc(100vh - 6rem), 40rem);
  }
}
</style>
