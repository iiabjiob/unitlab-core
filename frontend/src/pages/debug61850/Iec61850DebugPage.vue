<script setup lang="ts">
import { computed, nextTick, onUnmounted, ref, shallowRef, watch, type ComponentPublicInstance } from "vue"
import { RouterLink } from "vue-router"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"

import UiButton from "@/components/ui/UiButton.vue"
import type { NormalizedSclModel } from "@/modules/scd-sld-core"
import {
  buildIec61850DebugTreeRows,
  type Iec61850DebugTreeRow,
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
  model: NormalizedSclModel
  parseDurationMs: number
}

const DEBUG_TREE_SIGNAL_ROW_LIMIT = 500

const fileInput = ref<HTMLInputElement | null>(null)
const fileName = ref<string | null>(null)
const contentHash = ref<string | null>(null)
const loading = ref(false)
const loadProgress = ref<LoadProgress | null>(null)
const readError = ref<string | null>(null)
const model = shallowRef<NormalizedSclModel | null>(null)
const selectedValue = ref<NodeValue | null>(null)
const pendingDefaultExpansion = ref(false)
let loadRequestId = 0
let activeParse: { worker: Worker; reject: (error: Error) => void } | null = null

const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const treeRows = computed<Iec61850DebugTreeRow[]>(() => (
  model.value
    ? buildIec61850DebugTreeRows(model.value, {
        maxSignalRowsPerCollection: DEBUG_TREE_SIGNAL_ROW_LIMIT,
        maxDetailRowsPerSection: DEBUG_TREE_SIGNAL_ROW_LIMIT,
      })
    : []
))

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

const childrenByParent = computed(() => {
  const map = new Map<NodeValue | null, NodeValue[]>()
  treeRows.value.forEach((row) => {
    const children = map.get(row.parent) ?? []
    children.push(row.value)
    map.set(row.parent, children)
  })
  return map
})

const expandedSet = computed(() => new Set(tree.state.value.expanded))
const visibleRows = computed(() => treeRows.value.filter(row => isNodeVisible(row.value)))
const selectedRow = computed(() => (
  selectedValue.value ? rowByValue.value.get(selectedValue.value) ?? null : treeRows.value[0] ?? null
))

const diagnostics = computed(() => model.value?.diagnostics ?? [])
const diagnosticsBySeverity = computed(() => ({
  error: diagnostics.value.filter(item => item.severity === "error").length,
  warning: diagnostics.value.filter(item => item.severity === "warning").length,
  info: diagnostics.value.filter(item => item.severity === "info").length,
}))

const stats = computed(() => ({
  sites: model.value?.substations.length ?? 0,
  voltageLevels: model.value?.substations.reduce((sum, site) => sum + site.voltageLevels.length, 0) ?? 0,
  bays: model.value?.substations.reduce(
    (sum, site) => sum + site.voltageLevels.reduce((vlSum, vl) => vlSum + vl.bays.length, 0),
    0,
  ) ?? 0,
  switchgears: model.value ? countSwitchgears(model.value) : 0,
  ieds: model.value?.ieds.length ?? 0,
  logicalDevices: model.value ? countLogicalDevices(model.value) : 0,
  dataSets: model.value ? countDataSets(model.value) : 0,
  reports: model.value?.reportSubscriptions.length ?? 0,
  reportSignals: model.value?.reportSubscriptions.reduce((sum, candidate) => sum + candidate.signalCount, 0) ?? 0,
}))

