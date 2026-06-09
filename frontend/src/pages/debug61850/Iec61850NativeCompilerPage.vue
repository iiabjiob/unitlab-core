<script setup lang="ts">
import { computed, nextTick, onMounted, onUnmounted, ref, watch, type ComponentPublicInstance } from "vue"
import { RouterLink } from "vue-router"
import { useVirtualTreeviewController, type TreeviewNode, type VirtualTreeviewRow } from "@affino/treeview-vue"

import { Iec61850SclAPI, type Iec61850SclIedDiscoveryResponse, type Iec61850SclIedSummary, type Iec61850SclImportResponse, type Iec61850VirtualMmsServerState } from "@/api/iec61850Client.api"
import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import Iec61850DiagnosticsGrid from "./Iec61850DiagnosticsGrid.vue"
import type { ScdDiagnostic } from "@/modules/scd-sld-core"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import {
  buildIec61850IedDiscoveryTreeDocument,
  buildIec61850NativeTreeDocument,
  type Iec61850NativeTreeDocument,
  type Iec61850NativeTreeRow,
} from "./iec61850NativeTree"

type NodeValue = string
type CompileFailure = { iedName: string; message: string; code?: string }
type RenderedNativeTreeRow = { row: Iec61850NativeTreeRow; meta: VirtualTreeviewRow<NodeValue> }

const TREE_ROW_HEIGHT = 30
const TREE_OVERSCAN_ROWS = 12

const fileInput = ref<HTMLInputElement | null>(null)
const fileName = ref<string | null>(null)
const loading = ref(false)
const loadingPhase = ref<"discover" | "compile" | null>(null)
const error = ref<string | null>(null)
const importResponse = ref<Iec61850SclImportResponse | null>(null)
const discoveryResponse = ref<Iec61850SclIedDiscoveryResponse | null>(null)
const compiledImports = ref<Iec61850SclImportResponse[]>([])
const compileFailures = ref<CompileFailure[]>([])
const pendingFile = ref<File | null>(null)
const discoveredIeds = ref<Iec61850SclIedSummary[]>([])
const selectedIedNames = ref<string[]>([])
const iedSearchQuery = ref("")
const iedDialogOpen = ref(false)
const compileProgress = ref({ current: 0, total: 0, currentIed: "", failed: 0 })
let compileAbortController: AbortController | null = null
const activeCompileJobId = ref<string | null>(null)
const persistedImportsLoading = ref(false)
const virtualServerLoading = ref(false)
const virtualServerState = ref<Iec61850VirtualMmsServerState | null>(null)
const document = ref<Iec61850NativeTreeDocument | null>(null)
const selectedValue = ref<NodeValue | null>(null)
const treeSearch = ref("")
const treeSearchInputRef = ref<HTMLInputElement | null>(null)
const treeViewportRef = ref<HTMLDivElement | null>(null)
let treeViewportResizeObserver: ResizeObserver | null = null
const itemElements = new Map<NodeValue, HTMLButtonElement>()

const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()

const tree = useVirtualTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
  rowHeight: TREE_ROW_HEIGHT,
  overscan: TREE_OVERSCAN_ROWS,
  viewportHeight: 0,
})

const rows = computed(() => document.value?.rows ?? [])
const rowByValue = computed(() => {
  const map = new Map<NodeValue, Iec61850NativeTreeRow>()
  rows.value.forEach(row => map.set(row.value, row))
  return map
})
const treeNodes = computed<TreeviewNode<NodeValue>[]>(() => rows.value.map(row => ({
  value: row.value,
  parent: row.parent,
  text: [row.kind, row.label, row.meta, row.detail.title, row.detail.subtitle]
    .filter(Boolean)
    .join(" "),
})))
const parentByValue = computed(() => {
  const map = new Map<NodeValue, NodeValue | null>()
  rows.value.forEach(row => map.set(row.value, row.parent))
  return map
})
const expandedSet = computed(() => new Set(tree.state.value.expanded))
const childrenByParent = computed(() => {
  const map = new Map<NodeValue | null, NodeValue[]>()
  rows.value.forEach((row) => {
    const bucket = map.get(row.parent) ?? []
    bucket.push(row.value)
    map.set(row.parent, bucket)
  })
  return map
})
const renderedTreeRows = computed<RenderedNativeTreeRow[]>(() => {
  const rendered: RenderedNativeTreeRow[] = []
  for (const meta of tree.visibleRows.value) {
    const row = rowByValue.value.get(meta.value)
    if (row) rendered.push({ row, meta })
  }
  return rendered
})
const selectedRow = computed(() => selectedValue.value ? rowByValue.value.get(selectedValue.value) ?? null : rows.value[0] ?? null)
const stats = computed(() => document.value?.stats ?? {
  logicalDevices: 0,
  logicalNodes: 0,
  dataSets: 0,
  reports: 0,
  signals: 0,
  errors: 0,
  warnings: 0,
})
const statusLabel = computed(() => {
  if (persistedImportsLoading.value) return "Loading saved SCL imports"
  if (loading.value) return loadingPhase.value === "discover" ? "Reading SCD devices" : "Compiling SCD on backend"
  if (compiledImports.value.length) {
    const failed = compileFailures.value.length ? ` · ${compileFailures.value.length} failed` : ""
    return `${compiledImports.value.length} compiled IEDs · ${stats.value.logicalDevices} LD · ${stats.value.reports} reports · ${stats.value.signals} leaves${failed}`
  }
  if (discoveryResponse.value) return `${discoveryResponse.value.ieds.length} discovered IEDs · runtime models not compiled`
  if (error.value) return "Backend compile failed"
  return "No SCD loaded"
})
const selectedIedSet = computed(() => new Set(selectedIedNames.value))
const normalizedIedSearchQuery = computed(() => iedSearchQuery.value.trim().toLowerCase())
const visibleDiscoveredIeds = computed(() => {
  const query = normalizedIedSearchQuery.value
  if (!query) return discoveredIeds.value
  return discoveredIeds.value.filter(ied => ied.name.toLowerCase().includes(query))
})
const selectedIedCountLabel = computed(() => `${selectedIedNames.value.length}/${discoveredIeds.value.length} selected`)
const visibleIedCountLabel = computed(() => normalizedIedSearchQuery.value ? `${visibleDiscoveredIeds.value.length}/${discoveredIeds.value.length} visible` : `${discoveredIeds.value.length} devices`)
const compileProgressPercent = computed(() => {
  if (!compileProgress.value.total) return 0
  return Math.round((compileProgress.value.current / compileProgress.value.total) * 100)
})
const compileProgressLabel = computed(() => {
  if (loadingPhase.value !== "compile") return ""
  const total = compileProgress.value.total
  if (!total) return "Preparing backend batch compile"
  const percent = compileProgressPercent.value
  const base = `${percent}% · ${compileProgress.value.current}/${total}`
  if (compileProgress.value.currentIed) return `${base} · ${compileProgress.value.currentIed}`
  return `${base} · queued`
})
const normalizedTreeSearchQuery = computed(() => treeSearch.value.trim())
const treeSearchMatchCount = computed(() => {
  void tree.state.value
  return tree.getSearchMatchCount()
})
const treeVisibleCount = computed(() => {
  void tree.state.value
  return tree.getVisibleCount()
})
const treeCountLabel = computed(() => {
  if (!rows.value.length) return ""
  if (normalizedTreeSearchQuery.value) return `${treeSearchMatchCount.value}/${rows.value.length} matches`
  return `${rows.value.length} nodes`
})
const treeLoading = computed(() => persistedImportsLoading.value || loadingPhase.value === "discover")
const treeLoadingLabel = computed(() => persistedImportsLoading.value ? "Loading saved SCL imports" : "Reading SCD device index")
const virtualServerEndpointLabel = computed(() => {
  const state = virtualServerState.value
  if (!state?.running || !state.host || !state.port) return "Virtual MMS server stopped"
  return `${state.selected_ied ?? "IED"} · ${state.host}:${state.port}`
})
const canStartVirtualServer = computed(() => Boolean(compiledImports.value[0]?.import_id && !virtualServerLoading.value))

