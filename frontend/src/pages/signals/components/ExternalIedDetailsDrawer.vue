<template>
  <SlideOver
    :open="open"
    placement="right"
    title="IEC 61850 IED"
    :width-px="740"
    @close="emit('close')"
  >
    <div class="external-ied-details">
      <div class="external-ied-details__header">
        <p class="external-ied-details__endpoint">{{ endpointLabel }}</p>
        <span class="external-ied-details__status" :class="statusClass">
          <span class="external-ied-details__status-dot" aria-hidden="true"></span>
          {{ statusLabel }}
        </span>
      </div>

      <dl class="external-ied-details__facts">
        <div class="external-ied-details__fact">
          <dt>Status</dt>
          <dd>{{ statusLabel }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>Identity</dt>
          <dd>{{ identityLabel }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>Discovery</dt>
          <dd>{{ discoveryLabel }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>Planning</dt>
          <dd>{{ planningLabel }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>Verification</dt>
          <dd>{{ verificationLabel }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>Datasets</dt>
          <dd>{{ discoveryFacts.datasets }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>RCBs</dt>
          <dd>{{ discoveryFacts.rcbs }}</dd>
        </div>
        <div class="external-ied-details__fact">
          <dt>Signals</dt>
          <dd>{{ discoveryFacts.signals }}</dd>
        </div>
      </dl>

      <p v-if="lastError" class="external-ied-details__error">{{ lastError }}</p>

      <section class="external-ied-details__model" aria-label="IED discovery model">
        <div class="external-ied-details__model-header">
          <span>Reports</span>
          <span class="external-ied-details__model-count">{{ treeCountLabel }}</span>
        </div>
        <div class="external-ied-details__tree-toolbar">
          <input
            v-model="treeSearch"
            type="search"
            class="external-ied-details__tree-search"
            placeholder="Quick filter"
            autocomplete="off"
            spellcheck="false"
          >
          <button
            v-if="normalizedTreeSearch"
            type="button"
            class="external-ied-details__tree-clear"
            @click="treeSearch = ''"
          >
            Clear
          </button>
        </div>
        <div
          ref="treeViewportRef"
          class="external-ied-details__tree"
          role="tree"
          tabindex="0"
          aria-label="IED reports, datasets and signals"
          @scroll.passive="onTreeScroll"
          @keydown="onTreeRootKeydown"
        >
          <div v-if="treeLoading" class="external-ied-details__tree-empty">
            Loading discovery model...
          </div>
          <div v-else-if="treeError" class="external-ied-details__tree-empty">
            {{ treeError }}
          </div>
          <div v-else-if="treeVisibleCount === 0" class="external-ied-details__tree-empty">
            {{ normalizedTreeSearch ? "No matching model nodes." : "Run discovery to inspect model." }}
          </div>
          <div
            v-else
            class="external-ied-details__tree-spacer"
            :style="{ height: `${tree.totalHeight.value}px` }"
          >
            <div
              v-for="{ row, meta } in renderedTreeRows"
              :key="row.value"
              :ref="bindTreeItem(row.value)"
              class="external-ied-details__tree-row external-ied-details__tree-row--virtual"
              :class="{
                'is-active': meta.active,
                'is-selected': tree.isSelected(row.value),
                'is-match': meta.matched,
                'is-report-enabled': isReportEnabled(row),
                'is-leaf': row.isLeaf,
              }"
              role="treeitem"
              :aria-level="meta.depth + 1"
              :aria-expanded="row.isLeaf ? undefined : tree.isExpanded(row.value)"
              :aria-selected="tree.isSelected(row.value)"
              :tabindex="meta.active ? 0 : -1"
              :style="{
                height: `${meta.height}px`,
                transform: `translateY(${meta.top}px)`,
                paddingLeft: `${Math.max(8, (meta.depth + 1) * 14)}px`,
              }"
              @click="onTreeRowClick(row.value)"
              @dblclick="onTreeRowDoubleClick(row.value)"
            >
              <span v-if="row.isLeaf" class="external-ied-details__tree-toggle" aria-hidden="true">•</span>
              <span
                v-else
                class="external-ied-details__tree-toggle external-ied-details__tree-toggle--caret"
                role="button"
                tabindex="-1"
                :aria-label="tree.isExpanded(row.value) ? 'Collapse node' : 'Expand node'"
                @click.stop="onTreeToggleClick(row.value)"
              >
                {{ tree.isExpanded(row.value) ? "▾" : "▸" }}
              </span>
              <span class="external-ied-details__tree-kind">
                <template v-for="(part, partIndex) in highlightedTextParts(row.kind)" :key="`kind-${partIndex}`">
                  <mark v-if="part.match" class="external-ied-details__tree-highlight">{{ part.text }}</mark>
                  <template v-else>{{ part.text }}</template>
                </template>
              </span>
              <span class="external-ied-details__tree-label">
                <template v-for="(part, partIndex) in highlightedTextParts(row.label)" :key="`label-${partIndex}`">
                  <mark v-if="part.match" class="external-ied-details__tree-highlight">{{ part.text }}</mark>
                  <template v-else>{{ part.text }}</template>
                </template>
              </span>
              <span
                v-if="isReportEnabled(row)"
                class="external-ied-details__tree-enabled"
                title="Report enabled"
                aria-label="Report enabled"
              />
              <span v-if="treeRowValueText(row)" class="external-ied-details__tree-value">
                <template v-for="(part, partIndex) in highlightedTextParts(treeRowValueText(row))" :key="`value-${partIndex}`">
                  <mark v-if="part.match" class="external-ied-details__tree-highlight">{{ part.text }}</mark>
                  <template v-else>{{ part.text }}</template>
                </template>
                <span v-if="treeRowValueSource(row)" class="external-ied-details__tree-source">
                  {{ treeRowValueSource(row) }}
                </span>
                <span v-if="treeRowValueTimestamp(row)" class="external-ied-details__tree-time">
                  {{ treeRowValueTimestamp(row) }}
                </span>
              </span>
            </div>
          </div>
        </div>
      </section>

      <div class="external-ied-details__actions">
        <div class="external-ied-details__selected-report" :class="{ 'is-empty': !selectedReportRow }">
          <span class="external-ied-details__selected-report-label">
            {{ selectedReportRow?.label ?? "Select a report" }}
          </span>
          <span class="external-ied-details__selected-report-state">
            {{ selectedReportRow ? (selectedReportEnabled ? "Enabled" : "Disabled") : "No report selected" }}
          </span>
        </div>
        <UiButton
          class="external-ied-details__action-button"
          :variant="selectedReportEnabled ? 'danger' : 'success'"
          size="sm"
          :disabled="!selectedReportRow || !record || refreshing || manualReportBusy || manualGiBusy || record.status !== 'reachable'"
          @click="toggleSelectedReportEnabled"
        >
          {{ manualReportBusy ? "Applying..." : (selectedReportEnabled ? "Disable" : "Enable") }}
        </UiButton>
        <UiButton
          class="external-ied-details__action-button"
          variant="secondary"
          size="sm"
          :disabled="!selectedReportRow || !selectedReportEnabled || !record || refreshing || manualReportBusy || manualGiBusy || record.status !== 'reachable'"
          @click="sendSelectedReportGi"
        >
          {{ manualGiBusy ? "GI..." : "GI" }}
        </UiButton>
        <UiButton
          class="external-ied-details__refresh-button"
          variant="secondary"
          size="sm"
          :disabled="!record || refreshing || record.status !== 'reachable'"
          @click="emit('refreshDiscovery')"
        >
          {{ refreshing ? "Refreshing..." : "Refresh Discovery" }}
        </UiButton>
      </div>
    </div>
  </SlideOver>
</template>

<script setup lang="ts">
import { computed, nextTick, onUnmounted, ref, watch, type ComponentPublicInstance } from "vue"
import type { TreeviewNode } from "@/types/affinoTreeview"
import { useVirtualTreeviewController, type VirtualTreeviewRow } from "@affino/treeview-vue"

import { normalizeHttpError } from "@/api/http"
import SlideOver from "@/components/ui/SlideOver.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useExternalIedStore, type ExternalIedDiscoveryTree, type ExternalIedRecord } from "@/stores/externalIedStore"
import { useToastStore } from "@/stores/toastStore"

type NodeValue = string
type TreeKind = "LD" | "LN" | "Report" | "Dataset" | "Signal"

interface ModelTreeRow {
  value: NodeValue
  parent: NodeValue | null
  kind: TreeKind
  label: string
  valueLabel: string | null
  isLeaf: boolean
  text: string
  reportReference: string | null
  reportName: string | null
  reportKind: string | null
  datasetReference: string | null
}

type RenderedModelTreeRow = {
  row: ModelTreeRow
  meta: VirtualTreeviewRow<NodeValue>
}

type HighlightTextPart = {
  text: string
  match: boolean
}

const TREE_ROW_HEIGHT = 28
const TREE_OVERSCAN_ROWS = 10
const MANUAL_REPORT_LEASE_HEARTBEAT_MS = 5000

const props = defineProps<{
  open: boolean
  record: ExternalIedRecord | null
  discoveryTree: ExternalIedDiscoveryTree | null
  treeLoading?: boolean
  treeError?: string | null
  refreshing?: boolean
}>()

const emit = defineEmits<{
  (event: "close"): void
  (event: "refreshDiscovery"): void
}>()

const externalIedStore = useExternalIedStore()
const toastStore = useToastStore()
const treeSearch = ref("")
const selectedTreeValue = ref<NodeValue | null>(null)
const manualReportBusy = ref(false)
const manualGiBusy = ref(false)
const treeItemElements = new Map<NodeValue, HTMLElement>()
const treeViewportRef = ref<HTMLElement | null>(null)
let manualReportLeaseHeartbeatTimer: number | null = null
let treeViewportResizeObserver: ResizeObserver | null = null
const tree = useVirtualTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
  rowHeight: TREE_ROW_HEIGHT,
  overscan: TREE_OVERSCAN_ROWS,
  viewportHeight: 0,
})

const endpointLabel = computed(() => {
  if (!props.record) return "No endpoint selected"
  return `${props.record.ip}:${props.record.port}`
})

const statusLabel = computed(() => {
  const record = props.record
  if (!record) return "Not selected"
  if (record.discoveryState === "Failed" || record.discoveryState === "Cancelled") return "Failed"
  if (record.status === "offline") return "Offline"
  if (record.discoveryState === "Queued" || record.discoveryState === "Running" || record.discoveryState === "RetryWaiting" || record.discoveryState === "Stale") {
    return "Discovering"
  }
  if (record.status === "reachable" && (record.discoveryReadyForVerification || record.discoveryState === "Succeeded")) return "Ready"
  if (record.status === "reachable") return "MMS reachable"
  if (record.status === "expected" || record.status === "unknown") return "Checking"
  return "Not used"
})

const statusClass = computed(() => {
  const label = statusLabel.value
  if (label === "Ready" || label === "MMS reachable") return "external-ied-details__status--ready"
  if (label === "Discovering" || label === "Checking") return "external-ied-details__status--discovering"
  if (label === "Failed") return "external-ied-details__status--failed"
  return "external-ied-details__status--offline"
})

const identityLabel = computed(() => {
  const record = props.record
  if (!record) return "—"
  const vendorModel = [record.discoveryVendor, record.discoveryModel].filter(Boolean).join(" ")
  return vendorModel || record.discoveryDeviceIdentity || "—"
})

const discoveryLabel = computed(() => {
  const record = props.record
  if (!record) return "—"
  if (record.discoveryReadyForVerification || record.discoveryState === "Succeeded") return "✔"
  if (record.discoveryState === "Queued") return "Queued"
  if (record.discoveryState === "Running") return "Running"
  if (record.discoveryState === "RetryWaiting") return "Retry waiting"
  if (record.discoveryState === "Failed") return "Failed"
  if (record.discoveryState === "Cancelled") return "Cancelled"
  if (record.discoveryState === "Stale") return "Stale"
  return "Not started"
})

const planningLabel = computed(() => {
  const record = props.record
  if (!record) return "—"
  if (record.planningState === "Ready") return "✔"
  if (record.planningState === "Partial") return `${record.planningMatchedCount}/${record.planningMatchedCount + record.planningUnmatchedCount + record.planningAmbiguousCount} matched`
  if (record.planningState === "Running" || record.planningState === "Queued") return "Running"
  if (record.planningState === "WaitingForDiscovery") return "Waiting for discovery"
  if (record.planningState === "Failed") return "Failed"
  return record.discoveryReadyForVerification ? "Pending" : "—"
})

const verificationLabel = computed(() => "Not Started")

const lastError = computed(() => props.record?.planningLastError ?? props.record?.discoveryLastError ?? props.record?.lastError ?? null)

const normalizedTreeSearch = computed(() => treeSearch.value.trim())
const modelTreeRows = computed<ModelTreeRow[]>(() => buildModelTreeRows(props.discoveryTree))
const discoveryTreeCounts = computed(() => {
  const reports = props.discoveryTree?.reports ?? []
  const datasetReferences = new Set<string>()
  const signalReferences = new Set<string>()
  reports.forEach((report) => {
    const datasetReference = String(report.dataset?.reference ?? report.dataset_reference ?? "").trim()
    if (datasetReference) datasetReferences.add(datasetReference)
    report.dataset?.signals.forEach((signal) => {
      const signalReference = String(signal.reference ?? "").trim()
      if (signalReference) signalReferences.add(signalReference)
    })
  })
  return {
    datasets: datasetReferences.size,
    rcbs: reports.length,
    signals: signalReferences.size,
  }
})
const discoveryFacts = computed(() => {
  const treeCounts = discoveryTreeCounts.value
  const record = props.record
  return {
    datasets: resolveDiscoveryFactCount(record?.discoveryDatasets, treeCounts.datasets, 0),
    rcbs: resolveDiscoveryFactCount(record?.discoveryRcbs, treeCounts.rcbs, 0),
    signals: resolveDiscoveryFactCount(record?.discoveryModelSignals, treeCounts.signals, record?.signalIds.length ?? 0),
  }
})
const modelTreeNodes = computed<TreeviewNode<NodeValue>[]>(() => modelTreeRows.value.map(row => ({
  value: row.value,
  parent: row.parent,
  text: row.text,
})))
const rowByValue = computed(() => {
  const map = new Map<NodeValue, ModelTreeRow>()
  modelTreeRows.value.forEach(row => map.set(row.value, row))
  return map
})
const parentByValue = computed(() => {
  const map = new Map<NodeValue, NodeValue | null>()
  modelTreeRows.value.forEach(row => map.set(row.value, row.parent))
  return map
})
const childrenByParent = computed(() => {
  const map = new Map<NodeValue | null, NodeValue[]>()
  modelTreeRows.value.forEach((row) => {
    const children = map.get(row.parent) ?? []
    children.push(row.value)
    map.set(row.parent, children)
  })
  return map
})
const renderedTreeRows = computed<RenderedModelTreeRow[]>(() => {
  const rendered: RenderedModelTreeRow[] = []
  for (const meta of tree.visibleRows.value) {
    const row = rowByValue.value.get(meta.value)
    if (row) {
      rendered.push({ row, meta })
    }
  }
  return rendered
})
const selectedReportRow = computed(() => {
  const value = selectedTreeValue.value
  if (!value) return null
  const row = rowByValue.value.get(value)
  return row?.kind === "Report" ? row : null
})
const selectedReportEnabled = computed(() => (
  selectedReportRow.value && props.record
    ? externalIedStore.isManualReportEnabled(props.record.ip, props.record.port, selectedReportRow.value.reportReference)
    : false
))
const activeManualReportLeaseCount = computed(() => (
  props.record
    ? externalIedStore.getManualReportLeaseCountForEndpoint(props.record.ip, props.record.port)
    : 0
))
const treeVisibleCount = computed(() => {
  void tree.state.value
  return tree.getVisibleCount()
})
const treeMatchCount = computed(() => {
  void tree.state.value
  return tree.getSearchMatchCount()
})
const treeCountLabel = computed(() => {
  const total = modelTreeRows.value.length
  if (!total) return "0 nodes"
  if (normalizedTreeSearch.value) return `${treeMatchCount.value}/${total} matches`
  return `${props.discoveryTree?.reports.length ?? 0} report${(props.discoveryTree?.reports.length ?? 0) === 1 ? "" : "s"}`
})

watch(
  treeViewportRef,
  (element) => {
    treeViewportResizeObserver?.disconnect()
    treeViewportResizeObserver = null

    if (!element) {
      tree.setViewportHeight(0)
      return
    }

    const updateViewportHeight = () => {
      tree.setViewportHeight(element.clientHeight)
      tree.refreshWindow()
    }

    updateViewportHeight()
    if (typeof window !== "undefined") {
      window.requestAnimationFrame(updateViewportHeight)
    }

    if (typeof ResizeObserver !== "undefined") {
      treeViewportResizeObserver = new ResizeObserver(updateViewportHeight)
      treeViewportResizeObserver.observe(element)
    }
  },
  { flush: "post" },
)

onUnmounted(() => {
  stopManualReportLeaseHeartbeat()
  void releaseEndpointManualReportLeases()
  treeViewportResizeObserver?.disconnect()
  treeViewportResizeObserver = null
})

watch(modelTreeNodes, (nodes) => {
  tree.registerNodes(nodes)
  if (selectedTreeValue.value && !rowByValue.value.has(selectedTreeValue.value)) {
    selectedTreeValue.value = null
  }
  syncTreeExpansion()
}, { immediate: true })

watch(treeSearch, (query) => {
  tree.setSearchQuery(query)
  syncTreeExpansion()
  setTreeScrollTop(0)
})

watch(() => props.open, (open, wasOpen) => {
  if (!open) {
    if (wasOpen) {
      void releaseEndpointManualReportLeases()
    }
    treeSearch.value = ""
    selectedTreeValue.value = null
    return
  }
  syncTreeExpansion()
  const first = renderedTreeRows.value[0]?.row
  if (first) tree.focus(first.value)
})

watch(
  () => [
    props.open,
    props.record?.ip ?? "",
    props.record?.port ?? 102,
    activeManualReportLeaseCount.value,
  ] as const,
  () => {
    if (props.open && activeManualReportLeaseCount.value > 0) {
      startManualReportLeaseHeartbeat()
    } else {
      stopManualReportLeaseHeartbeat()
    }
  },
  { immediate: true },
)

function buildModelTreeRows(model: ExternalIedDiscoveryTree | null): ModelTreeRow[] {
  if (!model?.reports.length) return []
  const rows: ModelTreeRow[] = []
  const emittedGroups = new Set<string>()
  const hierarchyByReport = model.reports.map(resolveReportHierarchy)
  const logicalDeviceLabels = resolveLogicalDeviceLabels(hierarchyByReport.map(item => item.logicalDevice))
  const ldStats = new Map<string, { logicalNodes: Set<string>; reports: number }>()
  const lnStats = new Map<string, { reports: number }>()

  hierarchyByReport.forEach((hierarchy) => {
    const ld = ldStats.get(hierarchy.logicalDevice) ?? { logicalNodes: new Set<string>(), reports: 0 }
    ld.logicalNodes.add(hierarchy.logicalNode)
    ld.reports += 1
    ldStats.set(hierarchy.logicalDevice, ld)

    const lnKey = modelTreeGroupKey(hierarchy.logicalDevice, hierarchy.logicalNode)
    const ln = lnStats.get(lnKey) ?? { reports: 0 }
    ln.reports += 1
    lnStats.set(lnKey, ln)
  })

  model.reports.forEach((report, reportIndex) => {
    const hierarchy = hierarchyByReport[reportIndex]
    const ldValue = `ld:${hierarchy.logicalDevice}`
    const lnValue = `ln:${hierarchy.logicalDevice}:${hierarchy.logicalNode}`
    if (!emittedGroups.has(ldValue)) {
      emittedGroups.add(ldValue)
      const stats = ldStats.get(hierarchy.logicalDevice)
      rows.push({
        value: ldValue,
        parent: null,
        kind: "LD",
        label: logicalDeviceLabels.get(hierarchy.logicalDevice) ?? hierarchy.logicalDevice,
        valueLabel: stats ? `${stats.logicalNodes.size} LN · ${stats.reports} reports` : null,
        isLeaf: false,
        text: hierarchy.logicalDevice,
        reportReference: null,
        reportName: null,
        reportKind: null,
        datasetReference: null,
      })
    }
    if (!emittedGroups.has(lnValue)) {
      emittedGroups.add(lnValue)
      const stats = lnStats.get(modelTreeGroupKey(hierarchy.logicalDevice, hierarchy.logicalNode))
      rows.push({
        value: lnValue,
        parent: ldValue,
        kind: "LN",
        label: hierarchy.logicalNode,
        valueLabel: stats ? `${stats.reports} report${stats.reports === 1 ? "" : "s"}` : null,
        isLeaf: false,
        text: [hierarchy.logicalDevice, hierarchy.logicalNode].join(" "),
        reportReference: null,
        reportName: null,
        reportKind: null,
        datasetReference: null,
      })
    }
    const reportValue = `report:${reportIndex}:${report.reference}`
    const datasetReference = report.dataset?.reference ?? report.dataset_reference ?? null
    rows.push({
      value: reportValue,
      parent: lnValue,
      kind: "Report",
      label: resolveReportLabel(report),
      valueLabel: report.kind,
      isLeaf: false,
      text: [report.name, report.reference, report.kind, report.dataset_reference].filter(Boolean).join(" "),
      reportReference: report.reference,
      reportName: report.name,
      reportKind: report.kind,
      datasetReference,
    })
    const dataset = report.dataset
    const datasetValue = `dataset:${reportIndex}:${datasetReference ?? "missing"}`
    rows.push({
      value: datasetValue,
      parent: reportValue,
      kind: "Dataset",
      label: datasetReference ?? "Dataset not resolved",
      valueLabel: dataset ? `${dataset.signals.length} signals` : null,
      isLeaf: !dataset?.signals.length,
      text: [dataset?.reference, report.dataset_reference].filter(Boolean).join(" "),
      reportReference: null,
      reportName: null,
      reportKind: null,
      datasetReference: null,
    })
    dataset?.signals.forEach((signal, signalIndex) => {
      rows.push({
        value: `signal:${reportIndex}:${signalIndex}:${signal.reference}`,
        parent: datasetValue,
        kind: "Signal",
        label: signal.reference,
        valueLabel: signal.fc ?? null,
        isLeaf: true,
        text: [signal.reference, signal.fc].filter(Boolean).join(" "),
        reportReference: report.reference,
        reportName: null,
        reportKind: null,
        datasetReference: null,
      })
    })
  })
  return rows
}

function modelTreeGroupKey(logicalDevice: string, logicalNode: string): string {
  return `${logicalDevice}\u0000${logicalNode}`
}

function resolveLogicalDeviceLabels(logicalDevices: readonly string[]): Map<string, string> {
  const uniqueLogicalDevices = Array.from(new Set(logicalDevices.map(value => value.trim()).filter(Boolean)))
  const labels = new Map<string, string>()
  if (uniqueLogicalDevices.length <= 1) {
    uniqueLogicalDevices.forEach(value => labels.set(value, value))
    return labels
  }

  const prefix = trimLogicalDeviceDisplayPrefix(resolveCommonPrefix(uniqueLogicalDevices))
  uniqueLogicalDevices.forEach((logicalDevice) => {
    const candidate = prefix && logicalDevice.startsWith(prefix)
      ? logicalDevice.slice(prefix.length).trim()
      : logicalDevice
    labels.set(logicalDevice, candidate || logicalDevice)
  })
  return labels
}

function resolveCommonPrefix(values: readonly string[]): string {
  if (!values.length) return ""
  let prefix = values[0] ?? ""
  for (const value of values.slice(1)) {
    while (prefix && !value.startsWith(prefix)) {
      prefix = prefix.slice(0, -1)
    }
    if (!prefix) break
  }
  return prefix
}

function trimLogicalDeviceDisplayPrefix(prefix: string): string {
  let value = prefix
  while (value && /[A-Za-z]$/.test(value)) {
    value = value.slice(0, -1)
  }
  return value
}

function resolveReportHierarchy(report: ExternalIedDiscoveryTree["reports"][number]): { logicalDevice: string; logicalNode: string } {
  const candidates = [
    report.reference,
    report.dataset_reference,
    report.dataset?.reference,
    report.dataset?.signals[0]?.reference,
  ]
  for (const candidate of candidates) {
    const parsed = parseIec61850Hierarchy(candidate)
    if (parsed) {
      return parsed
    }
  }
  return {
    logicalDevice: "Unknown LD",
    logicalNode: "Unknown LN",
  }
}

function parseIec61850Hierarchy(reference: string | null | undefined): { logicalDevice: string; logicalNode: string } | null {
  let text = String(reference ?? "").trim()
  if (!text) return null
  if (text.includes("!")) {
    text = text.split("!", 2)[1] ?? text
  }
  const slashIndex = text.indexOf("/")
  if (slashIndex <= 0) return null
  const logicalDevice = text.slice(0, slashIndex).trim()
  const rest = text.slice(slashIndex + 1).trim()
  if (!logicalDevice || !rest) return null
  const logicalNode = rest.split(/[.$\/]/, 1)[0]?.trim()
  if (!logicalNode) return null
  return { logicalDevice, logicalNode }
}

function resolveReportLabel(report: ExternalIedDiscoveryTree["reports"][number]): string {
  const indexedName = indexedReportName(report.reference)
  if (indexedName) return indexedName
  const name = String(report.name ?? "").trim()
  if (name) return name
  const reference = String(report.reference ?? "").trim()
  if (!reference) return "Report"
  const parts = reference.split(/[\/.$]/).filter(Boolean)
  return parts[parts.length - 1] ?? reference
}

function indexedReportName(reference: string | null | undefined): string {
  const text = String(reference ?? "").trim()
  if (!text) return ""
  const item = text.includes(":") ? text.split(":", 2)[1] ?? text : text
  const parts = item.split("$")
  const reportFolderIndex = parts.findIndex(part => part === "BR" || part === "RP")
  if (reportFolderIndex >= 0) {
    return parts[reportFolderIndex + 1]?.trim() ?? ""
  }
  const pathParts = text.split(/[\/.]/).filter(Boolean)
  return pathParts[pathParts.length - 1]?.trim() ?? ""
}

function resolveDiscoveryFactCount(recordCount: number | null | undefined, treeCount: number, fallbackCount: number): number {
  if (Number.isFinite(recordCount) && Number(recordCount) > 0) {
    return Number(recordCount)
  }
  if (treeCount > 0) {
    return treeCount
  }
  return fallbackCount
}

function syncTreeExpansion() {
  modelTreeRows.value.forEach((row) => {
    if (!row.isLeaf) tree.collapse(row.value)
  })
  if (normalizedTreeSearch.value) {
    modelTreeRows.value.forEach((row) => {
      if (!row.isLeaf) tree.expand(row.value)
    })
  }
}

function bindTreeItem(value: NodeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element
      ? element
      : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLElement) {
      treeItemElements.set(value, resolved)
      return
    }
    treeItemElements.delete(value)
  }
}

