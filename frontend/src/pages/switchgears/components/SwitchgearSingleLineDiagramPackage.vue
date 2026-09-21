<script setup lang="ts">
import { computed, ref, watch } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import WorkspacePlaceholder from "@/components/ui/WorkspacePlaceholder.vue"
import { SldAPI, SLD_DOCUMENT_SCHEMA } from "@/api/sld.api"
import { normalizeHttpError } from "@/api/http"
import { generateSldFromScd } from "@/modules/scd-sld-core"
import type { ScdDiagnostic } from "@/modules/scd-sld-core"
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useChannelStore } from "@/stores/channelStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"

import SwitchgearSingleLineDiagramPackageCanvas from "./SwitchgearSingleLineDiagramPackageCanvas.vue"
import {
  SWITCHGEAR_SLD_NODE_HEIGHT,
  SWITCHGEAR_SLD_NODE_WIDTH,
  SWITCHGEAR_SLD_STAGE_PADDING,
  buildSwitchgearSldPackageSceneModel,
  normalizeStoredDiagramState,
} from "../utils/switchgearSldPackageScene"
import type { DiagramEdge, DiagramNodeLayout, StoredDiagramState } from "../utils/switchgearSldDiagramTypes"
import {
  adaptSldDocumentToSwitchgearDiagram,
  buildSwitchgearCandidateDecisions,
  mergeGeneratedSldDiagramOverlay,
} from "../utils/switchgearSldImportAdapter"
import type {
  SwitchgearSldCandidateDecision,
  SwitchgearSldImportAdapterDiagnostic,
  SwitchgearSldImportAdapterResult,
} from "../utils/switchgearSldImportAdapter"
import {
  createSwitchgearSldTransfer,
  parseSwitchgearSldTransfer,
  resolveImportedBindings,
  type SwitchgearSldTransferPayload,
} from "../utils/switchgearSldTransfer"

const props = defineProps<{
  active?: boolean
  selectedSwitchgearId?: number | null
  selectionVersion?: number
}>()

const emit = defineEmits<{
  (event: "editSwitchgearBindings", id: number): void
}>()

type ScdImportPreviewDiagnostic = Pick<ScdDiagnostic, "severity" | "code" | "message" | "sourceId" | "sourcePath" | "sourceLocation">
type ScdImportDiagnostic = ScdImportPreviewDiagnostic | SwitchgearSldImportAdapterDiagnostic

type ScdImportPreview = {
  fileName: string
  sourceHash: string
  adapterResult: SwitchgearSldImportAdapterResult
  diagnostics: ScdImportDiagnostic[]
}

const workspaceStore = useWorkspaceStore()
const switchgearStore = useSwitchgearStore()
const channelStore = useChannelStore()
const toastStore = useToastStore()
const storedState = ref<StoredDiagramState | null>(null)
const persistenceError = ref<string | null>(null)
const backendRevision = ref(0)
const scdFileInputRef = ref<HTMLInputElement | null>(null)
const transferFileInputRef = ref<HTMLInputElement | null>(null)
const scdImportBusy = ref(false)
const scdImportApplyBusy = ref(false)
const scdImportError = ref<string | null>(null)
const scdImportPreview = ref<ScdImportPreview | null>(null)
const scdImportCreateCandidates = ref(false)
const fitRequestKey = ref(0)
const selectionRequestKey = ref(0)
const requestedSelectionIds = ref<string[]>([])
const transferImportBusy = ref(false)
const transferImportApplyBusy = ref(false)
const transferImportError = ref<string | null>(null)
const transferImportPreview = ref<SwitchgearSldTransferPayload | null>(null)
const canvasRef = ref<{ getTransferState: () => { state: StoredDiagramState; selectedIds: string[] } } | null>(null)