const nativeDiagnostics = computed<ScdDiagnostic[]>(() => {
  const rows: ScdDiagnostic[] = []
  for (const response of compiledImports.value) {
    for (const diagnostic of response.diagnostics) {
      rows.push({
        severity: normalizeDiagnosticSeverity(diagnostic.severity),
        stage: "adapter",
        code: diagnostic.code,
        message: diagnostic.message,
        sourcePath: [response.selected_ied, diagnostic.logicalDeviceInst, diagnostic.logicalNodeName, diagnostic.dataSetName, diagnostic.reportControlName, diagnostic.memberReference].filter(Boolean).join("/"),
        sourceId: response.selected_ied,
      })
    }
  }
  for (const failure of compileFailures.value) {
    rows.push({
      severity: "error",
      stage: "adapter",
      code: failure.code ?? "SCL_BATCH_IMPORT_FAILED",
      message: failure.message,
      sourcePath: failure.iedName,
      sourceId: failure.iedName,
    })
  }
  return rows
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
    window.requestAnimationFrame(updateViewportHeight)

    if (typeof ResizeObserver !== "undefined") {
      treeViewportResizeObserver = new ResizeObserver(updateViewportHeight)
      treeViewportResizeObserver.observe(element)
    }
  },
  { flush: "post" },
)

watch(treeNodes, (nodes) => {
  tree.registerNodes(nodes)
  if (!nodes.length) {
    selectedValue.value = null
    return
  }
  expandDefaults()
  selectNode(nodes[0].value)
}, { immediate: true })

watch(treeSearch, (query) => {
  tree.setSearchQuery(query)
  setTreeScrollTop(0)
})

watch(() => tree.state.value.active, async (active) => {
  if (!active || isTreeSearchFocused()) return
  await nextTick()
  if (focusNodeElement(active)) return
  tree.scrollToValue(active)
  tree.refreshWindow()
  syncTreeViewportScrollTop()
  await nextTick()
  focusNodeElement(active)
})


function normalizeDiagnosticSeverity(severity: string): ScdDiagnostic["severity"] {
  return severity === "error" || severity === "warning" || severity === "info" ? severity : "info"
}

function openFileDialog() {
  fileInput.value?.click()
}

async function onFileSelected(event: Event) {
  const input = event.target as HTMLInputElement | null
  const file = input?.files?.[0]
  if (!file) return

  loading.value = true
  loadingPhase.value = "discover"
  error.value = null
  fileName.value = file.name
  pendingFile.value = file
  importResponse.value = null
  discoveryResponse.value = null
  compiledImports.value = []
  compileFailures.value = []
  document.value = null
  selectedValue.value = null
  discoveredIeds.value = []
  selectedIedNames.value = []
  iedSearchQuery.value = ""
  compileProgress.value = { current: 0, total: 0, currentIed: "", failed: 0 }

  try {
    await workspaceStore.bootstrap()
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) throw new Error("Active workspace is not selected")
    const discovered = await Iec61850SclAPI.discoverIeds(workspaceId, file)
    discoveryResponse.value = discovered
    document.value = buildIec61850IedDiscoveryTreeDocument(discovered, file.name)
    discoveredIeds.value = discovered.ieds
    selectedIedNames.value = discovered.ieds.map(ied => ied.name)
    if (!discovered.ieds.length) {
      throw new Error("SCD does not contain selectable IED devices")
    }
    toastStore.success(`SCD indexed: ${discovered.ieds.length} IED${discovered.ieds.length === 1 ? "" : "s"}`)
  } catch (caught) {
    pendingFile.value = null
    error.value = caught instanceof Error ? caught.message : "SCL IED discovery failed"
    toastStore.error(error.value)
  } finally {
    loading.value = false
    loadingPhase.value = null
    if (input) input.value = ""
  }
}