function onTreeRowClick(value: NodeValue) {
  selectTreeRow(value)
}

function onTreeRowDoubleClick(value: NodeValue) {
  const row = rowByValue.value.get(value)
  selectTreeRow(value)
  if (row && !row.isLeaf) {
    tree.toggle(value)
    tree.refreshWindow()
  }
}

function selectTreeRow(value: NodeValue) {
  tree.focus(value)
  tree.clearSelection()
  tree.select(value)
  selectedTreeValue.value = value
}

function onTreeToggleClick(value: NodeValue) {
  const row = rowByValue.value.get(value)
  if (row && !row.isLeaf) {
    selectTreeRow(value)
    tree.toggle(value)
    tree.refreshWindow()
  }
}

function isReportEnabled(row: ModelTreeRow): boolean {
  if (row.kind !== "Report" || !props.record) return false
  return externalIedStore.isManualReportEnabled(props.record.ip, props.record.port, row.reportReference)
}

function treeRowValueText(row: ModelTreeRow): string | null {
  const state = signalRuntimeState(row)
  if (state) {
    return state.value ?? "—"
  }
  return row.valueLabel
}

function treeRowValueSource(row: ModelTreeRow): string | null {
  const reason = signalRuntimeState(row)?.reason
  if (!reason) return null
  if (reason.toLowerCase().includes("general")) return "GI"
  if (reason.toLowerCase().includes("data")) return "RPT"
  return reason.toUpperCase()
}