const workspaceId = computed(() => workspaceStore.activeWorkspaceId)
const storageKey = computed(() => (
  workspaceId.value ? localSettingsKeys.switchgearDiagram(workspaceId.value) : null
))
const sceneModel = computed(() => buildSwitchgearSldPackageSceneModel(
  switchgearStore.switchgears,
  storedState.value,
))
const scdImportModalOpen = computed(() => (
  scdImportBusy.value
  || scdImportApplyBusy.value
  || scdImportPreview.value !== null
  || scdImportError.value !== null
))
const transferImportModalOpen = computed(() => (
  transferImportBusy.value || transferImportApplyBusy.value || transferImportPreview.value !== null || transferImportError.value !== null
))
const scdImportSummary = computed(() => {
  const preview = scdImportPreview.value
  if (!preview) {
    return null
  }
  return {
    candidates: preview.adapterResult.switchgearCandidates.length,
    diagnostics: scdImportVisibleDiagnostics.value.length,
  }
})
const scdImportCandidateDecisions = computed<SwitchgearSldCandidateDecision[]>(() => {
  const preview = scdImportPreview.value
  return preview
    ? buildSwitchgearCandidateDecisions(preview.adapterResult.switchgearCandidates, switchgearStore.switchgears)
    : []
})
const scdImportCreatableCandidateCount = computed(() => (
  scdImportCandidateDecisions.value.filter(decision => decision.action === "create").length
))
const scdImportReusableCandidateCount = computed(() => (
  scdImportCandidateDecisions.value.filter(decision => decision.action === "reuse-existing").length
))
const scdImportOverlaySummary = computed(() => {
  const preview = scdImportPreview.value
  if (!preview) {
    return "0 lines"
  }
  const diagram = preview.adapterResult.diagram
  return `${(diagram.edges ?? diagram.lines ?? []).length} lines · ${(diagram.staticElements ?? []).length} symbols · ${(diagram.textElements ?? []).length} texts`
})
const scdImportApplyLabel = computed(() => (
  scdImportCreateCandidates.value && scdImportCreatableCandidateCount.value > 0
    ? "Apply overlay and create records"
    : "Apply overlay"
))
const scdImportVisibleDiagnostics = computed(() => scdImportPreview.value?.diagnostics ?? [])
const canApplyScdImport = computed(() => scdImportPreview.value !== null && !scdImportBusy.value && !scdImportApplyBusy.value)
let persistenceLoadToken = 0
let saveQueue = Promise.resolve()

watch(
  () => [workspaceId.value, props.active] as const,
  () => {
    void loadStoredState()
  },
  { immediate: true },
)

watch(
  () => [props.selectedSwitchgearId, props.selectionVersion] as const,
  (id) => {
    const selectedId = id[0]
    requestedSelectionIds.value = selectedId == null ? [] : [`switchgear:${selectedId}`]
    selectionRequestKey.value += 1
  },
  { immediate: true },
)

defineExpose({
  openScdFileDialog,
  openSldImportDialog,
})

function openSldImportDialog() {
  if (!workspaceId.value || transferImportBusy.value || transferImportApplyBusy.value) return
  transferFileInputRef.value?.click()
}

function downloadJson(payload: unknown, filename: string) {
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: "application/json;charset=utf-8" })
  const url = URL.createObjectURL(blob)
  const link = document.createElement("a")
  link.href = url
  link.download = filename
  document.body.appendChild(link)
  link.click()
  document.body.removeChild(link)
  URL.revokeObjectURL(url)
}

function exportSld(scope: "full" | "selection") {
  const transfer = canvasRef.value?.getTransferState()
  if (!transfer || !workspaceId.value) return
  if (scope === "selection" && transfer.selectedIds.length === 0) {
    return
  }
  const payload = createSwitchgearSldTransfer(
    transfer.state,
    {
      switchgears: switchgearStore.switchgears,
      channels: channelStore.channels,
      resolveUnitId: channelStore.resolveUnitId,
    },
    scope === "selection" ? transfer.selectedIds : undefined,
  )
  downloadJson(payload, `${scope === "selection" ? "sld-selection" : "sld"}_${workspaceId.value}.json`)
}

async function handleTransferFileSelected(event: Event) {
  const input = event.target as HTMLInputElement | null
  const file = input?.files?.[0] ?? null
  if (input) input.value = ""
  if (!file) return
  transferImportBusy.value = true
  transferImportError.value = null
  transferImportPreview.value = null
  try {
    transferImportPreview.value = parseSwitchgearSldTransfer(JSON.parse(await file.text()))
  } catch (error) {
    transferImportError.value = error instanceof Error ? error.message : "Unable to read SLD transfer file"
  } finally {
    transferImportBusy.value = false
  }
}