const statusLabel = computed(() => {
  if (loading.value) return loadProgress.value?.label ?? "Reading SCD"
  if (readError.value) return "Parse failed"
  if (!model.value) return "No SCD loaded"
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

async function onFileSelected(event: Event) {
  const input = event.target as HTMLInputElement | null
  const file = input?.files?.[0]
  if (!file) return

  const requestId = loadRequestId + 1
  loadRequestId = requestId
  terminateActiveParse()
  loading.value = true
  fileName.value = file.name
  contentHash.value = null
  model.value = null
  selectedValue.value = null
  loadProgress.value = {
    label: "Reading SCD",
    detail: `${file.name} · ${formatBytes(file.size)}`,
  }
  readError.value = null

  try {
    const xmlText = await file.text()
    if (requestId !== loadRequestId) return

    loadProgress.value = {
      label: "Starting parser",
      detail: `${file.name} · ${formatBytes(xmlText.length)}`,
    }
    const parsed = await parseScdInWorker(requestId, file.name, xmlText)
    if (requestId !== loadRequestId) return

    contentHash.value = parsed.contentHash
    model.value = parsed.model
    loadProgress.value = {
      label: "Building debug tree",
      detail: `Parsed in ${parsed.parseDurationMs}ms · capped signal rows at ${DEBUG_TREE_SIGNAL_ROW_LIMIT} per DataSet/report`,
    }
    pendingDefaultExpansion.value = true
  } catch (error) {
    if (error instanceof Error && error.message === "cancelled") return
    model.value = null
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

function parseScdInWorker(requestId: number, selectedFileName: string, xmlText: string): Promise<ParseResult> {
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
          detail: selectedFileName,
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
        model: message.model,
        parseDurationMs: message.parseDurationMs,
      })
    }

    worker.onerror = (event) => {
      cleanupActiveParse(worker)
      reject(new Error(event.message || "SCD worker failed"))
    }

    worker.postMessage({
      type: "parse",
      requestId,
      fileName: selectedFileName,
      xmlText,
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
      || row.kind === "ied"
      || row.kind === "access-point"
      || row.kind === "server"
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

function countSwitchgears(value: NormalizedSclModel): number {
  return value.substations.reduce((siteSum, site) => (
    siteSum + site.voltageLevels.reduce((voltageSum, voltageLevel) => (
      voltageSum + voltageLevel.bays.reduce((baySum, bay) => (
        baySum + bay.equipments.filter(equipment => equipment.kind === "breaker" || equipment.kind === "disconnector").length
      ), 0)
    ), 0)
  ), 0)
}

function countLogicalDevices(value: NormalizedSclModel): number {
  return value.ieds.reduce((sum, ied) => (
    sum + ied.accessPoints.reduce((apSum, accessPoint) => (
      apSum + (accessPoint.server?.logicalDevices.length ?? 0)
    ), 0)
  ), 0)
}

function countDataSets(value: NormalizedSclModel): number {
  return value.ieds.reduce((sum, ied) => (
    sum + ied.accessPoints.reduce((apSum, accessPoint) => (
      apSum + (accessPoint.server?.logicalDevices.reduce((ldSum, logicalDevice) => (
        ldSum + logicalDevice.logicalNodes.reduce((lnSum, logicalNode) => lnSum + logicalNode.dataSets.length, 0)
      ), 0) ?? 0)
    ), 0)
  ), 0)
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
        <UiButton variant="secondary" size="sm" :disabled="loading" @click="openFileDialog">
          {{ model ? "Load another SCD" : "Choose SCD" }}
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

    <section v-if="model" class="iec61850-debug-page__summary" aria-label="SCD summary">
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

        <div v-if="!model" class="iec61850-debug-page__empty">
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
              <h3>{{ section.title }}</h3>
              <dl class="iec61850-debug-page__property-list">
                <template v-for="row in section.rows" :key="`${section.title}:${row.label}`">
                  <dt>{{ row.label }}</dt>
                  <dd>{{ row.value }}</dd>
                </template>
              </dl>
            </section>
          </div>
        </article>
      </section>
    </main>

    <section v-if="model" class="iec61850-debug-page__diagnostics" aria-label="Parser diagnostics">
      <div class="iec61850-debug-page__panel-header">
        <span>Diagnostics</span>
        <span class="iec61850-debug-page__panel-count">
          {{ diagnosticsBySeverity.error }} errors · {{ diagnosticsBySeverity.warning }} warnings · {{ diagnosticsBySeverity.info }} info
        </span>
      </div>
      <div v-if="!diagnostics.length" class="iec61850-debug-page__diagnostics-empty">
        No parser diagnostics.
      </div>
      <div v-else class="iec61850-debug-page__diagnostics-list">
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
    </section>
  </div>
</template>

<style scoped>
.iec61850-debug-page {
  display: flex;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  gap: 0.75rem;
  padding: 1rem;
  overflow: hidden;
  color: var(--color-neutral-800);
}

.iec61850-debug-page__header,
.iec61850-debug-page__summary,
.iec61850-debug-page__tree-panel,
.iec61850-debug-page__detail-panel,
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
  flex: 1 1 auto;
  min-height: 0;
  grid-template-columns: minmax(20rem, 0.9fr) minmax(24rem, 1.4fr);
  gap: 0.75rem;
}

.iec61850-debug-page__tree-panel,
.iec61850-debug-page__detail-panel,
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
.iec61850-debug-page__section h3 {
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

.iec61850-debug-page__section h3 {
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 74%, transparent);
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
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

.iec61850-debug-page__diagnostics {
  flex: 0 0 min(12rem, 24%);
}

.iec61850-debug-page__diagnostics-list {
  overflow: auto;
  padding: 0.5rem;
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
:global(.dark .iec61850-debug-page__nav-link) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 58%, transparent);
}

:global(.dark .iec61850-debug-page__metric-value),
:global(.dark .iec61850-debug-page__progress-title),
:global(.dark .iec61850-debug-page__property-list dd) {
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-debug-page__panel-header),
:global(.dark .iec61850-debug-page__detail-heading),
:global(.dark .iec61850-debug-page__section h3) {
  border-color: var(--color-neutral-800);
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
  .iec61850-debug-page {
    overflow: auto;
  }

  .iec61850-debug-page__header {
    align-items: flex-start;
    flex-direction: column;
  }

  .iec61850-debug-page__summary,
  .iec61850-debug-page__workspace {
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
    flex-basis: auto;
    max-height: 18rem;
  }
}
</style>