function treeRowValueTimestamp(row: ModelTreeRow): string | null {
  const timestamp = signalRuntimeState(row)?.timestamp
  if (!timestamp) return null
  const date = new Date(timestamp)
  if (!Number.isFinite(date.getTime())) return timestamp
  return date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" })
}

function signalRuntimeState(row: ModelTreeRow) {
  if (row.kind !== "Signal" || !props.record || !row.reportReference) return null
  return externalIedStore.getManualReportSignalState(props.record.ip, props.record.port, row.reportReference, row.label)
}

function highlightedTextParts(value: string | null | undefined): HighlightTextPart[] {
  const text = String(value ?? "")
  const query = normalizedTreeSearch.value
  if (!text || !query) return [{ text, match: false }]

  const lowerText = text.toLocaleLowerCase()
  const lowerQuery = query.toLocaleLowerCase()
  const parts: HighlightTextPart[] = []
  let cursor = 0
  while (cursor < text.length) {
    const matchIndex = lowerText.indexOf(lowerQuery, cursor)
    if (matchIndex < 0) {
      parts.push({ text: text.slice(cursor), match: false })
      break
    }
    if (matchIndex > cursor) {
      parts.push({ text: text.slice(cursor, matchIndex), match: false })
    }
    parts.push({ text: text.slice(matchIndex, matchIndex + query.length), match: true })
    cursor = matchIndex + query.length
  }
  return parts.length > 0 ? parts : [{ text, match: false }]
}