function closeTransferImport() {
  if (transferImportBusy.value || transferImportApplyBusy.value) return
  transferImportPreview.value = null
  transferImportError.value = null
}

async function applyTransferImport() {
  const payload = transferImportPreview.value
  const workspace = workspaceId.value
  const current = canvasRef.value?.getTransferState().state ?? storedState.value
  if (!payload || !workspace || !current) return

  transferImportApplyBusy.value = true
  try {
    await channelStore.ensureLoaded()
    const usedNames = new Set(switchgearStore.switchgears.map(item => item.name.trim().toLowerCase()))
    const switchgearIdMap = new Map<string, number>()
    const importedNodeIds: string[] = []
    let detachedBindingCount = 0
    for (const item of payload.switchgears) {
      const baseName = item.name.trim() || "Imported switchgear"
      let name = baseName
      let suffix = 2
      while (usedNames.has(name.toLowerCase())) {
        name = `${baseName} (${suffix++})`
      }
      usedNames.add(name.toLowerCase())
      const resolved = resolveImportedBindings(item, channelStore.channels, channelStore.resolveUnitId)
      detachedBindingCount += resolved.detachedRoles.length
      const created = await switchgearStore.create({
        name,
        switchgear_type: item.switchgearType,
        bindings: resolved.bindings,
      })
      switchgearIdMap.set(item.key, created.id)
      importedNodeIds.push(`switchgear:${created.id}`)
    }

    const merged = mergeImportedDiagram(current, payload.diagram, switchgearIdMap)
    const nextState: StoredDiagramState = {
      ...current,
      ...merged,
      workspaceId: workspace,
      viewState: current.viewState,
    }
    storedState.value = nextState
    await persistDocument(nextState, "import")
    requestedSelectionIds.value = importedNodeIds
    selectionRequestKey.value += 1
    closeTransferImport()
    if (detachedBindingCount > 0) {
      toastStore.warning(`Detached ${detachedBindingCount} unavailable binding${detachedBindingCount === 1 ? "" : "s"} during SLD import`)
    }
  } catch (error) {
    transferImportError.value = error instanceof Error ? error.message : "Unable to apply SLD transfer"
  } finally {
    transferImportApplyBusy.value = false
  }
}