function toggleIed(name: string) {
  const selected = selectedIedSet.value
  selectedIedNames.value = selected.has(name)
    ? selectedIedNames.value.filter(item => item !== name)
    : [...selectedIedNames.value, name]
}

function selectAllIeds() {
  selectedIedNames.value = discoveredIeds.value.map(ied => ied.name)
}

function selectVisibleIeds() {
  const next = new Set(selectedIedNames.value)
  visibleDiscoveredIeds.value.forEach(ied => next.add(ied.name))
  selectedIedNames.value = Array.from(next)
}

function clearIedSelection() {
  selectedIedNames.value = []
}

function openIedDialog() {
  if (!pendingFile.value || !discoveredIeds.value.length || loading.value) return
  iedDialogOpen.value = true
}

function closeIedDialog() {
  if (loadingPhase.value === "compile") return
  iedDialogOpen.value = false
}

async function cancelCompile() {
  const workspaceId = workspaceStore.activeWorkspaceId
  const jobId = activeCompileJobId.value
  if (workspaceId && jobId) {
    try {
      await Iec61850SclAPI.cancelSclImportBatchJob(workspaceId, jobId)
    } catch {
      // The local abort below still releases the UI if the cancel request fails.
    }
  }
  compileAbortController?.abort()
}

async function compileSelectedIeds() {
  const file = pendingFile.value
  if (!file || !selectedIedNames.value.length || loading.value) return

  loading.value = true
  loadingPhase.value = "compile"
  error.value = null
  compileAbortController = new AbortController()
  try {
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) throw new Error("Active workspace is not selected")
    const selectedNames = [...selectedIedNames.value]
    compileFailures.value = []
    compileProgress.value = { current: 0, total: selectedNames.length, currentIed: "", failed: 0 }
    const started = await Iec61850SclAPI.startSclImportBatchJob(workspaceId, file, selectedNames)
    activeCompileJobId.value = started.job_id
    const batch = await pollCompileJob(workspaceId, started.job_id, compileAbortController.signal)
    const responses = batch.imports
    const failures = batch.failures.map(failure => ({
      iedName: failure.selected_ied,
      code: failure.code,
      message: `${failure.code}: ${failure.message}`,
    }))
    compiledImports.value = responses
    compileFailures.value = failures
    showCompiledImports(responses)
    if (failures.length) {
      error.value = `Compiled ${responses.length}/${selectedNames.length}. ${failures.length} failed.`
      toastStore.warning(error.value)
      return
    }
    iedDialogOpen.value = false
    pendingFile.value = null
    toastStore.success(`SCL compiled: ${responses.length} IED${responses.length === 1 ? "" : "s"}`)
  } catch (caught) {
    const aborted = compileAbortController?.signal.aborted
    error.value = aborted ? "SCL batch compile cancelled." : (caught instanceof Error ? caught.message : "SCL backend import failed")
    if (aborted) toastStore.info(error.value)
    else toastStore.error(error.value)
  } finally {
    activeCompileJobId.value = null
    compileAbortController = null
    loading.value = false
    loadingPhase.value = null
  }
}


async function pollCompileJob(workspaceId: number, jobId: string, signal: AbortSignal) {
  while (true) {
    if (signal.aborted) throw new DOMException("aborted", "AbortError")
    const status = await Iec61850SclAPI.getSclImportBatchJob(workspaceId, jobId)
    compileProgress.value = {
      current: status.current,
      total: status.total,
      currentIed: status.current_ied ?? "",
      failed: status.failures.length,
    }
    compileFailures.value = status.failures.map(failure => ({
      iedName: failure.selected_ied,
      code: failure.code,
      message: `${failure.code}: ${failure.message}`,
    }))
    if (status.status === "completed" || status.status === "failed" || status.status === "cancelled") {
      return status
    }
    await sleepWithCancel(1000, signal)
  }
}

function sleepWithCancel(ms: number, signal: AbortSignal) {
  return new Promise<void>((resolve, reject) => {
    const cleanup = () => signal.removeEventListener("abort", abort)
    const timer = window.setTimeout(() => {
      cleanup()
      resolve()
    }, ms)
    const abort = () => {
      window.clearTimeout(timer)
      cleanup()
      reject(new DOMException("aborted", "AbortError"))
    }
    signal.addEventListener("abort", abort, { once: true })
  })
}

function showCompiledImports(responses: Iec61850SclImportResponse[]) {
  importResponse.value = responses[0] ?? null
  document.value = responses.length ? buildCombinedNativeTreeDocument(responses) : null
  treeSearch.value = ""
}

function buildCombinedNativeTreeDocument(responses: Iec61850SclImportResponse[]): Iec61850NativeTreeDocument {
  const combined: Iec61850NativeTreeDocument = {
    stats: {
      logicalDevices: 0,
      logicalNodes: 0,
      dataSets: 0,
      reports: 0,
      signals: 0,
      errors: 0,
      warnings: 0,
    },
    rows: [],
  }

  for (const response of responses) {
    const source = buildIec61850NativeTreeDocument(response)
    combined.stats.logicalDevices += source.stats.logicalDevices
    combined.stats.logicalNodes += source.stats.logicalNodes
    combined.stats.dataSets += source.stats.dataSets
    combined.stats.reports += source.stats.reports
    combined.stats.signals += source.stats.signals
    combined.stats.errors += source.stats.errors
    combined.stats.warnings += source.stats.warnings

    const prefix = `ied:${response.selected_ied}:`
    combined.rows.push(...source.rows.map(row => ({
      ...row,
      value: `${prefix}${row.value}`,
      parent: row.parent === null ? null : `${prefix}${row.parent}`,
    })))
  }

  return combined
}