async function toggleSelectedReportEnabled() {
  const report = selectedReportRow.value
  const record = props.record
  if (!report?.reportReference || !record || manualReportBusy.value) return
  const targetEnabled = !selectedReportEnabled.value
  manualReportBusy.value = true
  try {
    await externalIedStore.setManualReportEnabled(
      record.ip,
      record.port,
      {
        report_reference: report.reportReference,
        report_name: report.reportName,
        report_kind: report.reportKind,
        dataset_reference: report.datasetReference,
      },
      targetEnabled,
    )
    toastStore.success(`Report ${targetEnabled ? "enabled" : "disabled"}.`)
  } catch (error) {
    toastStore.error(normalizeHttpError(error, `Failed to ${targetEnabled ? "enable" : "disable"} report`).message)
  } finally {
    manualReportBusy.value = false
  }
}

async function sendSelectedReportGi() {
  const report = selectedReportRow.value
  const record = props.record
  if (!report?.reportReference || !record || !selectedReportEnabled.value || manualGiBusy.value) return
  manualGiBusy.value = true
  try {
    await externalIedStore.sendManualReportGi(record.ip, record.port, report.reportReference)
    toastStore.success("GI completed.")
  } catch (error) {
    toastStore.error(normalizeHttpError(error, "Failed to run GI").message)
  } finally {
    manualGiBusy.value = false
  }
}