function mergeImportedDiagram(current: StoredDiagramState, imported: StoredDiagramState, switchgearIdMap: ReadonlyMap<string, number>): StoredDiagramState {
  const currentEdges = current.edges ?? current.lines ?? []
  const importedEdges = imported.edges ?? imported.lines ?? []
  const existingEdgeIds = new Set(currentEdges.map(item => item.id))
  const existingStaticIds = new Set((current.staticElements ?? []).map(item => item.id))
  const existingTextIds = new Set((current.textElements ?? []).map(item => item.id))
  const edgeIdMap = new Map<string, string>()
  const staticIdMap = new Map<string, string>()
  const textIdMap = new Map<string, string>()
  const uniqueId = (prefix: string, source: string, used: Set<string>) => {
    let id = `${prefix}:${source}`
    let index = 2
    while (used.has(id)) id = `${prefix}:${source}:${index++}`
    used.add(id)
    return id
  }
  for (const edge of importedEdges) edgeIdMap.set(edge.id, uniqueId("sld-import-edge", edge.id, existingEdgeIds))
  for (const item of imported.staticElements ?? []) staticIdMap.set(item.id, uniqueId("sld-import-static", item.id, existingStaticIds))
  for (const item of imported.textElements ?? []) textIdMap.set(item.id, uniqueId("sld-import-text", item.id, existingTextIds))

  const remapBinding = (binding: DiagramEdge["startBinding"]) => {
    if (!binding) return null
    if (binding.ownerType === "node") {
      const target = switchgearIdMap.get(`switchgear:${binding.ownerId}`)
      return target == null ? null : { ...binding, ownerId: target }
    }
    const target = staticIdMap.get(String(binding.ownerId))
    return target == null ? null : { ...binding, ownerId: target }
  }
  const remappedEdges = importedEdges.map(edge => ({
    ...edge,
    id: edgeIdMap.get(edge.id) ?? edge.id,
    startBinding: remapBinding(edge.startBinding),
    endBinding: remapBinding(edge.endBinding),
  }))
  const remappedStatics = (imported.staticElements ?? []).map(item => ({ ...item, id: staticIdMap.get(item.id) ?? item.id }))
  const remappedTexts = (imported.textElements ?? []).map(item => ({ ...item, id: textIdMap.get(item.id) ?? item.id }))
  const remapRecord = (record: Record<string, number> | undefined) => Object.fromEntries(Object.entries(record ?? {}).flatMap(([id, value]) => {
    if (id.startsWith("switchgear:")) {
      const target = switchgearIdMap.get(id)
      return target == null ? [] : [[`switchgear:${target}`, value]]
    }
    if (id.startsWith("static:")) {
      const target = staticIdMap.get(id.slice("static:".length))
      return target == null ? [] : [[`static:${target}`, value]]
    }
    return [[edgeIdMap.get(id) ?? textIdMap.get(id) ?? id, value]]
  }))
  const remapRotation = (record: StoredDiagramState["rotationById"]) => remapRecord(record)
  const remappedLayout = Object.fromEntries(Object.entries(imported.layoutById ?? {}).flatMap(([id, layout]) => {
    const target = switchgearIdMap.get(`switchgear:${id}`)
    return target == null ? [] : [[String(target), layout]]
  }))
  const remappedLabels = Object.fromEntries(Object.entries(imported.labelOffsetById ?? {}).flatMap(([id, offset]) => {
    const target = switchgearIdMap.get(`switchgear:${id}`)
    return target == null ? [] : [[String(target), offset]]
  }))
  return {
    layoutById: { ...(current.layoutById ?? {}), ...remappedLayout },
    labelOffsetById: { ...(current.labelOffsetById ?? {}), ...remappedLabels },
    zIndexById: { ...(current.zIndexById ?? {}), ...remapRecord(imported.zIndexById) },
    rotationById: { ...(current.rotationById ?? {}), ...remapRotation(imported.rotationById) },
    edges: [...currentEdges, ...remappedEdges],
    lines: [...currentEdges, ...remappedEdges],
    staticElements: [...(current.staticElements ?? []), ...remappedStatics],
    textElements: [...(current.textElements ?? []), ...remappedTexts],
  }
}

function openScdFileDialog() {
  if (!workspaceId.value || scdImportBusy.value) {
    return
  }
  scdFileInputRef.value?.click()
}

async function handleScdFileSelected(event: Event) {
  const input = event.target as HTMLInputElement | null
  const file = input?.files?.[0] ?? null
  if (input) {
    input.value = ""
  }
  if (!file || !workspaceId.value) {
    return
  }

  scdImportBusy.value = true
  scdImportApplyBusy.value = false
  scdImportError.value = null
  scdImportPreview.value = null
  scdImportCreateCandidates.value = false

  try {
    const xmlText = await file.text()
    const sourceHash = await hashText(xmlText)
    const generated = generateSldFromScd({
      fileName: file.name,
      contentHash: sourceHash,
      xmlText,
      workspaceId: workspaceId.value,
    }, {
      gridSize: 24,
    })
    const adapterResult = adaptSldDocumentToSwitchgearDiagram(generated.document, {
      stagePadding: SWITCHGEAR_SLD_STAGE_PADDING,
    })

    scdImportPreview.value = {
      fileName: file.name,
      sourceHash,
      adapterResult,
      diagnostics: [...generated.diagnostics, ...adapterResult.diagnostics],
    }
  } catch (error) {
    scdImportError.value = error instanceof Error ? error.message : "Unable to read SCD file"
  } finally {
    scdImportBusy.value = false
  }
}

function closeScdImportPreview() {
  if (scdImportBusy.value || scdImportApplyBusy.value) {
    return
  }
  resetScdImportPreview()
}

