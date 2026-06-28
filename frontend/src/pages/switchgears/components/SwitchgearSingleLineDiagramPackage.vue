<script setup lang="ts">
import { computed, ref, watch } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import WorkspacePlaceholder from "@/components/ui/WorkspacePlaceholder.vue"
import { generateSldFromScd } from "@/modules/scd-sld-core"
import type { ScdDiagnostic } from "@/modules/scd-sld-core"
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"
import { useSwitchgearStore } from "@/stores/switchgearStore"
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
import type { DiagramNodeLayout, StoredDiagramState } from "../utils/switchgearSldDiagramTypes"
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

const props = defineProps<{
  active?: boolean
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
const toastStore = useToastStore()
const storedState = ref<StoredDiagramState | null>(null)
const scdFileInputRef = ref<HTMLInputElement | null>(null)
const scdImportBusy = ref(false)
const scdImportApplyBusy = ref(false)
const scdImportError = ref<string | null>(null)
const scdImportPreview = ref<ScdImportPreview | null>(null)
const scdImportCreateCandidates = ref(false)
const fitRequestKey = ref(0)
const selectionRequestKey = ref(0)
const requestedSelectionIds = ref<string[]>([])

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

watch(
  () => [workspaceId.value, props.active] as const,
  () => {
    if (props.active === false) {
      return
    }
    loadStoredState()
  },
  { immediate: true },
)

defineExpose({
  openScdFileDialog,
})

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
    writeLocalSetting(key, nextState, {
      legacyKeys: [`unitlab.switchgears.sld.${workspace}`],
    })
    requestedSelectionIds.value = createdIds.map(item => `switchgear:${item.id}`)
    selectionRequestKey.value += 1
    fitRequestKey.value += 1
    resetScdImportPreview()
    if ((preview.adapterResult.diagram.edges ?? preview.adapterResult.diagram.lines ?? []).length > 0 || createdIds.length > 0) {
      toastStore.success(createdIds.length > 0
        ? `Imported SLD overlay and created ${createdIds.length} switchgear record${createdIds.length > 1 ? "s" : ""}`
        : "Imported SLD overlay")
      return
    }
    toastStore.info("SCD import did not change the diagram overlay")
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

function loadStoredState() {
  if (!workspaceId.value || !storageKey.value) {
    storedState.value = null
    return
  }

  storedState.value = readLocalSetting<StoredDiagramState | null>(
    storageKey.value,
    null,
    {
      legacyKeys: [`unitlab.switchgears.sld.${workspaceId.value}`],
      validate: normalizeStoredDiagramState,
    },
  )
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
    <WorkspacePlaceholder
      v-if="!workspaceId"
      tag="SLD"
      title="Select a workspace"
      description="Choose a workspace to compare and edit the package-based SLD projection."
    />
    <WorkspacePlaceholder
      v-else-if="switchgearStore.switchgears.length === 0"
      tag="Single Line Diagram"
      title="No switchgears yet"
      description="Create switchgears from the sidebar or import an SCD overlay."
    />
    <SwitchgearSingleLineDiagramPackageCanvas
      v-else-if="storageKey"
      :key="sceneModel.sceneKey"
      :model="sceneModel"
      :workspace-id="workspaceId"
      :storage-key="storageKey"
      :initial-stored-state="storedState"
      :fit-request-key="fitRequestKey"
      :selection-request-key="selectionRequestKey"
      :requested-selection-ids="requestedSelectionIds"
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
  padding: 1rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.75rem;
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
  border-radius: 999px;
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
  border-radius: 0.75rem;
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
  border-radius: 0.75rem;
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