function startManualReportLeaseHeartbeat() {
  stopManualReportLeaseHeartbeat()
  if (typeof window === "undefined") return
  manualReportLeaseHeartbeatTimer = window.setInterval(() => {
    void heartbeatEndpointManualReportLeases()
  }, MANUAL_REPORT_LEASE_HEARTBEAT_MS)
}

function stopManualReportLeaseHeartbeat() {
  if (manualReportLeaseHeartbeatTimer === null || typeof window === "undefined") return
  window.clearInterval(manualReportLeaseHeartbeatTimer)
  manualReportLeaseHeartbeatTimer = null
}

async function heartbeatEndpointManualReportLeases() {
  const record = props.record
  if (!props.open || !record) return
  try {
    await externalIedStore.heartbeatManualReportLeasesForEndpoint(record.ip, record.port)
  } catch {
    stopManualReportLeaseHeartbeat()
  }
}

async function releaseEndpointManualReportLeases() {
  const record = props.record
  if (!record) return
  await externalIedStore.releaseManualReportLeasesForEndpoint(record.ip, record.port)
}

function onTreeRootKeydown(event: KeyboardEvent) {
  const active = tree.state.value.active
  const row = active ? rowByValue.value.get(active) : null
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      if (active) tree.focusNext()
      else tree.focusFirst()
      focusActiveTreeRow()
      return
    case "ArrowUp":
      event.preventDefault()
      if (active) tree.focusPrevious()
      else tree.focusLast()
      focusActiveTreeRow()
      return
    case "ArrowRight":
      if (!active || !row || row.isLeaf) return
      event.preventDefault()
      if (!tree.isExpanded(active)) {
        tree.expand(active)
        tree.refreshWindow()
        return
      }
      tree.focus(childrenByParent.value.get(active)?.[0] ?? active)
      focusActiveTreeRow()
      return
    case "ArrowLeft":
      if (!active) return
      event.preventDefault()
      if (row && !row.isLeaf && tree.isExpanded(active)) {
        tree.collapse(active)
        tree.refreshWindow()
        return
      }
      tree.focus(parentByValue.value.get(active) ?? active)
      focusActiveTreeRow()
      return
    case "Enter":
    case " ":
      if (!active) return
      event.preventDefault()
      onTreeRowClick(active)
      return
    default:
      return
  }
}