function resetScdImportPreview() {
  scdImportPreview.value = null
  scdImportError.value = null
  scdImportCreateCandidates.value = false
}

async function applyScdImportPreview() {
  const preview = scdImportPreview.value
  const workspace = workspaceId.value
  const key = storageKey.value
  if (!preview || !workspace || !key || !canApplyScdImport.value) {
    return
  }

  scdImportApplyBusy.value = true
  try {
    const base = storedState.value ?? { workspaceId: workspace, snapEnabled: true }
    const merged = mergeGeneratedSldDiagramOverlay(base, preview.adapterResult.diagram)
    const createdIds = scdImportCreateCandidates.value
      ? await createSwitchgearsFromScdCandidates(scdImportCandidateDecisions.value)
      : []
    const nextState: StoredDiagramState = {
      ...base,
      ...merged,
      workspaceId: workspace,
      layoutById: {
        ...(base.layoutById ?? {}),
        ...Object.fromEntries(createdIds.map(item => [String(item.id), item.layout])),
      },
      labelOffsetById: base.labelOffsetById ?? {},
      viewState: base.viewState,
    }
    storedState.value = nextState
    await persistDocument(nextState, "import")
    requestedSelectionIds.value = createdIds.map(item => `switchgear:${item.id}`)
    selectionRequestKey.value += 1
    fitRequestKey.value += 1
    resetScdImportPreview()
    if ((preview.adapterResult.diagram.edges ?? preview.adapterResult.diagram.lines ?? []).length > 0 || createdIds.length > 0) {
      return
    }
    toastStore.warning("SCD import did not change the diagram overlay")
  } catch (error) {
    scdImportError.value = error instanceof Error ? error.message : "Unable to apply SCD import"
  } finally {
    scdImportApplyBusy.value = false
  }
}

async function createSwitchgearsFromScdCandidates(decisions: SwitchgearSldCandidateDecision[]) {
  const toCreate = decisions.filter(decision => decision.action === "create")
  const created: Array<{ id: number; layout: DiagramNodeLayout }> = []

  for (const decision of toCreate) {
    const next = await switchgearStore.create({
      name: decision.createName,
      switchgear_type: decision.candidate.switchgearType,
    })
    created.push({
      id: next.id,
      layout: layoutFromCandidatePosition(decision.candidate.position),
    })
  }

  return created
}

function layoutFromCandidatePosition(position: { x: number; y: number }): DiagramNodeLayout {
  return {
    x: Math.round(position.x - SWITCHGEAR_SLD_STAGE_PADDING - SWITCHGEAR_SLD_NODE_WIDTH / 2),
    y: Math.round(position.y - SWITCHGEAR_SLD_STAGE_PADDING - SWITCHGEAR_SLD_NODE_HEIGHT / 2),
  }
}

function formatScdImportDiagnosticContext(diagnostic: ScdImportDiagnostic) {
  const parts: string[] = []
  if (diagnostic.sourceId) {
    parts.push(diagnostic.sourceId)
  }
  if (diagnostic.sourcePath) {
    parts.push(diagnostic.sourcePath)
  }
  return parts.length > 0 ? parts.join(" · ") : null
}

async function hashText(value: string): Promise<string> {
  if (typeof crypto !== "undefined" && crypto.subtle) {
    const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value))
    return Array.from(new Uint8Array(digest))
      .map(byte => byte.toString(16).padStart(2, "0"))
      .join("")
  }

  let hash = 0
  for (let index = 0; index < value.length; index += 1) {
    hash = ((hash << 5) - hash + value.charCodeAt(index)) | 0
  }
  return `fallback:${value.length}:${Math.abs(hash)}`
}