function expandDefaults() {
  for (const row of rows.value) {
    if (row.kind === "import" || row.kind.endsWith("group")) {
      tree.expand(row.value)
    }
  }
}

function selectNode(value: NodeValue) {
  selectedValue.value = value
  tree.focus(value)
  tree.select(value)
}

function onRowClick(row: Iec61850NativeTreeRow) {
  selectNode(row.value)
  if (!row.isLeaf) tree.toggle(row.value)
}

function bindItemElement(value: NodeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element ? element : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLButtonElement) {
      itemElements.set(value, resolved)
      return
    }
    itemElements.delete(value)
  }
}

function focusNodeElement(value: NodeValue): boolean {
  const element = itemElements.get(value)
  if (!element) return false
  element.focus({ preventScroll: true })
  element.scrollIntoView({ block: "nearest" })
  return true
}

function isTreeSearchFocused(): boolean {
  return globalThis.document?.activeElement === treeSearchInputRef.value
}

function clearTreeSearch() {
  treeSearch.value = ""
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
  }
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
    case " ": {
      if (!active) return
      event.preventDefault()
      const row = rowByValue.value.get(active)
      if (row) onRowClick(row)
      return
    }
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
  if (firstChild) tree.focus(firstChild)
}

function collapseOrFocusParent(value: NodeValue) {
  const row = rowByValue.value.get(value)
  if (row && !row.isLeaf && tree.isExpanded(value)) {
    tree.collapse(value)
    return
  }
  const parent = parentByValue.value.get(value)
  if (parent) tree.focus(parent)
}

function isSelected(value: NodeValue): boolean {
  return selectedValue.value === value
}

function isExpanded(value: NodeValue): boolean {
  return tree.isExpanded(value)
}

async function refreshVirtualServerState() {
  try {
    await workspaceStore.bootstrap()
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) return
    virtualServerState.value = await Iec61850SclAPI.virtualMmsServerState(workspaceId)
  } catch {
    virtualServerState.value = null
  }
}

async function startVirtualServer() {
  const selectedImport = compiledImports.value[0]
  if (!selectedImport?.import_id || virtualServerLoading.value) return
  virtualServerLoading.value = true
  error.value = null
  try {
    await workspaceStore.bootstrap()
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) throw new Error("Active workspace is not selected")
    virtualServerState.value = await Iec61850SclAPI.startVirtualMmsServer(workspaceId, {
      import_id: selectedImport.import_id,
      host: "127.0.0.1",
      port: 1102,
    })
    toastStore.success(`Virtual MMS server started: ${virtualServerState.value.host}:${virtualServerState.value.port}`)
  } catch (caught) {
    error.value = caught instanceof Error ? caught.message : "Virtual MMS server start failed"
    toastStore.error(error.value)
  } finally {
    virtualServerLoading.value = false
  }
}

async function stopVirtualServer() {
  if (virtualServerLoading.value) return
  virtualServerLoading.value = true
  error.value = null
  try {
    await workspaceStore.bootstrap()
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) throw new Error("Active workspace is not selected")
    virtualServerState.value = await Iec61850SclAPI.stopVirtualMmsServer(workspaceId)
    toastStore.info("Virtual MMS server stopped")
  } catch (caught) {
    error.value = caught instanceof Error ? caught.message : "Virtual MMS server stop failed"
    toastStore.error(error.value)
  } finally {
    virtualServerLoading.value = false
  }
}

async function loadPersistedImports() {
  persistedImportsLoading.value = true
  error.value = null
  try {
    await workspaceStore.bootstrap()
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) return
    const response = await Iec61850SclAPI.listSclImports(workspaceId, 200)
    const latestHash = response.imports[0]?.source_hash ?? null
    const savedImports = latestHash ? response.imports.filter(item => item.source_hash === latestHash) : []
    compiledImports.value = savedImports
    compileFailures.value = []
    discoveryResponse.value = null
    discoveredIeds.value = []
    selectedIedNames.value = []
    pendingFile.value = null
    if (savedImports.length) {
      fileName.value = savedImports[0]?.source_filename ?? null
      showCompiledImports(savedImports)
    }
    await refreshVirtualServerState()
  } catch (caught) {
    error.value = caught instanceof Error ? caught.message : "Saved SCL imports loading failed"
  } finally {
    persistedImportsLoading.value = false
  }
}

function shortHash(value: string | null | undefined): string {
  return value ? value.slice(0, 12) : ""
}

onMounted(() => {
  void loadPersistedImports()
})

onUnmounted(() => {
  compileAbortController?.abort()
  treeViewportResizeObserver?.disconnect()
})
</script>