function focusActiveTreeRow() {
  void nextTick(() => {
    const active = tree.state.value.active
    if (!active) return
    if (treeItemElements.get(active)) {
      treeItemElements.get(active)?.focus({ preventScroll: true })
      return
    }
    tree.scrollToValue(active)
    tree.refreshWindow()
    syncTreeViewportScrollTop()
    void nextTick(() => treeItemElements.get(active)?.focus({ preventScroll: true }))
  })
}

function syncTreeViewportScrollTop() {
  const viewport = treeViewportRef.value
  if (!viewport) return
  if (Math.abs(viewport.scrollTop - tree.scrollTop.value) > 0.5) {
    viewport.scrollTop = tree.scrollTop.value
  }
}

function setTreeScrollTop(scrollTop: number) {
  tree.setScrollTop(scrollTop)
  tree.refreshWindow()
  syncTreeViewportScrollTop()
}

function onTreeScroll(event: Event) {
  const target = event.currentTarget
  if (target instanceof HTMLElement) {
    tree.setScrollTop(target.scrollTop)
    tree.refreshWindow()
  }
}
</script>

<style scoped>
.external-ied-details {
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  gap: 1rem;
  height: 100%;
  min-height: 0;
  padding: 1rem;
}

.external-ied-details__header {
  border-bottom: 1px solid var(--color-neutral-200);
  display: flex;
  flex: 0 0 auto;
  flex-direction: column;
  gap: 0.5rem;
  padding-bottom: 0.875rem;
}

.external-ied-details__endpoint {
  color: var(--color-neutral-900);
  font-family: var(--font-mono, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace);
  font-size: var(--text-base);
  font-weight: 700;
}

.external-ied-details__status {
  align-items: center;
  border-radius: var(--radius-pill);
  display: inline-flex;
  font-size: var(--text-xs);
  font-weight: 700;
  gap: 0.375rem;
  width: fit-content;
}