async function loadStoredState() {
  const loadToken = ++persistenceLoadToken
  if (!workspaceId.value || !storageKey.value) {
    storedState.value = null
    persistenceError.value = null
    return
  }

  const localState = readLocalSetting<StoredDiagramState | null>(
    storageKey.value,
    null,
    {
      legacyKeys: [`unitlab.switchgears.sld.${workspaceId.value}`],
      validate: normalizeStoredDiagramState,
    },
  )

  // Render the local recovery copy immediately. The backend response remains authoritative
  // and replaces it when available, but a slow or restarting backend must not hide the editor.
  storedState.value = localState ?? { workspaceId: workspaceId.value, snapEnabled: true }
  persistenceError.value = null
  try {
    const response = await SldAPI.get(workspaceId.value)
    if (loadToken !== persistenceLoadToken) return
    backendRevision.value = response.data.revision
    const remoteState = normalizeStoredDiagramState(response.data.document)
    if (response.data.revision > 0) {
      storedState.value = remoteState
    } else if (localState) {
      storedState.value = localState
      void persistDocument(localState, "migration", 0).catch(() => undefined)
    } else {
      storedState.value = { workspaceId: workspaceId.value, snapEnabled: true }
    }
  } catch (error) {
    if (loadToken !== persistenceLoadToken) return
    storedState.value = localState ?? { workspaceId: workspaceId.value, snapEnabled: true }
    persistenceError.value = normalizeHttpError(error, "Unable to load SLD from backend").message
    toastStore.error("SLD backend is unavailable; changes will not be persisted")
  }
}

function persistDocument(state: StoredDiagramState, changeKind: "edit" | "import" | "migration" = "edit", migrationBaseRevision?: number) {
  const workspace = workspaceId.value
  if (!workspace) return Promise.reject(new Error("Workspace is not selected"))
  const operation = saveQueue.catch(() => undefined).then(async () => {
    const response = await SldAPI.save(workspace, {
      base_revision: migrationBaseRevision ?? backendRevision.value,
      document_schema: SLD_DOCUMENT_SCHEMA,
      document: state,
      change_kind: changeKind,
    })
    backendRevision.value = response.data.revision
    writeLocalSetting(storageKey.value ?? localSettingsKeys.switchgearDiagram(workspace), state, {
      legacyKeys: [`unitlab.switchgears.sld.${workspace}`],
    })
  })
  saveQueue = operation.then(() => undefined)
  return operation.catch((error) => {
    persistenceError.value = normalizeHttpError(error, "Unable to save SLD").message
    throw error
  })
}
</script>