<template>
  <div class="iec61850-native-page">
    <header class="iec61850-native-page__header">
      <div>
        <p class="iec61850-native-page__eyebrow">IEC 61850 backend compiler</p>
        <h1 class="iec61850-native-page__title">Native SCL Tree</h1>
        <p class="iec61850-native-page__status">{{ statusLabel }}</p>
      </div>
      <div class="iec61850-native-page__actions">
        <RouterLink class="iec61850-native-page__nav-link" to="/61850-debug">Frontend parser</RouterLink>
        <RouterLink class="iec61850-native-page__nav-link" to="/61850-debug/client">MMS client test</RouterLink>
        <UiButton v-if="pendingFile && discoveredIeds.length" variant="primary" size="sm" :disabled="loading" @click="openIedDialog">
          Compile selected IEDs
        </UiButton>
        <UiButton
          v-if="virtualServerState?.running"
          variant="secondary"
          size="sm"
          :disabled="virtualServerLoading"
          @click="stopVirtualServer"
        >
          Stop virtual MMS
        </UiButton>
        <UiButton
          v-else-if="compiledImports.length"
          variant="primary"
          size="sm"
          :disabled="!canStartVirtualServer"
          @click="startVirtualServer"
        >
          Start virtual MMS
        </UiButton>
        <UiButton variant="secondary" size="sm" :disabled="loading || persistedImportsLoading" @click="openFileDialog">
          {{ importResponse || discoveryResponse ? "Choose another SCD" : "Choose SCD" }}
        </UiButton>
        <input
          ref="fileInput"
          class="iec61850-native-page__file-input"
          type="file"
          accept=".scd,.ssd,.xml,text/xml,application/xml"
          autocomplete="off"
          @change="onFileSelected"
        >
      </div>
    </header>

    <section v-if="error" class="iec61850-native-page__alert">{{ error }}</section>

    <section class="iec61850-native-page__virtual-server" aria-label="Virtual MMS server state">
      <span>{{ virtualServerEndpointLabel }}</span>
      <small v-if="virtualServerState?.running">Use IEDScout endpoint {{ virtualServerState.host }}:{{ virtualServerState.port }}</small>
      <small v-else>Start a localhost MMS server from the compiled model before connecting IEDScout.</small>
    </section>


    <section v-if="compileFailures.length" class="iec61850-native-page__failure-report" aria-label="IED compile failures">
      <div class="iec61850-native-page__failure-header">
        <span>Failed IED compiles</span>
        <strong>{{ compileFailures.length }}</strong>
      </div>
      <div class="iec61850-native-page__failure-list">
        <div v-for="failure in compileFailures" :key="failure.iedName" class="iec61850-native-page__failure-item">
          <span>{{ failure.iedName }}</span>
          <small>{{ failure.message }}</small>
        </div>
      </div>
    </section>

    <section class="iec61850-native-page__summary" aria-label="Native compiler summary">
      <div class="iec61850-native-page__metric"><span>LD</span><strong>{{ stats.logicalDevices }}</strong></div>
      <div class="iec61850-native-page__metric"><span>LN</span><strong>{{ stats.logicalNodes }}</strong></div>
      <div class="iec61850-native-page__metric"><span>DataSets</span><strong>{{ stats.dataSets }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Reports</span><strong>{{ stats.reports }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Signals</span><strong>{{ stats.signals }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Errors</span><strong>{{ stats.errors }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Warnings</span><strong>{{ stats.warnings }}</strong></div>
      <div class="iec61850-native-page__metric iec61850-native-page__metric--wide"><span>Hash</span><strong>{{ shortHash(importResponse?.source_hash) }}</strong></div>
    </section>

    <main class="iec61850-native-page__workspace">
      <section class="iec61850-native-page__tree-panel" aria-label="Native compiler tree">
        <div class="iec61850-native-page__panel-header">
          <span>{{ compiledImports.length ? "Compiled MMS model" : "SCD IED index" }}</span>
          <span>{{ treeCountLabel }}</span>
        </div>
        <div v-if="treeLoading" class="iec61850-native-page__skeleton" aria-live="polite" aria-busy="true">
          <div class="iec61850-native-page__skeleton-header">{{ treeLoadingLabel }}</div>
          <div v-for="index in 12" :key="`tree-skeleton-${index}`" class="iec61850-native-page__skeleton-row" :style="{ width: `${92 - (index % 5) * 8}%` }">
            <span></span>
          </div>
        </div>
        <div v-else-if="!document" class="iec61850-native-page__empty">
          Upload SCD to inspect the lightweight IED index first. Full MMS runtime models are compiled only on explicit request.
        </div>
        <template v-else>
          <div class="iec61850-native-page__tree-toolbar">
            <input
              ref="treeSearchInputRef"
              v-model="treeSearch"
              class="iec61850-native-page__search"
              type="search"
              :placeholder="compiledImports.length ? 'Search compiled model' : 'Search SCD IED index'"
              autocomplete="off"
              spellcheck="false"
            >
            <button
              v-if="normalizedTreeSearchQuery"
              type="button"
              class="iec61850-native-page__tree-search-clear"
              @click="clearTreeSearch"
            >
              Clear
            </button>
          </div>
          <div
            ref="treeViewportRef"
            class="iec61850-native-page__tree"
            role="tree"
            tabindex="0"
            aria-label="Compiled IEC 61850 model"
            @scroll.passive="onTreeScroll"
            @keydown="onTreeRootKeydown"
          >
            <div v-if="treeVisibleCount === 0" class="iec61850-native-page__tree-empty">
              {{ normalizedTreeSearchQuery ? "No matching IEC 61850 nodes." : "No IEC 61850 nodes." }}
            </div>
            <div
              v-else
              class="iec61850-native-page__tree-spacer"
              :style="{ height: `${tree.totalHeight.value}px` }"
            >
              <button
                v-for="{ row, meta } in renderedTreeRows"
                :key="row.value"
                :ref="bindItemElement(row.value)"
                type="button"
                class="iec61850-native-page__tree-row"
                :class="[`is-${row.kind}`, { 'is-selected': isSelected(row.value), 'is-active': meta.active, 'is-match': meta.matched }]"
                role="treeitem"
                :aria-level="meta.depth + 1"
                :aria-expanded="row.isLeaf ? undefined : isExpanded(row.value)"
                :aria-selected="isSelected(row.value)"
                :tabindex="meta.active ? 0 : -1"
                :style="{ height: `${meta.height}px`, transform: `translateY(${meta.top}px)` }"
                @click="onRowClick(row)"
              >
                <span class="iec61850-native-page__indent" :style="{ width: `${meta.depth * 14}px` }"></span>
                <span class="iec61850-native-page__toggle">{{ row.isLeaf ? "•" : (isExpanded(row.value) ? "▾" : "▸") }}</span>
                <span class="iec61850-native-page__kind">{{ row.kind.replace(/-.*/, "") }}</span>
                <span class="iec61850-native-page__label">{{ row.label }}</span>
                <span v-if="row.meta" class="iec61850-native-page__meta">{{ row.meta }}</span>
              </button>
            </div>
          </div>
        </template>
      </section>

      <aside class="iec61850-native-page__detail" aria-label="Selected node detail">
        <div class="iec61850-native-page__panel-header">
          <span>Details</span>
          <span>{{ fileName ?? "" }}</span>
        </div>
        <div v-if="treeLoading" class="iec61850-native-page__detail-skeleton" aria-hidden="true">
          <div class="iec61850-native-page__skeleton-line is-title"></div>
          <div class="iec61850-native-page__skeleton-line"></div>
          <div class="iec61850-native-page__skeleton-line is-short"></div>
          <div class="iec61850-native-page__skeleton-pairs">
            <span v-for="index in 8" :key="`detail-skeleton-${index}`"></span>
          </div>
        </div>
        <div v-else-if="!selectedRow" class="iec61850-native-page__empty">No node selected.</div>
        <template v-else>
          <h2>{{ selectedRow.detail.title }}</h2>
          <p>{{ selectedRow.detail.subtitle }}</p>
          <dl>
            <template v-for="row in selectedRow.detail.rows" :key="`${row.label}:${row.value}`">
              <dt>{{ row.label }}</dt>
              <dd>{{ row.value || "—" }}</dd>
            </template>
          </dl>
        </template>
      </aside>
    </main>


    <section v-if="nativeDiagnostics.length" class="iec61850-native-page__diagnostics" aria-label="Native compiler diagnostics">
      <div class="iec61850-native-page__panel-header">
        <span>Diagnostics</span>
        <span>{{ nativeDiagnostics.length }}</span>
      </div>
      <Iec61850DiagnosticsGrid :diagnostics="nativeDiagnostics" />
    </section>


    <UiModal :open="iedDialogOpen" title="Select IED devices" max-width="3xl" :content-scroll="true" @close="closeIedDialog">
      <div class="iec61850-native-page__ied-dialog">
        <div class="iec61850-native-page__ied-dialog-toolbar">
          <span>{{ selectedIedCountLabel }} · {{ visibleIedCountLabel }}</span>
          <button type="button" :disabled="loading" @click="selectVisibleIeds">Select visible</button>
          <button type="button" :disabled="loading" @click="selectAllIeds">Select all</button>
          <button type="button" :disabled="loading" @click="clearIedSelection">Clear</button>
        </div>
        <input
          v-model="iedSearchQuery"
          class="iec61850-native-page__ied-search"
          type="search"
          placeholder="Filter IED devices"
          autocomplete="off"
          spellcheck="false"
          :disabled="loading"
        >
        <div v-if="loadingPhase === 'compile'" class="iec61850-native-page__compile-progress" aria-live="polite">
          <div class="iec61850-native-page__compile-progress-copy">
            <span>{{ compileProgressLabel }}</span>
            <span v-if="compileProgress.failed">{{ compileProgress.failed }} failed</span>
          </div>
          <div class="iec61850-native-page__compile-progress-track" :class="{ 'is-indeterminate': loadingPhase === 'compile' && compileProgress.current === 0 }" aria-hidden="true">
            <span :style="{ width: `${compileProgressPercent}%` }"></span>
          </div>
        </div>

        <div v-if="compileFailures.length" class="iec61850-native-page__modal-failures">
          <strong>{{ compileFailures.length }} failed</strong>
          <div>
            <span v-for="failure in compileFailures" :key="failure.iedName">{{ failure.iedName }}</span>
          </div>
        </div>
        <div class="iec61850-native-page__ied-list">
          <label v-for="ied in visibleDiscoveredIeds" :key="ied.name" class="iec61850-native-page__ied-option">
            <input
              type="checkbox"
              :checked="selectedIedSet.has(ied.name)"
              :disabled="loading"
              @change="toggleIed(ied.name)"
            >
            <span>{{ ied.name }}</span>
            <small>{{ ied.accessPointCount }} AP</small>
          </label>
        </div>
      </div>
      <template #footer>
        <UiButton v-if="loadingPhase === 'compile'" variant="secondary" size="sm" @click="cancelCompile">Cancel compile</UiButton>
        <UiButton v-else variant="secondary" size="sm" :disabled="loading" @click="closeIedDialog">Cancel</UiButton>
        <UiButton variant="primary" size="sm" :disabled="loading || !selectedIedNames.length" @click="compileSelectedIeds">
          {{ loading ? "Compiling..." : `Compile ${selectedIedNames.length} IED` }}
        </UiButton>
      </template>
    </UiModal>
  </div>
</template>

<style scoped>
.iec61850-native-page {
  display: flex;
  min-height: calc(100vh - 32px);
  flex-direction: column;
  gap: 12px;
  padding: 16px;
  color: #172033;
  background: #f4f6f8;
}

.iec61850-native-page__header,
.iec61850-native-page__summary,
.iec61850-native-page__failure-report,
.iec61850-native-page__virtual-server,
.iec61850-native-page__imports,
.iec61850-native-page__workspace,
.iec61850-native-page__tree-panel,
.iec61850-native-page__detail,
.iec61850-native-page__diagnostics,
.iec61850-native-page__alert {
  border: 1px solid #d7dde6;
  border-radius: 8px;
  background: #ffffff;
}

.iec61850-native-page__header {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 16px;
  padding: 14px 16px;
}

.iec61850-native-page__eyebrow,
.iec61850-native-page__status {
  margin: 0;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__title {
  margin: 2px 0 4px;
  font-size: 22px;
  line-height: 1.2;
}

.iec61850-native-page__actions {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-end;
  justify-content: flex-end;
  gap: 8px;
}

.iec61850-native-page__nav-link {
  color: #24589a;
  font-size: 13px;
  text-decoration: none;
}

.iec61850-native-page__ied-field {
  display: grid;
  gap: 3px;
  color: #637083;
  font-size: 11px;
}

.iec61850-native-page__ied-field input,
.iec61850-native-page__search {
  height: 32px;
  border: 1px solid #cbd4df;
  border-radius: 6px;
  padding: 0 10px;
  background: #ffffff;
  color: #172033;
}

.iec61850-native-page__file-input {
  display: none;
}

.iec61850-native-page__alert {
  border-color: #efb4b4;
  padding: 10px 12px;
  color: #9f2424;
  background: #fff5f5;
}


.iec61850-native-page__virtual-server {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 12px;
  color: #172033;
  font-size: 13px;
}

.iec61850-native-page__virtual-server span {
  font-weight: 700;
}

.iec61850-native-page__virtual-server small {
  min-width: 0;
  color: #637083;
  overflow-wrap: anywhere;
}

.iec61850-native-page__failure-report {
  display: grid;
  gap: 8px;
  border-color: #efc6a4;
  padding: 10px 12px;
  background: #fff8f0;
}

.iec61850-native-page__failure-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: #8a4b12;
  font-size: 13px;
}

.iec61850-native-page__failure-list {
  display: grid;
  max-height: 180px;
  gap: 4px;
  overflow: auto;
}

.iec61850-native-page__failure-item {
  display: grid;
  grid-template-columns: minmax(160px, 0.35fr) minmax(0, 1fr);
  gap: 10px;
  border-top: 1px solid #f0d4bd;
  padding-top: 4px;
  font-size: 12px;
}

.iec61850-native-page__failure-item span {
  font-weight: 600;
}

.iec61850-native-page__failure-item small {
  min-width: 0;
  overflow-wrap: anywhere;
  color: #8a4b12;
}

.iec61850-native-page__modal-failures {
  display: grid;
  gap: 6px;
  border: 1px solid #efc6a4;
  border-radius: 6px;
  padding: 8px 10px;
  color: #8a4b12;
  background: #fff8f0;
  font-size: 12px;
}

.iec61850-native-page__modal-failures div {
  display: flex;
  max-height: 72px;
  flex-wrap: wrap;
  gap: 4px 8px;
  overflow: auto;
}

.iec61850-native-page__summary {
  display: grid;
  grid-template-columns: repeat(8, minmax(90px, 1fr));
  gap: 1px;
  overflow: hidden;
}

.iec61850-native-page__metric {
  display: grid;
  gap: 4px;
  padding: 10px 12px;
  background: #fbfcfe;
}

.iec61850-native-page__metric span {
  color: #637083;
  font-size: 11px;
}

.iec61850-native-page__metric strong {
  overflow: hidden;
  font-size: 17px;
  text-overflow: ellipsis;
  white-space: nowrap;
}


.iec61850-native-page__imports {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  padding: 10px 12px;
}

.iec61850-native-page__import-tab {
  height: 30px;
  border: 1px solid #cbd4df;
  border-radius: 6px;
  padding: 0 10px;
  background: #ffffff;
  color: #172033;
  font: inherit;
}

.iec61850-native-page__import-tab:hover,
.iec61850-native-page__import-tab.is-selected {
  border-color: #7da7d9;
  background: #e9f1fb;
}

.iec61850-native-page__ied-dialog {
  display: grid;
  gap: 12px;
}

.iec61850-native-page__ied-dialog-toolbar {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 8px;
  color: #637083;
  font-size: 13px;
}

.iec61850-native-page__ied-dialog-toolbar span {
  margin-right: auto;
}

.iec61850-native-page__ied-dialog-toolbar button {
  height: 28px;
  border: 1px solid #cbd4df;
  border-radius: 6px;
  padding: 0 9px;
  background: #ffffff;
  color: #172033;
}


.iec61850-native-page__ied-search {
  width: 100%;
  height: 32px;
  border: 1px solid #cbd4df;
  border-radius: 6px;
  padding: 0 10px;
  background: #ffffff;
  color: #172033;
}

.iec61850-native-page__compile-progress {
  display: grid;
  gap: 6px;
  border: 1px solid #d7dde6;
  border-radius: 6px;
  padding: 8px 10px;
  background: #fbfcfe;
}

.iec61850-native-page__compile-progress-copy {
  display: flex;
  justify-content: space-between;
  gap: 10px;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__compile-progress-track {
  height: 6px;
  overflow: hidden;
  border-radius: 999px;
  background: #e2e7ee;
}

.iec61850-native-page__compile-progress-track span {
  display: block;
  height: 100%;
  border-radius: inherit;
  background: #2f6fb2;
  transition: width 160ms ease;
}

.iec61850-native-page__compile-progress-track.is-indeterminate span {
  width: 35% !important;
  animation: iec61850-native-progress-sweep 1.2s ease-in-out infinite;
}

@keyframes iec61850-native-progress-sweep {
  0% { transform: translateX(-120%); }
  100% { transform: translateX(320%); }
}

.iec61850-native-page__ied-list {
  display: grid;
  max-height: 420px;
  gap: 6px;
  overflow: auto;
}

.iec61850-native-page__ied-option {
  display: grid;
  grid-template-columns: 20px minmax(0, 1fr) auto;
  align-items: center;
  gap: 8px;
  border: 1px solid #e2e7ee;
  border-radius: 6px;
  padding: 8px 10px;
  color: #172033;
}

.iec61850-native-page__ied-option span {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-native-page__ied-option small {
  color: #637083;
}

.iec61850-native-page__skeleton,
.iec61850-native-page__detail-skeleton {
  display: grid;
  gap: 10px;
  padding: 12px;
}

.iec61850-native-page__skeleton-header {
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__skeleton-row,
.iec61850-native-page__skeleton-line,
.iec61850-native-page__skeleton-pairs span {
  position: relative;
  overflow: hidden;
  border-radius: 6px;
  background: #e6ebf1;
}

.iec61850-native-page__skeleton-row::after,
.iec61850-native-page__skeleton-line::after,
.iec61850-native-page__skeleton-pairs span::after {
  position: absolute;
  inset: 0;
  background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.65), transparent);
  animation: iec61850-native-skeleton 1.15s ease-in-out infinite;
  content: "";
  transform: translateX(-100%);
}

.iec61850-native-page__skeleton-row {
  height: 28px;
}

.iec61850-native-page__skeleton-row span {
  display: block;
  width: 18px;
  height: 100%;
  border-right: 1px solid rgba(255, 255, 255, 0.55);
}

.iec61850-native-page__skeleton-line {
  height: 14px;
}

.iec61850-native-page__skeleton-line.is-title {
  width: 52%;
  height: 22px;
}

.iec61850-native-page__skeleton-line.is-short {
  width: 68%;
}

.iec61850-native-page__skeleton-pairs {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px 12px;
  margin-top: 8px;
}

.iec61850-native-page__skeleton-pairs span {
  height: 16px;
}

@keyframes iec61850-native-skeleton {
  100% { transform: translateX(100%); }
}

.iec61850-native-page__workspace {
  display: grid;
  min-height: 560px;
  grid-template-columns: minmax(360px, 1.1fr) minmax(320px, 0.9fr);
  gap: 0;
  overflow: hidden;
}

.iec61850-native-page__tree-panel,
.iec61850-native-page__detail {
  min-width: 0;
  border: 0;
  border-radius: 0;
}

.iec61850-native-page__detail {
  border-left: 1px solid #d7dde6;
}

.iec61850-native-page__panel-header {
  display: flex;
  min-height: 42px;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  border-bottom: 1px solid #d7dde6;
  padding: 0 12px;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__empty {
  padding: 18px;
  color: #637083;
  font-size: 13px;
}

.iec61850-native-page__search {
  width: calc(100% - 24px);
  margin: 10px 12px;
}

.iec61850-native-page__tree-toolbar {
  display: flex;
  flex: 0 0 auto;
  gap: 8px;
  border-bottom: 1px solid #e2e7ee;
  padding: 8px 10px;
}

.iec61850-native-page__tree-toolbar .iec61850-native-page__search {
  flex: 1 1 auto;
  min-width: 0;
}

.iec61850-native-page__tree-search-clear {
  height: 32px;
  border: 1px solid #cbd4df;
  border-radius: 6px;
  padding: 0 10px;
  background: #fbfcfe;
  color: #637083;
  font: inherit;
  font-size: 12px;
  font-weight: 700;
}

.iec61850-native-page__tree-search-clear:hover {
  border-color: #7da7d9;
  color: #172033;
}

.iec61850-native-page__tree {
  height: 492px;
  overflow: auto;
  padding: 4px 8px 12px;
  outline: none;
}

.iec61850-native-page__tree-empty {
  padding: 12px 8px;
  color: #637083;
  font-size: 13px;
}

.iec61850-native-page__tree-spacer {
  position: relative;
  min-height: 100%;
}

.iec61850-native-page__tree-row {
  position: absolute;
  top: 0;
  right: 0;
  left: 0;
  display: flex;
  width: 100%;
  align-items: center;
  gap: 6px;
  border: 0;
  border-radius: 6px;
  padding: 0 8px;
  background: transparent;
  color: #172033;
  font: inherit;
  text-align: left;
}

.iec61850-native-page__tree-row:hover,
.iec61850-native-page__tree-row.is-selected {
  background: #e9f1fb;
}

.iec61850-native-page__tree-row.is-active {
  box-shadow: inset 0 0 0 2px #a8c7ea;
}

.iec61850-native-page__tree-row.is-match .iec61850-native-page__label {
  color: #24589a;
  font-weight: 700;
}

.iec61850-native-page__toggle {
  width: 14px;
  color: #637083;
  text-align: center;
}

.iec61850-native-page__kind {
  width: 72px;
  flex: 0 0 auto;
  color: #637083;
  font-size: 11px;
  text-transform: uppercase;
}

.iec61850-native-page__label {
  min-width: 0;
  flex: 1 1 auto;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-native-page__meta {
  flex: 0 0 auto;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__detail h2 {
  margin: 16px 16px 4px;
  font-size: 18px;
}

.iec61850-native-page__detail p {
  margin: 0 16px 14px;
  overflow-wrap: anywhere;
  color: #637083;
  font-size: 13px;
}

.iec61850-native-page__detail dl {
  display: grid;
  grid-template-columns: 132px minmax(0, 1fr);
  gap: 0;
  margin: 0;
  padding: 0 16px 16px;
  font-size: 13px;
}

.iec61850-native-page__detail dt,
.iec61850-native-page__detail dd {
  border-top: 1px solid #eef1f5;
  margin: 0;
  padding: 8px 0;
}

.iec61850-native-page__detail dt {
  color: #637083;
}

.iec61850-native-page__detail dd {
  min-width: 0;
  overflow-wrap: anywhere;
}

@media (max-width: 900px) {
  .iec61850-native-page__header {
    align-items: stretch;
    flex-direction: column;
  }

  .iec61850-native-page__summary {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .iec61850-native-page__workspace {
    grid-template-columns: 1fr;
  }

  .iec61850-native-page__detail {
    border-top: 1px solid #d7dde6;
    border-left: 0;
  }
}
</style>