.external-ied-details__status-dot {
  border-radius: var(--radius-pill);
  height: 0.5rem;
  width: 0.5rem;
}

.external-ied-details__status--ready .external-ied-details__status-dot {
  background: var(--color-emerald-500);
}

.external-ied-details__status--discovering .external-ied-details__status-dot {
  background: var(--color-amber-400);
}

.external-ied-details__status--offline .external-ied-details__status-dot {
  background: var(--color-neutral-400);
}

.external-ied-details__status--failed .external-ied-details__status-dot {
  background: var(--color-rose-500);
}

.external-ied-details__facts {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  display: grid;
  flex: 0 0 auto;
  margin: 0;
  overflow: hidden;
}

.external-ied-details__fact {
  align-items: baseline;
  display: grid;
  gap: 0.75rem;
  grid-template-columns: minmax(6rem, 0.75fr) minmax(0, 1fr);
  padding: 0.625rem 0.75rem;
}

.external-ied-details__fact + .external-ied-details__fact {
  border-top: 1px solid var(--color-neutral-100);
}

.external-ied-details__fact dt {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.external-ied-details__fact dd {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 600;
  margin: 0;
  min-width: 0;
  overflow-wrap: anywhere;
}

.external-ied-details__error {
  background: color-mix(in srgb, var(--color-rose-50) 80%, var(--color-white));
  border: 1px solid var(--color-rose-200);
  border-radius: var(--radius-lg);
  color: var(--color-rose-700);
  flex: 0 0 auto;
  font-size: var(--text-xs);
  padding: 0.625rem;
}

.external-ied-details__model {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  min-height: 0;
  overflow: hidden;
}

.external-ied-details__model-header {
  align-items: center;
  border-bottom: 1px solid var(--color-neutral-100);
  color: var(--color-neutral-900);
  display: flex;
  flex: 0 0 auto;
  font-size: var(--text-sm);
  font-weight: 700;
  justify-content: space-between;
  padding: 0.625rem 0.75rem;
}

.external-ied-details__model-count {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
}

.external-ied-details__tree-toolbar {
  align-items: center;
  border-bottom: 1px solid var(--color-neutral-100);
  display: flex;
  flex: 0 0 auto;
  gap: 0.5rem;
  padding: 0.5rem 0.75rem;
}

.external-ied-details__tree-search {
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  color: var(--color-neutral-900);
  flex: 1 1 auto;
  font-size: var(--text-xs);
  min-width: 0;
  padding: 0.375rem 0.5rem;
}

.external-ied-details__tree-search:focus {
  border-color: var(--color-blue-500);
  box-shadow: 0 0 0 3px color-mix(in srgb, var(--color-blue-100) 75%, transparent);
  outline: none;
}

.external-ied-details__tree-clear {
  color: var(--color-neutral-500);
  flex: 0 0 auto;
  font-size: var(--text-xs);
  font-weight: 700;
}

.external-ied-details__tree-clear:hover {
  color: var(--color-neutral-900);
}

.external-ied-details__tree {
  flex: 1 1 auto;
  min-height: 8rem;
  overflow: auto;
  padding: 0.375rem;
}

.external-ied-details__tree:focus {
  outline: none;
}

.external-ied-details__tree-spacer {
  position: relative;
}

.external-ied-details__tree-empty {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  padding: 0.625rem;
}

.external-ied-details__tree-row {
  align-items: center;
  border-radius: var(--radius-md);
  color: var(--color-neutral-700);
  cursor: pointer;
  display: flex;
  gap: 0.375rem;
  min-height: 1.75rem;
  padding-bottom: 0.25rem;
  padding-right: 0.5rem;
  padding-top: 0.25rem;
  text-align: left;
  user-select: none;
  width: 100%;
}

.external-ied-details__tree-row--virtual {
  left: 0;
  position: absolute;
  top: 0;
  will-change: transform;
}

.external-ied-details__tree-row:hover,
.external-ied-details__tree-row.is-active {
  background: var(--color-neutral-50);
  color: var(--color-neutral-900);
}

.external-ied-details__tree-row.is-selected {
  background: color-mix(in srgb, var(--color-blue-100) 88%, var(--color-white));
  box-shadow:
    inset 3px 0 0 var(--color-blue-500),
    inset 0 0 0 1px color-mix(in srgb, var(--color-blue-300) 82%, transparent);
  color: var(--color-blue-950);
}

.external-ied-details__tree-row.is-selected:hover,
.external-ied-details__tree-row.is-selected.is-active {
  background: color-mix(in srgb, var(--color-blue-100) 92%, var(--color-white));
}

.external-ied-details__tree-row.is-report-enabled .external-ied-details__tree-label {
  color: var(--color-emerald-700);
}

.external-ied-details__tree-row.is-match .external-ied-details__tree-label {
  color: var(--color-blue-700);
  font-weight: 700;
}

.external-ied-details__tree-highlight {
  background: color-mix(in srgb, var(--color-yellow-300) 56%, var(--color-white));
  border-radius: var(--radius-highlight);
  color: var(--color-neutral-950);
  font: inherit;
  padding: 0 0.125rem;
}

.external-ied-details__tree-toggle {
  align-items: center;
  color: var(--color-neutral-400);
  display: inline-flex;
  flex: 0 0 1rem;
  font-size: var(--text-xs);
  height: 1rem;
  justify-content: center;
  line-height: 1;
  text-align: center;
}

.external-ied-details__tree-toggle--caret {
  border-radius: var(--radius-xs);
  color: var(--color-neutral-600);
  cursor: pointer;
}

.external-ied-details__tree-toggle--caret:hover {
  background: var(--color-neutral-100);
  color: var(--color-neutral-900);
}

.external-ied-details__tree-kind {
  color: var(--color-neutral-500);
  flex: 0 0 auto;
  font-size: var(--text-2xs);
  font-weight: 800;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.external-ied-details__tree-label {
  flex: 1 1 auto;
  font-size: var(--text-xs);
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.external-ied-details__tree-enabled {
  background: var(--color-emerald-500);
  border-radius: var(--radius-pill);
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-emerald-100) 80%, transparent);
  flex: 0 0 auto;
  height: 0.45rem;
  width: 0.45rem;
}

.external-ied-details__tree-value {
  align-items: center;
  color: var(--color-neutral-500);
  display: inline-flex;
  flex: 0 0 auto;
  font-size: var(--text-compact);
  gap: 0.25rem;
}

.external-ied-details__tree-source {
  background: color-mix(in srgb, var(--color-emerald-500) 16%, transparent);
  border: 1px solid color-mix(in srgb, var(--color-emerald-500) 36%, transparent);
  border-radius: var(--radius-xs);
  color: var(--color-emerald-700);
  font-size: var(--text-2xs);
  font-weight: 700;
  line-height: 1;
  padding: 0.125rem 0.25rem;
}

.external-ied-details__tree-time {
  color: var(--color-neutral-400);
  font-size: var(--text-2xs);
}

.external-ied-details__actions {
  align-items: center;
  border-top: 1px solid var(--color-neutral-200);
  display: grid;
  flex: 0 0 auto;
  gap: 0.5rem;
  grid-template-columns: minmax(0, 1fr) auto auto auto;
  padding-top: 0.875rem;
}

.external-ied-details__selected-report {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 0.125rem;
  min-width: 0;
}

.external-ied-details__selected-report.is-empty {
  color: var(--color-neutral-400);
}

.external-ied-details__selected-report-label {
  color: var(--color-neutral-800);
  font-size: var(--text-xs);
  font-weight: 700;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.external-ied-details__selected-report-state {
  color: var(--color-neutral-500);
  font-size: var(--text-compact);
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
}

.external-ied-details__action-button {
  min-width: 6.75rem;
}

.external-ied-details__refresh-button {
  min-width: 8.75rem;
}

:global(.dark .external-ied-details__header),
:global(.dark .external-ied-details__actions) {
  border-color: var(--color-neutral-800);
}

:global(.dark .external-ied-details__facts) {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-800);
}

:global(.dark .external-ied-details__fact + .external-ied-details__fact) {
  border-color: var(--color-neutral-900);
}

:global(.dark .external-ied-details__endpoint),
:global(.dark .external-ied-details__fact dd) {
  color: var(--color-neutral-100);
}

:global(.dark .external-ied-details__fact dt) {
  color: var(--color-neutral-400);
}

:global(.dark .external-ied-details__error) {
  background: color-mix(in srgb, var(--color-rose-950) 55%, var(--color-neutral-950));
  border-color: var(--color-rose-800);
  color: var(--color-rose-200);
}

:global(.dark .external-ied-details__model) {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-800);
}