<template>
  <section class="switchgear-sld-package">
    <input
      ref="scdFileInputRef"
      type="file"
      accept=".scd,.sed,.ssd,.xml,application/xml,text/xml"
      class="switchgear-sld-package__file-input"
      @change="handleScdFileSelected"
    >
    <input
      ref="transferFileInputRef"
      type="file"
      accept="application/json,.json"
      class="switchgear-sld-package__file-input"
      @change="handleTransferFileSelected"
    >
    <WorkspacePlaceholder
      v-if="!workspaceId"
      tag="SLD"
      title="Select a workspace"
      description="Choose a workspace to compare and edit the package-based SLD projection."
    />
    <SwitchgearSingleLineDiagramPackageCanvas
      ref="canvasRef"
      v-else-if="storageKey"
      :key="sceneModel.sceneKey"
      :model="sceneModel"
      :workspace-id="workspaceId"
      :storage-key="storageKey"
      :initial-stored-state="storedState"
      :fit-request-key="fitRequestKey"
      :selection-request-key="selectionRequestKey"
      :requested-selection-ids="requestedSelectionIds"
      :persist-document="persistDocument"
      @export-sld="exportSld"
      @import-sld="openSldImportDialog"
      @edit-switchgear-bindings="emit('editSwitchgearBindings', $event)"
    />

    <UiModal
      :open="scdImportModalOpen"
      title="Import SCD"
      max-width="2xl"
      desktop-height="92vh"
      :content-scroll="false"
      @close="closeScdImportPreview"
    >
      <div class="switchgear-sld-package__import-review">
        <div v-if="scdImportBusy" class="switchgear-sld-package__import-state">
          Parsing SCD topology...
        </div>

        <div v-else-if="scdImportError" class="switchgear-sld-package__import-alert">
          {{ scdImportError }}
        </div>

        <template v-else-if="scdImportPreview && scdImportSummary">
          <div class="switchgear-sld-package__import-heading">
            <div>
              <p class="switchgear-sld-package__import-eyebrow">Source</p>
              <p class="switchgear-sld-package__import-title">{{ scdImportPreview.fileName }}</p>
            </div>
            <span class="switchgear-sld-package__import-hash">{{ scdImportPreview.sourceHash.slice(0, 12) }}</span>
          </div>

          <div class="switchgear-sld-package__import-metrics">
            <div class="switchgear-sld-package__import-metric"><span>Found</span><strong>{{ scdImportSummary.candidates }}</strong></div>
            <div class="switchgear-sld-package__import-metric"><span>New records</span><strong>{{ scdImportCreatableCandidateCount }}</strong></div>
            <div class="switchgear-sld-package__import-metric"><span>Existing</span><strong>{{ scdImportReusableCandidateCount }}</strong></div>
            <div class="switchgear-sld-package__import-metric"><span>Diagnostics</span><strong>{{ scdImportSummary.diagnostics }}</strong></div>
          </div>

          <div v-if="scdImportCandidateDecisions.length > 0" class="switchgear-sld-package__import-records">
            <div class="switchgear-sld-package__import-record-summary">
              <div>
                <p class="switchgear-sld-package__import-section-title">Switchgear records</p>
                <p class="switchgear-sld-package__import-muted">
                  {{ scdImportCreatableCandidateCount }} new · {{ scdImportReusableCandidateCount }} matched existing · overlay {{ scdImportOverlaySummary }}
                </p>
              </div>
              <label v-if="scdImportCreatableCandidateCount > 0" class="switchgear-sld-package__import-candidate-toggle">
                <input v-model="scdImportCreateCandidates" type="checkbox" :disabled="scdImportCreatableCandidateCount === 0">
                <span>Create missing records</span>
              </label>
            </div>
            <div class="switchgear-sld-package__import-note">
              Operational records are created only when enabled here. SCD import never creates bindings.
            </div>
          </div>

          <div class="switchgear-sld-package__import-diagnostics">
            <p class="switchgear-sld-package__import-section-title">Errors / warnings log</p>
            <div v-if="scdImportVisibleDiagnostics.length === 0" class="switchgear-sld-package__import-empty-log">
              No actionable errors or warnings.
            </div>
            <ul v-else class="switchgear-sld-package__import-diagnostic-list">
              <li
                v-for="diagnostic in scdImportVisibleDiagnostics"
                :key="`${diagnostic.severity}:${diagnostic.code}:${diagnostic.sourceId ?? diagnostic.sourcePath ?? diagnostic.message}`"
                class="switchgear-sld-package__import-diagnostic"
                :class="`switchgear-sld-package__import-diagnostic--${diagnostic.severity}`"
              >
                <span>{{ diagnostic.severity }}</span>
                <div>
                  <strong>{{ diagnostic.code }}</strong>
                  <p
                    v-if="formatScdImportDiagnosticContext(diagnostic)"
                    class="switchgear-sld-package__import-diagnostic-context"
                  >
                    {{ formatScdImportDiagnosticContext(diagnostic) }}
                  </p>
                  <p>{{ diagnostic.message }}</p>
                </div>
              </li>
            </ul>
          </div>
        </template>
      </div>

      <template #footer>
        <UiButton variant="secondary" :disabled="scdImportBusy || scdImportApplyBusy" @click="closeScdImportPreview">
          Cancel
        </UiButton>
        <UiButton variant="primary" :disabled="!canApplyScdImport || scdImportBusy || scdImportApplyBusy" @click="applyScdImportPreview">
          {{ scdImportApplyBusy ? 'Applying...' : scdImportApplyLabel }}
        </UiButton>
      </template>
    </UiModal>

    <UiModal
      :open="transferImportModalOpen"
      title="Import SLD"
      max-width="xl"
      :content-scroll="false"
      @close="closeTransferImport"
    >
      <div class="switchgear-sld-package__import-review">
        <div v-if="transferImportBusy" class="switchgear-sld-package__import-state">Reading SLD transfer...</div>
        <div v-else-if="transferImportError" class="switchgear-sld-package__import-alert">{{ transferImportError }}</div>
        <template v-else-if="transferImportPreview">
          <p>This will add {{ transferImportPreview.switchgears.length }} switchgear records and {{ transferImportPreview.diagram.edges?.length ?? 0 }} lines to the current SLD.</p>
          <p class="switchgear-sld-package__import-muted">Unavailable hardware channels will be detached automatically.</p>
        </template>
      </div>
      <template #footer>
        <UiButton variant="secondary" :disabled="transferImportBusy || transferImportApplyBusy" @click="closeTransferImport">Cancel</UiButton>
        <UiButton variant="primary" :disabled="!transferImportPreview || transferImportBusy || transferImportApplyBusy" @click="applyTransferImport">
          {{ transferImportApplyBusy ? 'Importing...' : 'Import' }}
        </UiButton>
      </template>
    </UiModal>
  </section>