:global(.dark .external-ied-details__model-header),
:global(.dark .external-ied-details__tree-toolbar) {
  border-color: var(--color-neutral-900);
}

:global(.dark .external-ied-details__model-header),
:global(.dark .external-ied-details__tree-row:hover),
:global(.dark .external-ied-details__tree-row.is-active) {
  color: var(--color-neutral-100);
}

:global(.dark .external-ied-details__model-count),
:global(.dark .external-ied-details__tree-empty),
:global(.dark .external-ied-details__tree-clear),
:global(.dark .external-ied-details__tree-kind),
:global(.dark .external-ied-details__tree-value),
:global(.dark .external-ied-details__tree-toggle) {
  color: var(--color-neutral-500);
}

:global(.dark .external-ied-details__tree-source) {
  background: color-mix(in srgb, var(--color-emerald-500) 18%, transparent);
  border-color: color-mix(in srgb, var(--color-emerald-500) 42%, transparent);
  color: var(--color-emerald-300);
}

:global(.dark .external-ied-details__tree-time) {
  color: var(--color-neutral-500);
}

:global(.dark .external-ied-details__tree-search) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark .external-ied-details__tree-search:focus) {
  border-color: var(--color-blue-500);
  box-shadow: 0 0 0 3px color-mix(in srgb, var(--color-blue-900) 45%, transparent);
}

:global(.dark .external-ied-details__tree-row) {
  color: var(--color-neutral-300);
}

:global(.dark .external-ied-details__tree-row:hover),
:global(.dark .external-ied-details__tree-row.is-active) {
  background: var(--color-neutral-900);
}

:global(.dark .external-ied-details__tree-row.is-selected) {
  background: color-mix(in srgb, var(--color-blue-900) 58%, var(--color-neutral-950));
  box-shadow:
    inset 3px 0 0 var(--color-blue-400),
    inset 0 0 0 1px color-mix(in srgb, var(--color-blue-700) 70%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .external-ied-details__tree-row.is-selected:hover),
:global(.dark .external-ied-details__tree-row.is-selected.is-active) {
  background: color-mix(in srgb, var(--color-blue-900) 68%, var(--color-neutral-950));
}

:global(.dark .external-ied-details__tree-row.is-report-enabled .external-ied-details__tree-label) {
  color: var(--color-emerald-200);
}

:global(.dark .external-ied-details__tree-row.is-match .external-ied-details__tree-label) {
  color: var(--color-blue-300);
}

:global(.dark .external-ied-details__tree-enabled) {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-emerald-950) 80%, transparent);
}

:global(.dark .external-ied-details__tree-highlight) {
  background: color-mix(in srgb, var(--color-yellow-300) 82%, var(--color-amber-500));
  color: var(--color-neutral-950);
}

:global(.dark .external-ied-details__tree-toggle--caret:hover) {
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark .external-ied-details__selected-report-label) {
  color: var(--color-neutral-100);
}

:global(.dark .external-ied-details__selected-report-state) {
  color: var(--color-neutral-400);
}

:global(.dark .external-ied-details__selected-report.is-empty .external-ied-details__selected-report-label),
:global(.dark .external-ied-details__selected-report.is-empty .external-ied-details__selected-report-state) {
  color: var(--color-neutral-500);
}
</style>