</template>

<style scoped>
.switchgear-sld-package__file-input {
  display: none;
}

.switchgear-sld-package {
  display: flex;
  min-height: 0;
  flex: 1 1 auto;
}

.switchgear-sld-package > :deep(*) {
  min-height: 0;
  flex: 1 1 auto;
}

.switchgear-sld-package__import-review {
  display: grid;
  gap: 1rem;
}

.switchgear-sld-package__import-state,
.switchgear-sld-package__import-alert,
.switchgear-sld-package__import-records,
.switchgear-sld-package__import-diagnostics {
  max-height: min(42vh, 30rem);
  overflow-y: auto;
  overscroll-behavior: contain;
  padding: 1rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-xl);
  background: var(--color-white);
}

.switchgear-sld-package__import-alert {
  color: var(--color-rose-700);
}

.switchgear-sld-package__import-heading,
.switchgear-sld-package__import-record-summary,
.switchgear-sld-package__import-metrics {
  display: flex;
  gap: 0.75rem;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
}

.switchgear-sld-package__import-eyebrow,
.switchgear-sld-package__import-muted,
.switchgear-sld-package__import-note,
.switchgear-sld-package__import-diagnostic-context {
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.switchgear-sld-package__import-title,
.switchgear-sld-package__import-section-title {
  font-weight: 600;
}

.switchgear-sld-package__import-hash {
  padding: 0.25rem 0.5rem;
  border-radius: var(--radius-pill);
  background: var(--color-neutral-100);
  font-size: var(--text-xs);
}

.switchgear-sld-package__import-metrics {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
}

.switchgear-sld-package__import-metric {
  display: grid;
  gap: 0.2rem;
  padding: 0.75rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-xl);
  background: var(--color-neutral-50);
}

.switchgear-sld-package__import-candidate-toggle {
  display: inline-flex;
  gap: 0.5rem;
  align-items: center;
}

.switchgear-sld-package__import-diagnostic-list {
  display: grid;
  gap: 0.75rem;
  margin: 0;
  padding: 0;
  list-style: none;
}

.switchgear-sld-package__import-diagnostic {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: 0.75rem;
  padding: 0.75rem;
  border-radius: var(--radius-xl);
  background: var(--color-neutral-50);
}

.switchgear-sld-package__import-diagnostic--warning {
  border: 1px solid var(--color-amber-200);
}

.switchgear-sld-package__import-empty-log {
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

:global(.dark .switchgear-sld-package__import-state),
:global(.dark .switchgear-sld-package__import-alert),
:global(.dark .switchgear-sld-package__import-records),
:global(.dark .switchgear-sld-package__import-diagnostics),
:global(.dark .switchgear-sld-package__import-metric),
:global(.dark .switchgear-sld-package__import-diagnostic) {
  border-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}

:global(.dark .switchgear-sld-package__import-hash) {
  background: var(--color-neutral-800);
}

:global(.dark .switchgear-sld-package__import-eyebrow),
:global(.dark .switchgear-sld-package__import-muted),
:global(.dark .switchgear-sld-package__import-note),
:global(.dark .switchgear-sld-package__import-diagnostic-context),
:global(.dark .switchgear-sld-package__import-empty-log) {
  color: var(--color-neutral-400);
}

@media (max-width: 960px) {
  .switchgear-sld-package__import-metrics {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

</style>
