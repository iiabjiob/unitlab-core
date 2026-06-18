<template>
  <div class="signals-page">
    <AllocationEditorHeader
      :summary-text="summaryText"
      :workspace-missing="workspaceMissing"
      :loading="loading"
      :allocated-cable-rows-count="allocatedCableRows.length"
      :allocation-rows-count="allocationProjectionRowsCount"
      :allocating-selected="allocatingSelected"
      :deallocating-selected="deallocatingSelected"
      :allocate-selected-label="allocateSelectedButtonLabel"
      :deallocate-selected-label="deallocateSelectedButtonLabel"
      :can-resume-active-test-run="canResumeActiveTestRun"
      :can-allocate-selected="selectedUnassignedSignalIds.length > 0 || allocatingSelected"
      :can-deallocate-selected="selectedAllocatedSignalIds.length > 0 || deallocatingSelected"
      :can-run-test="selectedVisibleAllocatedPhysicalRows.length > 0 || isTestRunBusy || canResumeActiveTestRun"
      :is-test-run-busy="isTestRunBusy"
      :test-run-toggle-mode="testRunToggleMode"
      :test-run-interval-ms="testRunIntervalMs"
      @import="openImportModal"
      @export-cable="openExportModal"
      @export-report="exportSignalReport"
      @allocate-selected="allocateSelectedUnassigned"
      @deallocate-selected="deallocateSelected"
      @run-test="runTestVisualOnly"
      @set-toggle-mode="setTestRunToggleMode"
      @set-interval-ms="setTestRunIntervalMs"
    />

    <div
      v-if="workspaceMissing"
      class="signals-page__empty"
    >
      Select a workspace first.
    </div>

    <div
      v-else-if="error"
      class="signals-page__error"
    >
      {{ error }}
    </div>

    <div
      v-else-if="!loading && allocationProjectionRowsCount === 0"
      class="signals-page__empty"
    >
      No signals found.
    </div>

    <section v-else class="affino-native-data-grid signals-page__grid-section">
      <div
        v-if="showSignalGridSkeleton"
        ref="signalGridSkeletonRef"
        class="signals-page__skeleton"
        aria-hidden="true"
      >
        <div class="signals-page__skeleton-toolbar">
          <div class="signals-page__skeleton-block signals-page__skeleton-block--toolbar-wide" />
          <div class="signals-page__skeleton-block signals-page__skeleton-block--toolbar-medium" />
          <div class="signals-page__skeleton-block signals-page__skeleton-block--toolbar-action" />
        </div>
        <div
          class="signals-page__skeleton-head-row"
          :style="{ gridTemplateColumns: '44px minmax(44px, 0.7fr) minmax(72px, 1fr) minmax(96px, 1.4fr) 136px 128px' }"
        >
          <div v-for="columnIndex in 6" :key="`signals-grid-skeleton-head-${columnIndex}`" class="signals-page__skeleton-block signals-page__skeleton-block--head" />
        </div>
        <div class="signals-page__skeleton-body">
          <div
            v-for="rowIndex in signalGridSkeletonRowCount"
            :key="`signals-grid-skeleton-row-${rowIndex}`"
            class="signals-page__skeleton-row"
            :style="{ gridTemplateColumns: '44px minmax(44px, 0.7fr) minmax(72px, 1fr) minmax(96px, 1.4fr) 136px 128px' }"
          >
            <div class="signals-page__skeleton-block signals-page__skeleton-block--checkbox" />
            <div class="signals-page__skeleton-block signals-page__skeleton-block--line" />
            <div class="signals-page__skeleton-block signals-page__skeleton-block--line" />
            <div class="signals-page__skeleton-block signals-page__skeleton-block--line" />
            <div class="signals-page__skeleton-block signals-page__skeleton-block--status" />
            <div class="signals-page__skeleton-block signals-page__skeleton-block--line" />
          </div>
        </div>
      </div>
      <div
        v-if="allocationProjectionRowsCount > 0"
        class="affino-native-data-grid__shell"
        :style="showSignalGridSkeleton ? { visibility: 'hidden', pointerEvents: 'none' } : undefined"
        :aria-busy="showSignalGridSkeleton ? 'true' : undefined"
      >
        <DataGrid
          ref="allocationGridRef"
          :rows="gridRows"
          :columns="resolvedColumns"
          :row-selection-state="rowSelectionState"
          :theme="theme"
          :grid-lines="gridLines"
          :client-row-model-options="clientRowModelOptions"
          :virtualization="virtualizationOptions"
          :toolbar-modules="toolbarModules"
          :column-layout="true"
          :advanced-filter="true"
          :row-selection="SIGNAL_GRID_ROW_SELECTION"
          render-mode="virtualization"
          layout-mode="fill"
          row-hover
          striped-rows
          @update:rowSelectionState="handleAllocationRowSelectionStateUpdate"
          @update:state="handleAllocationGridStateUpdate"
        />
      </div>
    </section>

    <AllocationChannelPickerPanel
      :key="allocationChannelPickerInstanceKey"
      :open="allocationChannelPickerOpen"
      :signal-name="allocationChannelPickerRow?.signal_name ?? ''"
      :signal-key="allocationChannelPickerRow?.signal_key ?? ''"
      :signal-direction="allocationChannelPickerRow?.signal_direction ?? ''"
      :current-label="allocationChannelPickerCurrentLabel"
      :current-channel-id="allocationChannelPickerCurrentChannelId"
      :channels="allocationChannelPickerChannels"
      :loading="allocationChannelPickerLoading"
      :saving="allocationChannelPickerSaving"
      :error="allocationChannelPickerError"
      @close="closeAllocationChannelPicker"
      @select="handleAllocationChannelPicked"
    />

    <SignalImportModal :open="importModalOpen" @close="closeImportModal" @imported="handleImported" />
    <SignalExportModal
      :open="exportModalOpen"
      :workspace-id="workspaceStore.activeWorkspaceId"
      :required-columns="requiredExportColumnOptions"
      :optional-columns="optionalExportColumnOptions"
      @close="closeExportModal"
      @export="handleExportCableFromWizard"
    />
    <ConfirmModal
      :open="deleteSelectedConfirmOpen"
      title="Delete selected signals"
      :message="deleteSelectedConfirmMessage"
      confirm-label="Delete signals"
      cancel-label="Cancel"
      @cancel="closeDeleteSelectedConfirm"
      @confirm="confirmDeleteSelected"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, defineComponent, h, nextTick, onBeforeUnmount, onMounted, ref, watch, type PropType } from "vue"
import { useRoute, useRouter } from "vue-router"
import { storeToRefs } from "pinia"
import { defineDataGridComponent, parseDataGridSavedView, useDataGridRef, type DataGridAppCellRendererContext, type DataGridAppColumnInput, type DataGridAppToolbarModule, type DataGridProps, type DataGridSavedViewSnapshot, writeDataGridSavedViewToStorage } from "@affino/datagrid-vue-app"

import { SignalsAPI } from "@/api/signals.api"
import type { AoChannel, DoChannel } from "@/types/channel"
import AllocationEditorHeader from "@/pages/signals/components/AllocationEditorHeader.vue"
import { extractSourceRowFromSignalMetadata, resolveAllSourceColumnHeaders, resolveSourceColumnInitialWidth, resolveSourceColumnMinWidth } from "@/pages/signals/utils/sourceColumns"
import AllocationChannelCell from "@/pages/signals/components/AllocationChannelCell.vue"
import AllocationChannelPickerPanel from "@/pages/signals/components/AllocationChannelPickerPanel.vue"
import AllocationControlCell from "@/pages/signals/components/AllocationControlCell.vue"
import SignalExportModal, { type ExportColumnOption } from "@/pages/signals/components/SignalExportModal.vue"
import SignalImportModal from "@/pages/signals/components/SignalImportModal.vue"
import { useSignalGridRowModel } from "@/pages/signals/composables/useSignalGridRowModel"
import { resolveSignalGridRowKey, resolveSignalGridSelectedRowKeys } from "@/pages/signals/utils/rowSelection"
import {
  createSignalGridRows,
  resolveSignalAllocationDisplayLabel,
  signalGridSourceColumnKey,
  type SignalGridRow as GridRow,
} from "@/pages/signals/utils/signalGridProjection"
import { createSignalAllocationProjectionCache } from "@/pages/signals/utils/signalAllocationProjectionCache"
import { createSignalGridPatchIngress } from "@/pages/signals/utils/signalGridPatchIngress"
import {
  normalizeSignalAllocationJobChangedRows,
  resolveSignalAllocationJobSkippedCount,
} from "@/pages/signals/utils/signalAllocationJobResult"
import {
  resolveSignalStaticRefreshReason,
  type SignalStaticRefreshReason,
} from "@/pages/signals/utils/signalStaticRefreshPolicy"
import { createSignalRuntimeStateCache } from "@/pages/signals/utils/signalRuntimeStateCache"
import {
  applyRuntimeTestedAtToRows,
  resolveRuntimeTestedAt,
} from "@/pages/signals/utils/runtimeProjection"
import { useAffinoDataGridTheme } from "@/components/ui/affinoDataGridTheme"
import "@/components/ui/affinoDataGridNative.css"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSignalJobStore } from "@/stores/signalJobStore"
import { useSignalRowsPatchStore } from "@/stores/signalRowsPatchStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useTestedAtRealtimeStore } from "@/stores/testedAtRealtimeStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { createLocalSettingsStringStorage, localSettingsKeys } from "@/services/localSettingsStorage"
import { formatDate } from "@/utils/datetime"
import { formatAoValue, parseAoInput } from "@/utils/channel"
import { resolveRuntimeChannelTypeForSignal } from "@/utils/signalRuntimeMapping"
import type { SignalAllocationJob, SignalAllocationRow } from "@/types/signal"
import type { SignalRowsPatchedEvent, SignalRowsPatchedRowPatch } from "@/types/ws/events"

const workspaceStore = useWorkspaceStore()
const signalSheetStore = useSignalSheetStore()
const signalJobStore = useSignalJobStore()
const signalRowsPatchStore = useSignalRowsPatchStore()
const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
const testedAtRealtimeStore = useTestedAtRealtimeStore()
const toastStore = useToastStore()
const route = useRoute()
const router = useRouter()
const { allocationRows, loadingAllocations, loadingSheet, sheet, allocationRevision, recentlyChangedSignalIds } = storeToRefs(signalSheetStore)
const { channels } = storeToRefs(channelStore)
const { activeJobs } = storeToRefs(signalJobStore)
const { activeWorkspaceRevision, activeWorkspacePatchedSignalIds } = storeToRefs(testedAtRealtimeStore)
const { activeWorkspacePatchEvent, activeWorkspacePatchRevision } = storeToRefs(signalRowsPatchStore)

const error = ref<string | null>(null)
const importModalOpen = ref(false)
const exportModalOpen = ref(false)
const allocationGridRef = useDataGridRef<GridRow>()
const allocationChannelPickerSignalId = ref<number | null>(null)
const allocationChannelPickerOpen = ref(false)
const allocationChannelPickerInstanceKey = ref(0)
const allocationChannelPickerLoading = ref(false)
const allocationChannelPickerSaving = ref(false)
const allocationChannelPickerError = ref<string | null>(null)
const rowSelectionState = ref<RowSelectionSnapshot | null>(null)
const rowSelectionProjectionRevision = ref(0)
const pendingSignalsGridSavedView = ref<string | DataGridSavedViewSnapshot<GridRow> | null>(null)
const restoringSignalsGridState = ref(false)
const signalsGridStatePersistenceReady = ref(false)
const refreshingSignalsStatic = ref(false)
const deletingSelected = ref(false)
const deletingProgressDone = ref(0)
const deletingProgressTotal = ref(0)
const deleteSelectedConfirmOpen = ref(false)
const deleteSelectedSignalIds = ref<number[]>([])
const allocatingSelected = ref(false)
const deallocatingSelected = ref(false)
const testRunInProgress = ref(false)
const testRunIntervalMs = ref(1000)
const testRunToggleMode = ref<"single" | "double">("single")
const activeAoControlSignalId = ref<number | null>(null)
const activeAoControlDraftValue = ref("")
const activeAoSubmittingSignalId = ref<number | null>(null)
const { gridLines, theme } = useAffinoDataGridTheme()

const SIGNAL_GRID_SKELETON_FIXED_HEIGHT = 88
const SIGNAL_GRID_SKELETON_ROW_HEIGHT = 36
const SIGNAL_GRID_SKELETON_FALLBACK_ROWS = 12
const BULK_ALLOCATION_COMPLETION_CHUNK_SIZE = 250
const signalGridSkeletonRef = ref<HTMLElement | null>(null)
const signalGridSkeletonHeight = ref(0)

type RowSelectionSnapshot = NonNullable<DataGridProps<GridRow>["rowSelectionState"]>
type DataGridStateUpdate = NonNullable<DataGridProps<Record<string, unknown>>["state"]>
type GridCellInteractiveContext = DataGridAppCellRendererContext<GridRow>["interactive"]

const DataGrid = defineDataGridComponent<GridRow>()
const SIGNAL_GRID_ROW_SELECTION = { enabled: true, columnWidth: 44 } satisfies NonNullable<DataGridProps<GridRow>["rowSelection"]>

const SIGNALS_GRID_LEGACY_STORAGE_KEY_PREFIX = "unitlab.signals-grid"
const REMOVED_SIGNAL_GRID_COLUMN_KEYS = new Set(["allocation_status", "allocation_health"])
const SIGNAL_GRID_PATCH_COLUMNS = ["internal_signal_type", "channel_select", "tested_at"] as const
const signalAllocationProjectionCache = createSignalAllocationProjectionCache()
const signalAllocationProjectionVersion = ref(0)
const signalRuntimeStateCache = createSignalRuntimeStateCache()
const signalRuntimeStateVersion = ref(0)
const signalsGridSavedViewStorage = createLocalSettingsStringStorage({
  resolveLegacyKeys: resolveSignalsGridSavedViewLegacyKeys,
})
let suppressSignalsGridStateEventsDepth = 0
let signalsGridStatePersistTimer: ReturnType<typeof setTimeout> | null = null
const signalGridRowModel = useSignalGridRowModel<GridRow>(allocationGridRef, {
  defaultReason: "signals-grid-patch",
})

const SignalsSelectionToolbarModule = defineComponent({
  name: "SignalsSelectionToolbarModule",
  props: {
    selectedCount: {
      type: Number,
      required: true,
    },
    showClearSelection: {
      type: Boolean,
      required: true,
    },
    deleteDisabled: {
      type: Boolean,
      required: true,
    },
    deleteLabel: {
      type: String,
      required: true,
    },
    onClearSelection: {
      type: Function as PropType<() => void>,
      required: true,
    },
    onDeleteSelected: {
      type: Function as PropType<() => void>,
      required: true,
    },
  },
  setup(props) {
    return () => h("div", { class: "affino-native-data-grid__toolbar-module" }, [
      h("span", { class: "affino-native-data-grid__stat" }, `Selected: ${props.selectedCount}`),
      props.showClearSelection
        ? h(
          "button",
          {
            type: "button",
            class: "datagrid-app-toolbar__button",
            onClick: () => props.onClearSelection(),
          },
          "Clear selection",
        )
        : null,
      h(
        "button",
        {
          type: "button",
          class: "datagrid-app-toolbar__button affino-native-data-grid__toolbar-button--danger",
          disabled: props.deleteDisabled,
          onClick: () => props.onDeleteSelected(),
        },
        props.deleteLabel,
      ),
    ])
  },
})

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const loading = computed(() => (
  refreshingSignalsStatic.value
  || loadingAllocations.value
  || loadingSheet.value
))
const allocationGridReadyForDisplay = computed(() => (
  signalsGridStatePersistenceReady.value && !restoringSignalsGridState.value
))
const showSignalGridSkeleton = computed(() => (
  (loading.value && allocationProjectionRowsCount.value === 0)
  || !allocationGridReadyForDisplay.value
))
const signalGridSkeletonRowCount = computed(() => {
  const measuredHeight = signalGridSkeletonHeight.value
  if (measuredHeight <= 0) {
    return SIGNAL_GRID_SKELETON_FALLBACK_ROWS
  }
  const bodyHeight = Math.max(0, measuredHeight - SIGNAL_GRID_SKELETON_FIXED_HEIGHT)
  return Math.max(1, Math.ceil(bodyHeight / SIGNAL_GRID_SKELETON_ROW_HEIGHT))
})
const activeSignalSheet = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId
  const currentSheet = sheet.value
  if (!workspaceId || !currentSheet) {
    return null
  }
  return currentSheet.workspace_id === workspaceId ? currentSheet : null
})

function bumpSignalAllocationProjectionVersion() {
  signalAllocationProjectionVersion.value = signalAllocationProjectionCache.version
}

function signalAllocationProjectionRows(): readonly SignalAllocationRow[] {
  void signalAllocationProjectionVersion.value
  return signalAllocationProjectionCache.getRows()
}

function signalAllocationProjectionRowCount(): number {
  void signalAllocationProjectionVersion.value
  return signalAllocationProjectionCache.rowCount
}

function replaceSignalAllocationProjectionRows(rows: readonly SignalAllocationRow[]) {
  signalAllocationProjectionCache.replaceRows(rows)
  bumpSignalAllocationProjectionVersion()
}

function bumpSignalRuntimeStateVersion() {
  signalRuntimeStateVersion.value = signalRuntimeStateCache.version
}

function patchSignalRuntimeStateFromStore(signalIds: readonly number[]) {
  const testedAtBySignal: Record<number, string> = {}
  signalIds.forEach((rawSignalId) => {
    const signalId = Number(rawSignalId)
    if (!Number.isFinite(signalId) || signalId <= 0) {
      return
    }
    const testedAt = String(testedAtRealtimeStore.getTestedAt(signalId, workspaceStore.activeWorkspaceId) ?? "").trim()
    if (testedAt) {
      testedAtBySignal[signalId] = testedAt
    }
  })
  const result = signalRuntimeStateCache.patchTestedAtBySignal(testedAtBySignal)
  if (result.changed > 0) {
    bumpSignalRuntimeStateVersion()
  }
  return result
}

function resolveStoreAllocationRowsBySignalIds(signalIds: readonly number[]): SignalAllocationRow[] {
  if (!signalIds.length) {
    return []
  }
  const pending = new Set(signalIds.map(item => Number(item)).filter(Number.isFinite))
  if (pending.size === 0) {
    return []
  }
  const rows: SignalAllocationRow[] = []
  allocationRows.value.forEach((row) => {
    if (!pending.has(row.signal_id)) {
      return
    }
    rows.push(row)
    pending.delete(row.signal_id)
  })
  return rows
}

function applyStoreSignalGridAllocationPatches(signalIds: readonly number[]) {
  const rows = resolveStoreAllocationRowsBySignalIds(signalIds)
  if (rows.length > 0) {
    signalGridPatchIngress.applyAllocationRows(rows, {
      reason: "signal-allocation-row-patch",
      columns: SIGNAL_GRID_PATCH_COLUMNS,
    })
  }
}

function getSignalAllocationProjectionRowBySignalId(signalId: number | null | undefined): SignalAllocationRow | null {
  void signalAllocationProjectionVersion.value
  const normalizedSignalId = Number(signalId)
  if (!Number.isFinite(normalizedSignalId)) {
    return null
  }
  return signalAllocationProjectionCache.getRowBySignalId(normalizedSignalId)
}

function hasSignalAllocationProjectionSignalId(signalId: number | null | undefined): boolean {
  return getSignalAllocationProjectionRowBySignalId(signalId) !== null
}

function getSignalAllocationOwnerSignalIdByChannelId(channelId: number | null | undefined): number | null {
  void signalAllocationProjectionVersion.value
  const normalizedChannelId = Number(channelId)
  if (!Number.isFinite(normalizedChannelId)) {
    return null
  }
  return signalAllocationProjectionCache.getOwnerSignalIdByChannelId(normalizedChannelId)
}

const allocationProjectionRowsCount = computed(() => signalAllocationProjectionRowCount())

function getSignalRuntimeTestedAt(signalId: number | null | undefined): string | null {
  void signalRuntimeStateVersion.value
  return signalRuntimeStateCache.getTestedAt(signalId)
}

const sourceHeaders = computed(() => {
  const headersFromSheet = resolveAllSourceColumnHeaders(activeSignalSheet.value, [])
  if (headersFromSheet.length > 0) {
    return headersFromSheet
  }
  return resolveAllSourceColumnHeaders(null, signalAllocationProjectionRows())
})
const signalGridPatchIngress = createSignalGridPatchIngress({
  cache: signalAllocationProjectionCache,
  rowModel: signalGridRowModel,
  getHeaders: () => sourceHeaders.value,
  getRuntime: signalGridRuntimeOverlay,
  onProjectionChanged: bumpSignalAllocationProjectionVersion,
  defaultColumns: SIGNAL_GRID_PATCH_COLUMNS,
})

watch(
  signalGridSkeletonRef,
  (element, _previousElement, onCleanup) => {
    signalGridSkeletonHeight.value = element?.getBoundingClientRect().height ?? 0
    if (!element || typeof ResizeObserver === "undefined") {
      return
    }

    const observer = new ResizeObserver((entries) => {
      signalGridSkeletonHeight.value = entries[0]?.contentRect.height ?? element.getBoundingClientRect().height
    })
    observer.observe(element)

    let frame: number | null = window.requestAnimationFrame(() => {
      signalGridSkeletonHeight.value = element.getBoundingClientRect().height
      frame = null
    })

    onCleanup(() => {
      observer.disconnect()
      if (frame !== null) {
        window.cancelAnimationFrame(frame)
      }
    })
  },
  { flush: "post" },
)

function resolveRuntimeAllocationRows(rows: readonly SignalAllocationRow[] = signalAllocationProjectionRows()): SignalAllocationRow[] {
  return applyRuntimeTestedAtToRows(rows, workspaceStore.activeWorkspaceId, getSignalRuntimeTestedAt)
}

function hasRuntimeOrStaticTestedAt(row: SignalAllocationRow): boolean {
  return Boolean(String(
    resolveRuntimeTestedAt(row, workspaceStore.activeWorkspaceId, getSignalRuntimeTestedAt) ?? "",
  ).trim())
}

const summaryText = computed(() => {
  if (workspaceMissing.value) {
    return "Workspace is not selected"
  }
  if (loading.value) {
    return "Loading static signals view"
  }
  void signalRuntimeStateVersion.value
  const rows = signalAllocationProjectionRows()
  const total = rows.length
  const allocated = rows.filter(row => Number.isFinite(row.channel_id as number)).length
  const tested = rows.filter(row => hasRuntimeOrStaticTestedAt(row)).length
  const remaining = Math.max(0, total - tested)
  const allocatedPercent = total > 0 ? ((allocated / total) * 100) : 0
  const testedPercent = total > 0 ? ((tested / total) * 100) : 0
  const remainingPercent = total > 0 ? ((remaining / total) * 100) : 0
  const formatPercent = (value: number) => {
    const rounded = Math.round(value * 10) / 10
    return Number.isInteger(rounded) ? rounded.toFixed(0) : rounded.toFixed(1)
  }
  return `${total} signals · ${allocated} allocated (${formatPercent(allocatedPercent)}%) · ${tested} tested (${formatPercent(testedPercent)}%) · ${remaining} remaining (${formatPercent(remainingPercent)}%)`
})

const clientRowModelOptions: NonNullable<DataGridProps<GridRow>["clientRowModelOptions"]> = {
  resolveRowId: row => row.rowId,
}

const selectedAllocationRows = computed(() => (
  selectedRowKeys.value
    .map((rowKey) => {
      const signalId = signalIdFromRowKey(rowKey)
      if (signalId === null) return null
      if (!hasSignalAllocationProjectionSignalId(signalId)) return null
      return getSignalAllocationProjectionRowBySignalId(signalId)
    })
    .filter((row): row is SignalAllocationRow => Boolean(row))
))

function resolveSelectedUnassignedSignalIdsInSelectionOrder(): number[] {
  const orderedIds: number[] = []
  const seen = new Set<number>()
  selectedAllocationRows.value.forEach((row) => {
    const signalId = Number(row.signal_id)
    if (!Number.isFinite(signalId) || seen.has(signalId)) {
      return
    }
    if (Number.isFinite(row.channel_id as number)) {
      return
    }
    seen.add(signalId)
    orderedIds.push(signalId)
  })
  return orderedIds
}

const selectedUnassignedSignalIds = computed(() => (
  resolveSelectedUnassignedSignalIdsInSelectionOrder()
))

const selectedAllocatedSignalIds = computed(() => (
  selectedAllocationRows.value
    .filter(row => Number.isFinite(row.channel_id as number))
    .map(row => row.signal_id)
))

const selectedVisibleAllocatedPhysicalRows = computed(() => (
  selectedAllocationRows.value.filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.device_id as number)
    && Boolean(row.unit_id)
  ))
))

const allocatedCableRows = computed(() => (
  signalAllocationProjectionRows().filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.channel_index as number)
    && Boolean(String(row.unit_id ?? "").trim())
  ))
))

const activeTestRunJob = computed(() => (
  activeJobs.value.find(job => String(job.operation) === "test_run") ?? null
))

const allocateSelectedButtonLabel = computed(() => (
  allocatingSelected.value ? "Assigning..." : "Assign Hardware"
))

const deallocateSelectedButtonLabel = computed(() => (
  deallocatingSelected.value ? "Unassigning..." : "Unassign Hardware"
))

function getAllocationJobChangedRows(job: SignalAllocationJob): SignalAllocationRow[] {
  return normalizeSignalAllocationJobChangedRows(job, getSignalAllocationProjectionRowBySignalId)
}

const SIGNAL_ROWS_PATCH_FIELD_GRID_COLUMNS: Record<string, readonly string[]> = {
  channel_id: ["channel_select", "control"],
  channel_type: ["internal_signal_type", "control"],
  channel_index: ["channel_select", "control"],
  channel_label: ["channel_select"],
  device_id: ["channel_select", "control"],
  unit_id: ["channel_select", "control"],
  unit_online: ["channel_select", "control"],
  tested_at: ["tested_at"],
}

function resolveSignalRowsPatchedChanges(patch: SignalRowsPatchedRowPatch): Record<string, unknown> {
  const changes = patch.changes
  if (!changes || typeof changes !== "object" || Array.isArray(changes)) {
    return {}
  }
  return changes
}

function uniqueGridPatchColumns(columns: readonly unknown[]): string[] {
  const seen = new Set<string>()
  const normalized: string[] = []
  columns.forEach((column) => {
    const key = String(column ?? "").trim()
    if (!key || seen.has(key)) {
      return
    }
    seen.add(key)
    normalized.push(key)
  })
  return normalized
}

function resolveSignalRowsPatchedColumns(patch: SignalRowsPatchedRowPatch): readonly string[] {
  const explicitColumns = uniqueGridPatchColumns(Array.isArray(patch.columns) ? patch.columns : [])
  if (explicitColumns.length > 0) {
    return explicitColumns
  }

  const inferredColumns = uniqueGridPatchColumns(
    Object.keys(resolveSignalRowsPatchedChanges(patch))
      .flatMap(key => SIGNAL_ROWS_PATCH_FIELD_GRID_COLUMNS[key] ?? []),
  )
  return inferredColumns.length > 0 ? inferredColumns : SIGNAL_GRID_PATCH_COLUMNS
}

function buildSignalRowsPatchedAllocationRow(patch: SignalRowsPatchedRowPatch): SignalAllocationRow | null {
  const signalId = Number(patch.signal_id)
  if (!Number.isFinite(signalId) || signalId <= 0) {
    return null
  }

  const baseRow = getSignalAllocationProjectionRowBySignalId(signalId)
  if (!baseRow) {
    return null
  }

  const changes = resolveSignalRowsPatchedChanges(patch) as Partial<SignalAllocationRow>
  const merged = {
    ...baseRow,
    ...changes,
  } as SignalAllocationRow

  return {
    ...merged,
    signal_id: signalId,
    row_id: String(patch.row_id ?? merged.row_id ?? `signal-${signalId}`),
    signal_key: String(merged.signal_key ?? ""),
    signal_name: String(merged.signal_name ?? ""),
    signal_direction: (merged.signal_direction ?? "DI") as SignalAllocationRow["signal_direction"],
    signal_category: merged.signal_category ?? null,
    signal_metadata: merged.signal_metadata && typeof merged.signal_metadata === "object" && !Array.isArray(merged.signal_metadata)
      ? merged.signal_metadata
      : {},
    allocation_health: merged.allocation_health ?? null,
    channel_id: merged.channel_id ?? null,
    channel_type: merged.channel_type ?? null,
    channel_index: merged.channel_index ?? null,
    channel_label: merged.channel_label ?? null,
    device_id: merged.device_id ?? null,
    unit_id: merged.unit_id ?? null,
    unit_online: merged.unit_online ?? null,
    unit_last_seen_at: merged.unit_last_seen_at ?? null,
    tested_at: merged.tested_at ?? null,
  }
}

function applySignalRowsPatchedRuntimeRows(patches: readonly SignalRowsPatchedRowPatch[]): number[] {
  const signalIds: number[] = []
  const testedAtBySignal: Record<number, string> = {}
  const columns = new Set<string>()
  const missingSignalIds: number[] = []

  patches.forEach((patch) => {
    const signalId = Number(patch.signal_id)
    if (!Number.isFinite(signalId) || signalId <= 0) {
      return
    }
    if (!hasSignalAllocationProjectionSignalId(signalId)) {
      missingSignalIds.push(signalId)
      return
    }

    signalIds.push(signalId)
    resolveSignalRowsPatchedColumns(patch).forEach(column => columns.add(column))

    const testedAt = resolveSignalRowsPatchedChanges(patch).tested_at
    if (testedAt !== undefined && testedAt !== null) {
      testedAtBySignal[signalId] = String(testedAt)
      columns.add("tested_at")
    }
  })

  const runtimePatchResult = signalRuntimeStateCache.patchTestedAtBySignal(testedAtBySignal)
  if (runtimePatchResult.changed > 0) {
    bumpSignalRuntimeStateVersion()
  }

  if (signalIds.length > 0) {
    signalGridPatchIngress.applyRuntimeSignals(signalIds, {
      reason: "signal-rows-patched:test-runtime",
      columns: columns.size > 0 ? [...columns] : ["tested_at"],
    })
  }

  return missingSignalIds
}

function applySignalRowsPatchedAllocationRows(
  patches: readonly SignalRowsPatchedRowPatch[],
  source: SignalRowsPatchedEvent["source"],
): number[] {
  const groupedRows = new Map<string, { columns: readonly string[]; rows: SignalAllocationRow[] }>()
  const missingSignalIds: number[] = []

  patches.forEach((patch) => {
    const signalId = Number(patch.signal_id)
    if (!Number.isFinite(signalId) || signalId <= 0) {
      return
    }
    if (!hasSignalAllocationProjectionSignalId(signalId)) {
      missingSignalIds.push(signalId)
      return
    }

    const row = buildSignalRowsPatchedAllocationRow(patch)
    if (!row) {
      return
    }

    const columns = resolveSignalRowsPatchedColumns(patch)
    const groupKey = columns.join("\u0000")
    const group = groupedRows.get(groupKey)
    if (group) {
      group.rows.push(row)
      return
    }
    groupedRows.set(groupKey, { columns, rows: [row] })
  })

  groupedRows.forEach((group) => {
    signalGridPatchIngress.applyAllocationRows(group.rows, {
      reason: source === "device_health"
        ? "signal-rows-patched:device-health"
        : "signal-rows-patched:allocation",
      columns: group.columns,
    })
  })

  return missingSignalIds
}

async function applySignalRowsPatchedEvent(event: SignalRowsPatchedEvent) {
  if (Number(event.workspace_id) !== workspaceStore.activeWorkspaceId) {
    return
  }

  const explicitRefreshReason = resolveSignalStaticRefreshReason({
    kind: "signal_rows_patched",
    requiresFullReload: event.requires_full_reload === true,
  })
  if (explicitRefreshReason) {
    await refreshSignalsStatic(explicitRefreshReason)
    return
  }

  const patches = Array.isArray(event.patches) ? event.patches : []
  if (patches.length === 0) {
    return
  }

  const missingSignalIds = event.source === "test_runtime"
    ? applySignalRowsPatchedRuntimeRows(patches)
    : applySignalRowsPatchedAllocationRows(patches, event.source)

  const recoveryRefreshReason = resolveSignalStaticRefreshReason({
    kind: "signal_rows_patched",
    missingSignalIds,
  })
  if (recoveryRefreshReason) {
    await refreshSignalsStatic(recoveryRefreshReason)
  }
}

async function syncCompletedAllocationProjectionRows(rows: readonly SignalAllocationRow[]) {
  suppressSignalsGridStateEventsDepth += 1
  try {
    for (let index = 0; index < rows.length; index += BULK_ALLOCATION_COMPLETION_CHUNK_SIZE) {
      const chunk = rows.slice(index, index + BULK_ALLOCATION_COMPLETION_CHUNK_SIZE)
      signalGridPatchIngress.patchAllocationRowsCache(chunk)
      signalGridPatchIngress.refreshSignalCells(chunk.map(row => row.signal_id), SIGNAL_GRID_PATCH_COLUMNS, {
        reason: "signal-allocation-job-complete",
      })
      await awaitUiPaintFrame()
    }
    await awaitUiPaintFrame()
  } finally {
    suppressSignalsGridStateEventsDepth = Math.max(0, suppressSignalsGridStateEventsDepth - 1)
  }
}

async function applyCompletedAllocationJobPatch(job: SignalAllocationJob): Promise<SignalAllocationRow[]> {
  const changedRows = getAllocationJobChangedRows(job)
  if (!changedRows.length) {
    return changedRows
  }

  await syncCompletedAllocationProjectionRows(changedRows)

  return changedRows
}

const deleteSelectedToolbarLabel = computed(() => (
  deletingSelected.value
    ? `Deleting ${deletingProgressDone.value}/${deletingProgressTotal.value}...`
    : "Delete selected"
))

const deleteSelectedConfirmMessage = computed(() => {
  const count = deleteSelectedSignalIds.value.length
  const noun = count === 1 ? "signal row" : "signal rows"
  return `Delete ${count} selected ${noun}? This removes the rows from the active signal list and cannot be undone from this grid.`
})

const selectedRowKeys = computed(() => (
  resolveSignalGridSelectedRowKeys(rowSelectionState.value, resolveSelectionCandidateRowKeys())
))

function resolveSelectionCandidateRowKeys(): string[] {
  void allocationRevision.value
  void rowSelectionProjectionRevision.value

  const api = allocationGridRef.value?.getApi()
  if (api) {
    return api.rows.getProjectedRows()
      .map((row: GridRow) => resolveSignalGridRowKey(row))
      .filter((rowKey): rowKey is string => Boolean(rowKey))
  }

  return signalAllocationProjectionRows()
    .map(row => resolveSignalGridRowKey(row))
    .filter((rowKey): rowKey is string => Boolean(rowKey))
}

const selectedVisibleRowCount = computed(() => selectedAllocationRows.value.length)

const toolbarModules = computed<DataGridAppToolbarModule[]>(() => ([
  {
    key: "signals-selection-actions",
    component: SignalsSelectionToolbarModule,
    props: {
      selectedCount: selectedVisibleRowCount.value,
      showClearSelection: selectedRowKeys.value.length > 0,
      deleteDisabled: deletingSelected.value || deleteSelectedConfirmOpen.value || selectedVisibleRowCount.value === 0,
      deleteLabel: deleteSelectedToolbarLabel.value,
      onClearSelection: clearGridSelection,
      onDeleteSelected: () => {
        openDeleteSelectedConfirm()
      },
    },
  },
]))

function getSignalsGridStorageKey(workspaceId: number | null): string | null {
  if (!Number.isFinite(workspaceId as number) || Number(workspaceId) <= 0) {
    return null
  }
  return localSettingsKeys.signalsGridSavedView(Number(workspaceId))
}

function resolveSignalsGridSavedViewLegacyKeys(key: string): readonly string[] {
  const match = /^signals\.grid\.savedView\.workspace\.([1-9]\d*)$/.exec(key)
  return match ? [`${SIGNALS_GRID_LEGACY_STORAGE_KEY_PREFIX}:workspace:${match[1]}`] : []
}

function persistSignalsGridState() {
  if (
    !signalsGridStatePersistenceReady.value
    || restoringSignalsGridState.value
    || pendingSignalsGridSavedView.value
    || loading.value
  ) {
    return
  }

  const storageKey = getSignalsGridStorageKey(workspaceStore.activeWorkspaceId)
  if (!storageKey) {
    return
  }

  const savedView = allocationGridRef.value?.getSavedView?.()
  if (!savedView) {
    return
  }

  writeDataGridSavedViewToStorage(signalsGridSavedViewStorage, storageKey, savedView)
}

function persistSignalsGridSavedView(savedView: DataGridSavedViewSnapshot<GridRow>) {
  const storageKey = getSignalsGridStorageKey(workspaceStore.activeWorkspaceId)
  if (!storageKey) {
    return
  }
  writeDataGridSavedViewToStorage(signalsGridSavedViewStorage, storageKey, savedView)
}

function withClearedSignalsGridSelection(
  savedView: DataGridSavedViewSnapshot<GridRow>,
): DataGridSavedViewSnapshot<GridRow> {
  return {
    ...savedView,
    state: {
      ...savedView.state,
      rowSelection: {
        focusedRow: null,
        selectedRows: [],
      },
    },
  }
}

function captureSignalsGridSavedViewForStaticMutation(): DataGridSavedViewSnapshot<GridRow> | null {
  const savedView = allocationGridRef.value?.getSavedView?.()
  if (!savedView) {
    return null
  }
  return withClearedSignalsGridSelection(filterRemovedSignalsGridColumnsFromSavedView(savedView))
}

async function restoreSignalsGridSavedViewAfterStaticMutation(savedView: DataGridSavedViewSnapshot<GridRow> | null) {
  if (!savedView) {
    return
  }

  await nextTick()
  if (allocationProjectionRowsCount.value <= 0) {
    return
  }

  pendingSignalsGridSavedView.value = savedView
  tryApplyPendingSignalsGridSavedView()

  if (pendingSignalsGridSavedView.value) {
    await awaitUiPaintFrame()
    tryApplyPendingSignalsGridSavedView()
  }
}

function scheduleSignalsGridStatePersist() {
  if (signalsGridStatePersistTimer !== null) {
    clearTimeout(signalsGridStatePersistTimer)
  }
  signalsGridStatePersistTimer = setTimeout(() => {
    signalsGridStatePersistTimer = null
    persistSignalsGridState()
  }, 120)
}

function normalizeSelectionKeyList(values: readonly unknown[] | undefined): string[] {
  return (values ?? [])
    .map(value => String(value ?? "").trim())
    .filter(Boolean)
}

function sameSelectionKeyList(left: readonly unknown[] | undefined, right: readonly unknown[] | undefined): boolean {
  const leftKeys = normalizeSelectionKeyList(left)
  const rightKeys = normalizeSelectionKeyList(right)
  if (leftKeys.length !== rightKeys.length) {
    return false
  }
  return leftKeys.every((key, index) => key === rightKeys[index])
}

function areRowSelectionSnapshotsEqual(left: RowSelectionSnapshot | null, right: RowSelectionSnapshot | null): boolean {
  if (left === right) {
    return true
  }
  if (!left || !right) {
    return !left && !right
  }
  return String(left.mode ?? "") === String(right.mode ?? "")
    && String(left.focusedRow ?? "") === String(right.focusedRow ?? "")
    && sameSelectionKeyList(left.selectedRows, right.selectedRows)
    && sameSelectionKeyList(left.excludedRows, right.excludedRows)
}

function applyRowSelectionStateSnapshot(nextSelection: RowSelectionSnapshot | null): boolean {
  if (areRowSelectionSnapshotsEqual(rowSelectionState.value, nextSelection)) {
    return false
  }
  rowSelectionState.value = nextSelection
  rowSelectionProjectionRevision.value += 1
  return true
}

function markSignalsGridStateRestored() {
  restoringSignalsGridState.value = false
  signalsGridStatePersistenceReady.value = true
}

function isRemovedSignalsGridColumnKey(key: unknown): boolean {
  return REMOVED_SIGNAL_GRID_COLUMN_KEYS.has(String(key ?? "").trim())
}

function filterRemovedSignalsGridColumnKeys(keys: readonly string[] | undefined): string[] {
  if (!Array.isArray(keys)) return []
  return keys.filter(key => !isRemovedSignalsGridColumnKey(key))
}

function filterRemovedSignalsGridColumnRecord<T>(record: Readonly<Record<string, T>>): Record<string, T> {
  return Object.fromEntries(
    Object.entries(record).filter(([key]) => !isRemovedSignalsGridColumnKey(key)),
  ) as Record<string, T>
}

function filterRemovedSignalsGridColumnsFromSavedView(
  savedView: DataGridSavedViewSnapshot<GridRow>,
): DataGridSavedViewSnapshot<GridRow> {
  const columns = savedView.state.columns
  const zoneOrder = columns.zoneOrder
    ? Object.fromEntries(
      Object.entries(columns.zoneOrder).map(([zone, keys]) => [
        zone,
        filterRemovedSignalsGridColumnKeys(keys),
      ]),
    ) as typeof columns.zoneOrder
    : columns.zoneOrder

  return {
    ...savedView,
    state: {
      ...savedView.state,
      columns: {
        ...columns,
        order: filterRemovedSignalsGridColumnKeys(columns.order),
        zoneOrder,
        visibility: filterRemovedSignalsGridColumnRecord(columns.visibility),
        widths: filterRemovedSignalsGridColumnRecord(columns.widths),
        pins: filterRemovedSignalsGridColumnRecord(columns.pins),
      },
    },
  }
}

function areSavedViewColumnsReady(savedView: DataGridSavedViewSnapshot<GridRow>): boolean {
  const currentColumnKeys = new Set(
    resolvedColumns.value
      .map(column => String(column.key ?? "").trim())
      .filter(Boolean),
  )

  if (currentColumnKeys.size === 0) {
    return false
  }

  const savedOrder = savedView.state.columns.order
    .map((key) => String(key ?? "").trim())
    .filter(Boolean)

  return savedOrder.every(key => currentColumnKeys.has(key))
}

function tryApplyPendingSignalsGridSavedView() {
  const pendingSavedView = pendingSignalsGridSavedView.value
  if (!pendingSavedView) {
    if (restoringSignalsGridState.value && !loading.value) {
      markSignalsGridStateRestored()
    }
    return
  }

  if (loading.value || workspaceMissing.value) {
    return
  }

  const grid = allocationGridRef.value
  if (!grid) {
    return
  }

  const migratedSavedView = typeof pendingSavedView === "string"
    ? parseDataGridSavedView<GridRow>(pendingSavedView, grid.migrateState)
    : grid.migrateSavedView(pendingSavedView)

  if (!migratedSavedView) {
    rowSelectionState.value = null
    pendingSignalsGridSavedView.value = null
    markSignalsGridStateRestored()
    return
  }

  const compatibleSavedView = filterRemovedSignalsGridColumnsFromSavedView(migratedSavedView)

  rowSelectionState.value = compatibleSavedView.state.rowSelection ?? null

  if (!areSavedViewColumnsReady(compatibleSavedView)) {
    pendingSignalsGridSavedView.value = compatibleSavedView
    if (!loading.value && resolvedColumns.value.length > 0) {
      pendingSignalsGridSavedView.value = null
      markSignalsGridStateRestored()
    }
    return
  }

  const applied = grid.applySavedView(compatibleSavedView)
  if (applied) {
    pendingSignalsGridSavedView.value = null
    markSignalsGridStateRestored()
  }
}

function restoreSignalsGridState() {
  restoringSignalsGridState.value = true
  signalsGridStatePersistenceReady.value = false

  const storageKey = getSignalsGridStorageKey(workspaceStore.activeWorkspaceId)
  if (!storageKey) {
    rowSelectionState.value = null
    pendingSignalsGridSavedView.value = null
    markSignalsGridStateRestored()
    return
  }

  const raw = signalsGridSavedViewStorage.getItem(storageKey)
  if (!raw) {
    rowSelectionState.value = null
    pendingSignalsGridSavedView.value = null
    markSignalsGridStateRestored()
    return
  }

  rowSelectionState.value = null
  pendingSignalsGridSavedView.value = raw
  tryApplyPendingSignalsGridSavedView()
}

const isTestRunBusy = computed(() => {
  const status = String(activeTestRunJob.value?.status ?? "")
  const activeJobBusy = status === "queued" || status === "running" || status === "cancelling"
  return testRunInProgress.value || activeJobBusy
})

const canResumeActiveTestRun = computed(() => activeTestRunJob.value?.status === "paused")

type CableExportColumnDef = {
  key: string
  label: string
  required: boolean
  getValue: (row: SignalAllocationRow, terminalHeader: string | null) => string
}

const cableExportColumnDefs = computed<CableExportColumnDef[]>(() => {
  const sourceColumnDefs: CableExportColumnDef[] = sourceHeaders.value.map((header) => ({
    key: `source:${header}`,
    label: header,
    required: false,
    getValue: (row: SignalAllocationRow) => {
      const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
      const rawValue = sourceRow[header]
      return rawValue === undefined || rawValue === null ? "" : String(rawValue)
    },
  }))

  return [
    {
      key: "unit_id",
      label: "unit_id",
      required: true,
      getValue: (row: SignalAllocationRow) => String(row.unit_id ?? ""),
    },
    {
      key: "terminal",
      label: "terminal",
      required: true,
      getValue: (row: SignalAllocationRow, terminalHeader: string | null) => resolveTerminalValue(row, terminalHeader),
    },
    {
      key: "channel_index",
      label: "channel_index",
      required: true,
      getValue: (row: SignalAllocationRow) => (
        Number.isFinite(row.channel_index as number) ? String(Number(row.channel_index)) : ""
      ),
    },
    {
      key: "channel_label",
      label: "channel_label",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.channel_label ?? ""),
    },
    {
      key: "signal_name",
      label: "signal_name",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_name ?? ""),
    },
    {
      key: "signal_key",
      label: "signal_key",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_key ?? ""),
    },
    {
      key: "signal_direction",
      label: "signal_direction",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_direction ?? ""),
    },
    {
      key: "signal_category",
      label: "signal_category",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_category ?? ""),
    },
    {
      key: "last_tested_at",
      label: "last_tested_at",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.tested_at ?? ""),
    },
    ...sourceColumnDefs,
  ]
})

const requiredExportColumnOptions = computed<ExportColumnOption[]>(() => (
  cableExportColumnDefs.value
    .filter(column => column.required)
    .map(column => ({ key: column.key, label: column.label }))
))

const optionalExportColumnOptions = computed<ExportColumnOption[]>(() => (
  cableExportColumnDefs.value
    .filter(column => !column.required)
    .map(column => ({ key: column.key, label: column.label }))
))

const channelMap = computed(() => {
  const map = new Map<number, typeof channels.value[number]>()
  channels.value.forEach((channel) => {
    map.set(channel.id, channel)
  })
  return map
})

const deviceStatusById = computed(() => {
  const map = new Map<number, string>()
  deviceStore.devices.forEach((device) => {
    map.set(device.id, device.status)
  })
  return map
})

const deviceNameById = computed(() => {
  const map = new Map<number, string>()
  deviceStore.devices.forEach((device) => {
    map.set(device.id, String(device.name ?? "").trim())
  })
  return map
})

const channelUnitById = computed(() => {
  const map = new Map<number, string>()
  channels.value.forEach((channel) => {
    const unitId = channelStore.resolveUnitId(channel.device_id)
    map.set(channel.id, unitId || `Device ${channel.device_id}`)
  })
  return map
})

function invokeRenderedCellAction(interactive: GridCellInteractiveContext) {
  if (!interactive?.enabled) {
    return
  }
  interactive.activate("click")
}

function handleAllocationGridStateUpdate(state: DataGridStateUpdate | null) {
  if (!signalsGridStatePersistenceReady.value || restoringSignalsGridState.value || loading.value) {
    return
  }
  if (suppressSignalsGridStateEventsDepth > 0) {
    return
  }

  applyRowSelectionStateSnapshot(state?.rowSelection ?? null)
  scheduleSignalsGridStatePersist()
}

function handleAllocationRowSelectionStateUpdate(state: RowSelectionSnapshot | null) {
  if (!signalsGridStatePersistenceReady.value || restoringSignalsGridState.value || loading.value) {
    return
  }
  if (suppressSignalsGridStateEventsDepth > 0) {
    return
  }

  applyRowSelectionStateSnapshot(state)
  scheduleSignalsGridStatePersist()
}

function signalIdFromRowKey(rowKey: string): number | null {
  const match = String(rowKey).match(/^signal-(\d+)$/)
  if (!match) {
    return null
  }
  const parsed = Number(match[1])
  return Number.isFinite(parsed) ? parsed : null
}

function clearGridSelection() {
  rowSelectionState.value = {
    focusedRow: null,
    selectedRows: [],
  }
}

function isImportQueryRequested(raw: unknown): boolean {
  const values = Array.isArray(raw) ? raw : [raw]
  return values.some((value) => {
    const normalized = String(value ?? "").trim().toLowerCase()
    return normalized === "1" || normalized === "true" || normalized === "yes" || normalized === "open"
  })
}

function clearImportQueryFlag() {
  if (!("import" in route.query)) {
    return
  }
  const nextQuery = { ...route.query }
  delete nextQuery.import
  void router.replace({ query: nextQuery }).catch(() => {
    return
  })
}

function syncImportModalFromRoute() {
  if (!isImportQueryRequested(route.query.import)) {
    return
  }
  importModalOpen.value = true
  clearImportQueryFlag()
}

async function awaitUiPaintFrame() {
  await nextTick()
  await new Promise<void>((resolve) => {
    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(() => resolve())
      return
    }
    setTimeout(() => resolve(), 0)
  })
}

function csvEscape(value: unknown): string {
  const text = String(value ?? "")
  if (/[",\n\r]/.test(text)) {
    return `"${text.replace(/"/g, '""')}"`
  }
  return text
}

function toFilenamePart(value: string | null | undefined): string {
  const normalized = String(value ?? "")
    .trim()
    .replace(/\s+/g, "-")
    .replace(/[^a-zA-Z0-9._-]/g, "-")
    .replace(/-+/g, "-")
    .replace(/^-+|-+$/g, "")
  return normalized || "workspace"
}

function downloadTextFile(content: string, filename: string, mimeType = "text/csv;charset=utf-8;") {
  const blob = new Blob([`\uFEFF${content}`], { type: mimeType })
  const url = URL.createObjectURL(blob)
  const link = document.createElement("a")
  link.href = url
  link.download = filename
  document.body.appendChild(link)
  link.click()
  link.remove()
  URL.revokeObjectURL(url)
}

function resolveTerminalHeader(): string | null {
  const fromImportMeta = String(activeSignalSheet.value?.import_meta?.terminal_column ?? "").trim()
  if (fromImportMeta) {
    return fromImportMeta
  }

  const byHeuristic = sourceHeaders.value.find((header) => /terminal|клем|клемм|xt/i.test(String(header)))
  if (byHeuristic) {
    return String(byHeuristic)
  }

  return null
}

function resolveTerminalValue(row: SignalAllocationRow, terminalHeader: string | null): string {
  if (!terminalHeader) return ""
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const rawValue = sourceRow[terminalHeader]
  if (rawValue === undefined || rawValue === null) return ""
  return String(rawValue)
}

function resolveSelectedCableExportColumns(optionalColumnKeys: readonly string[]): CableExportColumnDef[] {
  const byKey = new Map(cableExportColumnDefs.value.map(column => [column.key, column] as const))
  const requiredStart = ["unit_id", "channel_index"]
    .map(key => byKey.get(key))
    .filter((column): column is CableExportColumnDef => Boolean(column))
  const terminalColumn = byKey.get("terminal")

  const optional = optionalColumnKeys
    .map(key => byKey.get(key))
    .filter((column): column is CableExportColumnDef => Boolean(column && !column.required))

  return terminalColumn
    ? [...requiredStart, ...optional, terminalColumn]
    : [...requiredStart, ...optional]
}

function buildSignalReportRows(rows: readonly SignalAllocationRow[] = resolveRuntimeAllocationRows()): string[][] {
  const headers = sourceHeaders.value
  const fallbackHeaders = headers.length > 0 ? headers : ["signal_name", "signal_key"]

  return rows.map((row) => {
    const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
    const sourceCells = fallbackHeaders.map((header) => {
      if (header === "signal_name") return row.signal_name
      if (header === "signal_key") return row.signal_key
      return sourceRow[header] ?? ""
    })
    const channelNumber = Number.isFinite(row.channel_index as number) ? Number(row.channel_index) : ""
    return [
      ...sourceCells.map(item => String(item ?? "")),
      String(row.signal_direction ?? ""),
      String(row.unit_id ?? ""),
      String(channelNumber),
      String(row.tested_at ?? ""),
    ]
  })
}

function openImportModal() {
  if (workspaceMissing.value) return
  importModalOpen.value = true
}

function closeImportModal() {
  importModalOpen.value = false
}

async function handleImported() {
  importModalOpen.value = false
  await refreshSignalsStatic("import")
  await signalSheetStore.ensurePresetsLoaded({ force: true })
}

function exportCableJournal(optionalColumnKeys: string[] = []) {
  const cableRows = allocatedCableRows.value
  if (!cableRows.length) {
    toastStore.info("No allocated rows to export.")
    return
  }

  const workspaceName = String(workspaceStore.activeWorkspace?.name ?? "")
  const workspaceFilePart = toFilenamePart(workspaceName)
  const generatedAt = new Date()
  const terminalHeader = resolveTerminalHeader()
  const selectedColumns = resolveSelectedCableExportColumns(optionalColumnKeys)
  const tableHeaders = selectedColumns.map(column => column.label)
  const rows = resolveRuntimeAllocationRows(cableRows).map(row => (
    selectedColumns.map(column => column.getValue(row, terminalHeader))
  ))
  const summaryRows = [
    ["report", "cable-schedule"],
    ["workspace_name", workspaceName],
    ["exported_at", generatedAt.toISOString()],
    ["total", String(cableRows.length)],
  ]

  const csvContent = [
    ...summaryRows.map(row => row.map(csvEscape).join(",")),
    "",
    tableHeaders.map(csvEscape).join(","),
    ...rows.map(row => row.map(csvEscape).join(",")),
  ].join("\n")

  const dateSuffix = generatedAt.toISOString().slice(0, 19).replace(/:/g, "-")
  const filename = `cable-schedule-ws-${workspaceFilePart}-${dateSuffix}.csv`
  downloadTextFile(csvContent, filename)
  toastStore.success(`Cable schedule exported: ${rows.length} rows`)
}

function openExportModal() {
  if (workspaceMissing.value) return
  if (!allocatedCableRows.value.length) {
    toastStore.info("No allocated rows to export.")
    return
  }
  exportModalOpen.value = true
}

function closeExportModal() {
  exportModalOpen.value = false
}

function handleExportCableFromWizard(payload: { optionalColumnKeys: string[] }) {
  exportModalOpen.value = false
  exportCableJournal(payload.optionalColumnKeys)
}

function exportSignalReport() {
  const reportRows = resolveRuntimeAllocationRows()
  if (!reportRows.length) {
    toastStore.info("No signals to export.")
    return
  }

  const headers = sourceHeaders.value
  const fallbackHeaders = headers.length > 0 ? headers : ["signal_name", "signal_key"]
  const csvHeaders = [...fallbackHeaders, "signal_direction", "unit_id", "channel_index", "last_tested_at"]
  const rows = buildSignalReportRows(reportRows)
  const workspaceName = String(workspaceStore.activeWorkspace?.name ?? "")
  const workspaceFilePart = toFilenamePart(workspaceName)
  const generatedAt = new Date()
  const tested = reportRows.filter(row => Boolean(String(row.tested_at ?? "").trim())).length
  const remaining = Math.max(0, reportRows.length - tested)
  const total = reportRows.length

  const metaRows = [
    ["report", "signal-test-report"],
    ["generated_at", generatedAt.toISOString()],
    ["signals_total", String(total)],
    ["signals_tested", String(tested)],
    ["signals_remaining", String(remaining)],
  ]

  const csvContent = [
    ...metaRows.map(row => row.map(csvEscape).join(",")),
    "",
    csvHeaders.map(csvEscape).join(","),
    ...rows.map(row => row.map(csvEscape).join(",")),
  ].join("\n")

  const dateSuffix = generatedAt.toISOString().slice(0, 19).replace(/:/g, "-")
  const filename = `signal-report-ws-${workspaceFilePart}-${dateSuffix}.csv`
  downloadTextFile(csvContent, filename)
  toastStore.success(`Report exported: tested ${tested}, remaining ${remaining}, total ${total}`)
}

function normalizedChannelType(raw: string | null | undefined): "di" | "do" | "ai" | "ao" | null {
  const value = String(raw || "").trim().toLowerCase()
  if (value.startsWith("di")) return "di"
  if (value.startsWith("do")) return "do"
  if (value.startsWith("ai")) return "ai"
  if (value.startsWith("ao")) return "ao"
  return null
}

function resolveAllocationOnlineState(row: SignalAllocationRow): boolean | null {
  const channelId = Number(row.channel_id)
  const linkedChannel = Number.isFinite(channelId) && channelId > 0
    ? channelMap.value.get(channelId)
    : null

  const linkedDeviceId = Number(linkedChannel?.device_id)
  const rowDeviceId = Number(row.device_id)
  const deviceId = Number.isFinite(linkedDeviceId)
    ? linkedDeviceId
    : (Number.isFinite(rowDeviceId) ? rowDeviceId : NaN)

  if (Number.isFinite(deviceId) && deviceId > 0) {
    return deviceStatusById.value.get(deviceId) === "online"
  }

  return typeof row.unit_online === "boolean" ? row.unit_online : null
}

function signalGridRuntimeOverlay() {
  return {
    workspaceId: workspaceStore.activeWorkspaceId,
    getTestedAt: getSignalRuntimeTestedAt,
  }
}

function asAllocationRow(row: GridRow): SignalAllocationRow {
  return row as unknown as SignalAllocationRow
}

function resolveSignalIdFromGridRow(row: SignalAllocationRow): number | null {
  const signalId = Number((row as { signal_id?: unknown }).signal_id)
  if (Number.isFinite(signalId)) {
    return signalId
  }
  const rowId = String((row as { rowId?: unknown }).rowId ?? "")
  const match = rowId.match(/^signal-(\d+)$/)
  if (!match) {
    return null
  }
  const parsed = Number(match[1])
  return Number.isFinite(parsed) ? parsed : null
}

function resolveLiveAllocationRowBySignalId(signalId: number | null, fallback: SignalAllocationRow): SignalAllocationRow {
  if (!Number.isFinite(signalId as number)) {
    return fallback
  }
  return getSignalAllocationProjectionRowBySignalId(Number(signalId)) ?? fallback
}

function resolveLiveAllocationCellRow(row: SignalAllocationRow): SignalAllocationRow {
  const signalId = resolveSignalIdFromGridRow(row)
  return resolveLiveAllocationRowBySignalId(signalId, row)
}

type AllocationChannelPickerCandidate = {
  id: number
  unitId: string
  unitLabel: string
  channelIndex: number
  channelLabel: string
  online: boolean
  occupied: boolean
  ownerSignalId: number | null
  ownerRowText: string | null
  searchText: string
}

const allocationChannelPickerRow = computed(() => {
  const signalId = allocationChannelPickerSignalId.value
  if (!Number.isFinite(signalId as number)) {
    return null
  }
  return getSignalAllocationProjectionRowBySignalId(Number(signalId))
})

const allocationChannelPickerCurrentChannelId = computed<number | null>(() => {
  const channelId = Number(allocationChannelPickerRow.value?.channel_id)
  return Number.isFinite(channelId) && channelId > 0 ? channelId : null
})

const allocationChannelPickerCurrentLabel = computed(() => {
  const row = allocationChannelPickerRow.value
  return row ? resolveSignalAllocationDisplayLabel(row) : "-"
})

const allocationChannelPickerRequiredType = computed(() => {
  const direction = String(allocationChannelPickerRow.value?.signal_direction ?? "").trim()
  return direction ? resolveRuntimeChannelTypeForSignal(direction) : null
})

function formatSignalListOwnerValue(value: unknown): string {
  if (value === undefined || value === null) {
    return "-"
  }
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch {
      return String(value)
    }
  }
  const text = String(value).trim()
  return text || "-"
}

function buildSignalListOwnerRowText(ownerRow: SignalAllocationRow | null): string | null {
  if (!ownerRow) {
    return null
  }

  const sourceRow = extractSourceRowFromSignalMetadata(ownerRow.signal_metadata)
  const headers = sourceHeaders.value
  if (headers.length > 0) {
    return headers
      .map(header => `${header}: ${formatSignalListOwnerValue(sourceRow[header])}`)
      .join("\n")
  }

  return [
    `signal_name: ${formatSignalListOwnerValue(ownerRow.signal_name)}`,
    `signal_key: ${formatSignalListOwnerValue(ownerRow.signal_key)}`,
    `signal_direction: ${formatSignalListOwnerValue(ownerRow.signal_direction)}`,
  ].join("\n")
}

const allocationChannelPickerChannels = computed<AllocationChannelPickerCandidate[]>(() => {
  const row = allocationChannelPickerRow.value
  const requiredType = allocationChannelPickerRequiredType.value
  if (!row || !requiredType) {
    return []
  }

  const currentChannelId = allocationChannelPickerCurrentChannelId.value

  return channels.value
    .filter((channel) => {
      const channelType = String(channel.type).trim().toLowerCase()
      return channelType === requiredType || channel.id === currentChannelId
    })
    .map((channel) => {
      const unitId = channelUnitById.value.get(channel.id) ?? channelStore.resolveUnitId(channel.device_id)
      const resolvedName = String(channel.resolved_name ?? channel.name ?? "").trim()
      const unitName = deviceNameById.value.get(channel.device_id) ?? ""
      const unitLabel = unitName ? `${unitId} ${unitName}` : unitId
      const channelCode = `CH${channel.index + 1}`
      const channelLabel = resolvedName || channelCode
      const online = deviceStatusById.value.get(channel.device_id) === "online"
      const ownerSignalId = getSignalAllocationOwnerSignalIdByChannelId(channel.id)
      const ownerRow = ownerSignalId !== null ? getSignalAllocationProjectionRowBySignalId(ownerSignalId) : null
      const occupied = ownerSignalId !== null && ownerSignalId !== row.signal_id
      const ownerRowText = occupied ? buildSignalListOwnerRowText(ownerRow) : null

      return {
        id: channel.id,
        unitId,
        unitLabel,
        channelIndex: channel.index,
        channelLabel,
        online,
        occupied,
        ownerSignalId,
        ownerRowText,
        searchText: [unitId, unitName, unitLabel, channelCode, String(channel.index + 1), resolvedName, channel.name, channel.resolved_name, String(channel.device_id), ownerRowText]
          .filter((value): value is string => Boolean(value))
          .join(" ")
          .toLowerCase(),
      }
    })
    .sort((left, right) => {
      if (left.id === currentChannelId) return -1
      if (right.id === currentChannelId) return 1
      if (left.occupied !== right.occupied) return left.occupied ? 1 : -1
      if (left.online !== right.online) return left.online ? -1 : 1
      return `${left.unitId}/${left.channelIndex}`.localeCompare(`${right.unitId}/${right.channelIndex}`, undefined, { numeric: true, sensitivity: "base" })
    })
})

async function openAllocationChannelPicker(row: SignalAllocationRow) {
  const signalId = Number(row.signal_id)
  if (!Number.isFinite(signalId) || signalId <= 0) {
    return
  }

  allocationChannelPickerInstanceKey.value += 1
  allocationChannelPickerSignalId.value = signalId
  allocationChannelPickerOpen.value = true
  allocationChannelPickerError.value = null
  allocationChannelPickerLoading.value = true

  try {
    await ensureRuntimeCatalogLoaded()
  } catch (pickerError) {
    allocationChannelPickerError.value = pickerError instanceof Error ? pickerError.message : String(pickerError)
  } finally {
    allocationChannelPickerLoading.value = false
  }
}

function closeAllocationChannelPicker() {
  if (allocationChannelPickerSaving.value) {
    return
  }
  allocationChannelPickerOpen.value = false
  allocationChannelPickerSignalId.value = null
  allocationChannelPickerLoading.value = false
  allocationChannelPickerError.value = null
  allocationChannelPickerInstanceKey.value += 1
}

function resolveSelectedSignalIdsForDelete(): number[] {
  const signalIds = selectedAllocationRows.value
    .map(row => Number(row.signal_id))
    .filter((signalId): signalId is number => Number.isFinite(signalId) && signalId > 0)

  return Array.from(new Set(signalIds))
}

function openDeleteSelectedConfirm() {
  if (deletingSelected.value || deleteSelectedConfirmOpen.value) {
    return
  }

  const signalIds = resolveSelectedSignalIdsForDelete()
  if (!signalIds.length) {
    return
  }

  deleteSelectedSignalIds.value = signalIds
  deleteSelectedConfirmOpen.value = true
}

function closeDeleteSelectedConfirm() {
  if (deletingSelected.value) {
    return
  }

  deleteSelectedConfirmOpen.value = false
  deleteSelectedSignalIds.value = []
}

function confirmDeleteSelected() {
  const signalIds = [...deleteSelectedSignalIds.value]
  if (!signalIds.length || deletingSelected.value) {
    return
  }

  deleteSelectedConfirmOpen.value = false
  deleteSelectedSignalIds.value = []
  void handleDeleteSelected(signalIds)
}

async function handleAllocationChannelPicked(channelId: number | null) {
  const row = allocationChannelPickerRow.value
  if (!row) {
    return
  }

  const signalId = Number(row.signal_id)
  if (!Number.isFinite(signalId) || signalId <= 0) {
    return
  }

  allocationChannelPickerSaving.value = true
  allocationChannelPickerError.value = null
  let shouldClosePicker = false

  try {
    const currentChannelId = allocationChannelPickerCurrentChannelId.value
    const ownerSignalId = channelId !== null ? getSignalAllocationOwnerSignalIdByChannelId(channelId) : null
    if (channelId === null) {
      await signalSheetStore.unassignAllocation(signalId)
    } else if (currentChannelId === null) {
      if (ownerSignalId !== null && ownerSignalId !== signalId) {
        throw new Error("Channel is occupied. Assign this signal to a free channel first before swapping.")
      }
      await signalSheetStore.assignAllocation(signalId, channelId)
    } else if (ownerSignalId !== null && ownerSignalId !== signalId) {
      await signalSheetStore.swapAllocations(signalId, channelId)
    } else if (currentChannelId !== channelId) {
      await signalSheetStore.reassignAllocation(signalId, channelId)
    }

    if (channelId === null) {
      toastStore.success(`Cleared allocation for ${row.signal_name || row.signal_key}.`)
    } else {
      const nextChannel = allocationChannelPickerChannels.value.find(channel => channel.id === channelId)
      const nextLabel = nextChannel ? `${nextChannel.unitId}/${nextChannel.channelLabel}` : resolveSignalAllocationDisplayLabel(row)
      const verb = ownerSignalId !== null && ownerSignalId !== signalId ? "Swapped" : "Assigned"
      toastStore.success(`${verb} ${row.signal_name || row.signal_key} to ${nextLabel}.`)
    }
    shouldClosePicker = true
  } catch (pickerError) {
    const message = pickerError instanceof Error ? pickerError.message : String(pickerError)
    allocationChannelPickerError.value = message
    toastStore.error(message)
  } finally {
    allocationChannelPickerSaving.value = false
    if (shouldClosePicker) {
      closeAllocationChannelPicker()
    }
  }
}

async function handleDeleteSelected(signalIds: readonly number[]) {
  if (!signalIds.length) {
    return
  }

  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) {
    return
  }

  const savedViewBeforeDelete = captureSignalsGridSavedViewForStaticMutation()
  if (savedViewBeforeDelete) {
    persistSignalsGridSavedView(savedViewBeforeDelete)
  }

  deletingSelected.value = true
  deletingProgressDone.value = 0
  deletingProgressTotal.value = signalIds.length

  try {
    const chunkSize = 500
    let deletedTotal = 0
    let failedTotal = 0

    for (let index = 0; index < signalIds.length; index += chunkSize) {
      const chunk = signalIds.slice(index, index + chunkSize)
      try {
        await SignalsAPI.bulkDelete(workspaceId, chunk)
        deletedTotal += chunk.length
      } catch {
        failedTotal += chunk.length
      } finally {
        deletingProgressDone.value = Math.min(signalIds.length, index + chunk.length)
      }
      await nextTick()
    }

    clearGridSelection()
    await Promise.all([
      signalSheetStore.ensureSheetLoaded({ force: true }),
      signalSheetStore.ensureAllocationsLoaded({ force: true }),
    ])
    await restoreSignalsGridSavedViewAfterStaticMutation(savedViewBeforeDelete)

    if (failedTotal > 0) {
      toastStore.warning(`Deleted ${deletedTotal} signal(s), failed to delete ${failedTotal}.`)
    } else {
      toastStore.success(`Deleted ${deletedTotal} signal(s).`)
    }
  } catch (deleteError) {
    toastStore.error(deleteError instanceof Error ? deleteError.message : String(deleteError))
  } finally {
    deletingSelected.value = false
    deletingProgressDone.value = 0
    deletingProgressTotal.value = 0
  }
}

async function allocateSelectedUnassigned() {
  if (allocatingSelected.value) return
  if (!selectedUnassignedSignalIds.value.length) return
  const targetSignalIds = resolveSelectedUnassignedSignalIdsInSelectionOrder()
  if (!targetSignalIds.length) {
    toastStore.info("No free compatible channels available for selected rows.")
    return
  }
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) {
    return
  }

  allocatingSelected.value = true
  await awaitUiPaintFrame()
  try {
    const completedJob = await signalJobStore.enqueueAutoAllocateJob(workspaceId, {
      signal_ids: targetSignalIds,
      prefer_online: true,
      prefer_single_unit: false,
      overwrite_existing: false,
    })

    const changedRows = await applyCompletedAllocationJobPatch(completedJob)
    const requested = targetSignalIds.length
    const changed = changedRows.length
    const skipped = resolveSignalAllocationJobSkippedCount(completedJob, requested, changed)
    toastStore.success(`Allocation complete: ${requested} requested, ${changed} changed, ${skipped} skipped`)
  } catch (allocateError) {
    const message = allocateError instanceof Error ? allocateError.message : String(allocateError)
    toastStore.error(message)
  } finally {
    allocatingSelected.value = false
  }
}

async function deallocateSelected() {
  if (deallocatingSelected.value) return
  if (!selectedAllocatedSignalIds.value.length) return
  const targetSignalIds = [...selectedAllocatedSignalIds.value]
  const entries = targetSignalIds.map(signalId => ({ signal_id: signalId, channel_id: null }))
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) {
    return
  }

  deallocatingSelected.value = true
  await awaitUiPaintFrame()
  try {
    const completedJob = await signalJobStore.enqueueBulkUpdateJob(workspaceId, entries)
    const changedRows = await applyCompletedAllocationJobPatch(completedJob)
    const requested = targetSignalIds.length
    const changed = changedRows.length
    const skipped = resolveSignalAllocationJobSkippedCount(completedJob, requested, changed)
    toastStore.success(`Unassignment complete: ${requested} requested, ${changed} changed, ${skipped} skipped`)
  } catch (deallocateError) {
    const message = deallocateError instanceof Error ? deallocateError.message : String(deallocateError)
    toastStore.error(message)
  } finally {
    deallocatingSelected.value = false
  }
}

function applyCompletedTestRunPatch(job: SignalAllocationJob) {
  const testedIdsRaw = (job.result as Record<string, unknown> | undefined)?.tested_signal_ids
  const testedIds = Array.isArray(testedIdsRaw)
    ? testedIdsRaw.map((value) => Number(value)).filter((value) => Number.isFinite(value) && value > 0)
    : []
  if (!testedIds.length) {
    return
  }
  void signalSheetStore.markSignalsTested(testedIds, { optimistic: false }).catch(() => {
    return
  })
}

function setTestRunToggleMode(mode: "single" | "double") {
  testRunToggleMode.value = mode
}

function setTestRunIntervalMs(intervalMs: number) {
  const normalized = Math.max(100, Math.min(10000, Number(intervalMs)))
  testRunIntervalMs.value = normalized
}

async function controlActiveTestRun(action: "pause" | "resume" | "stop") {
  const workspaceId = workspaceStore.activeWorkspaceId
  const jobId = activeTestRunJob.value?.job_id
  if (!workspaceId || !jobId) {
    return
  }

  try {
    await signalJobStore.controlJob(workspaceId, jobId, action)
  } catch (controlError) {
    toastStore.error(controlError instanceof Error ? controlError.message : String(controlError))
  }
}

async function startTestRunJob(options?: { resumeFromCursor?: boolean; resumeJobId?: string }) {
  if (testRunInProgress.value || Boolean(activeTestRunJob.value)) return

  const queue = selectedVisibleAllocatedPhysicalRows.value.filter(row => canControl(row))
  if (!queue.length) {
    toastStore.info("Selected rows have no controllable channels.")
    return
  }

  testRunInProgress.value = true
  try {
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) {
      return
    }

    const selectedSignalIds = queue.map(row => row.signal_id)
    const completedJob = await signalJobStore.enqueueTestRunJob(
      workspaceId,
      selectedSignalIds,
      {
        signalIntervalMs: testRunIntervalMs.value,
        toggleMode: testRunToggleMode.value,
        resumeFromCursor: Boolean(options?.resumeFromCursor),
        resumeJobId: options?.resumeJobId,
      },
    )

    applyCompletedTestRunPatch(completedJob)
    toastStore.success("Run test complete.")
  } catch (runError) {
    const message = runError instanceof Error ? runError.message : String(runError)
    if (message.toLowerCase().includes("cancelled")) {
      toastStore.info("Run test cancelled")
    } else {
      toastStore.error(message)
    }
  } finally {
    testRunInProgress.value = false
  }
}

async function runTestVisualOnly() {
  if (canResumeActiveTestRun.value) {
    await controlActiveTestRun("resume")
    return
  }
  await startTestRunJob({ resumeFromCursor: false })
}

type ControlTargetBase = {
  channelId: number
  deviceId: number
  unitId: string
  channelIndex: number
  online: boolean
}

type DoControlTarget = ControlTargetBase & {
  kind: "do"
  channel: DoChannel | null
}

type AoControlTarget = ControlTargetBase & {
  kind: "ao"
  channel: AoChannel | null
}

type ControlTarget = DoControlTarget | AoControlTarget

function resolveControlTarget(row: SignalAllocationRow): ControlTarget | null {
  const channelId = Number(row.channel_id)
  if (!Number.isFinite(channelId) || channelId <= 0) {
    return null
  }

  const linkedChannel = channelMap.value.get(channelId)
  const linkedChannelType = normalizedChannelType(linkedChannel?.type)
  const rowChannelType = normalizedChannelType(row.channel_type)
  const effectiveChannelType = linkedChannelType ?? rowChannelType
  if (effectiveChannelType !== "do" && effectiveChannelType !== "ao") {
    return null
  }

  const linkedDeviceId = Number(linkedChannel?.device_id)
  const rowDeviceId = Number(row.device_id)
  const deviceId = Number.isFinite(linkedDeviceId)
    ? linkedDeviceId
    : (Number.isFinite(rowDeviceId) ? rowDeviceId : NaN)
  if (!Number.isFinite(deviceId) || deviceId <= 0) {
    return null
  }

  const linkedChannelIndex = Number(linkedChannel?.index)
  const rowChannelIndex = Number(row.channel_index)
  const channelIndex = Number.isFinite(linkedChannelIndex)
    ? linkedChannelIndex
    : (Number.isFinite(rowChannelIndex) ? rowChannelIndex : NaN)
  if (!Number.isFinite(channelIndex) || channelIndex < 0) {
    return null
  }

  const rowUnitId = String(row.unit_id ?? "").trim()
  const unitId = rowUnitId || (linkedChannel ? channelStore.resolveUnitId(linkedChannel.device_id) : "")
  if (!unitId) {
    return null
  }

  const online = deviceStatusById.value.get(deviceId) === "online"
  if (effectiveChannelType === "do") {
    const doChannel = linkedChannel && linkedChannelType === "do"
      ? (linkedChannel as DoChannel)
      : null

    return {
      kind: "do",
      channelId,
      deviceId,
      unitId,
      channelIndex,
      channel: doChannel,
      online,
    }
  }

  const aoChannel = linkedChannel && linkedChannelType === "ao"
    ? (linkedChannel as AoChannel)
    : null

  return {
    kind: "ao",
    channelId,
    deviceId,
    unitId,
    channelIndex,
    channel: aoChannel,
    online,
  }
}

function canControl(row: SignalAllocationRow) {
  return resolveControlTarget(row) !== null
}

function controlBusy(row: SignalAllocationRow): boolean {
  const target = resolveControlTarget(row)
  if (!target) return false
  if (target.kind === "ao") {
    return activeAoSubmittingSignalId.value === row.signal_id
  }
  if (!target.channel) return false
  const stage = target.channel.ui?.stage ?? "idle"
  return stage === "pending" || stage === "debounce"
}

function controlStateLabel(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "UNKNOWN"
  if (target.kind === "ao") {
    if (!target.channel) return "UNKNOWN"
    const valueLabel = `${formatAoValue(target.channel.state)} mA`
    if (!target.online) return valueLabel
    if (activeAoSubmittingSignalId.value === row.signal_id) return `${valueLabel} (PENDING)`
    return valueLabel
  }
  if (!target.channel) {
    return "UNKNOWN"
  }
  const stableLabel = target.channel.state ? "ON" : "OFF"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return stableLabel
  if (stage === "pending" || stage === "debounce") return `${stableLabel} (PENDING)`
  if (stage === "error") return `${stableLabel} (ERROR)`
  return stableLabel
}

function controlLampTone(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "allocation-control-cell__lamp--unknown"
  if (target.kind === "ao") {
    if (!target.online) return "allocation-control-cell__lamp--offline"
    if (activeAoSubmittingSignalId.value === row.signal_id) return "allocation-control-cell__lamp--pending"
    if (!target.channel) return "allocation-control-cell__lamp--unknown"
    if (target.channel.diagnostics?.hasError) return "allocation-control-cell__lamp--fault"
    const quality = target.channel.diagnostics?.quality
    if (quality === "fault") return "allocation-control-cell__lamp--fault"
    if (quality === "pending") return "allocation-control-cell__lamp--pending"
    return "allocation-control-cell__lamp--ao"
  }
  if (!target.channel) {
    return target.online ? "allocation-control-cell__lamp--unknown" : "allocation-control-cell__lamp--offline"
  }
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return "allocation-control-cell__lamp--offline"
  if (stage === "pending" || stage === "debounce") return "allocation-control-cell__lamp--pending"
  if (stage === "error") return "allocation-control-cell__lamp--error"
  return target.channel.state ? "allocation-control-cell__lamp--on" : "allocation-control-cell__lamp--off"
}

function controlStatusTag(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "N/A"
  if (target.kind === "ao") {
    if (!target.online) return ""
    if (activeAoSubmittingSignalId.value === row.signal_id) return "PEND"
    if (!target.channel) return "UNKN"
    if (target.channel.diagnostics?.hasError) return "ERR"
    const quality = target.channel.diagnostics?.quality
    if (quality === "pending") return "PEND"
    if (quality === "fault") return "FAULT"
    if (quality === "valid") return "OK"
    return ""
  }
  if (!target.channel) return target.online ? "UNKN" : ""
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return ""
  if (stage === "pending" || stage === "debounce") return "PEND"
  if (stage === "error") return "ERR"
  return target.channel.state ? "ON" : "OFF"
}

function controlStatusTone(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target || !target.online) return "allocation-control-cell__status--muted"
  if (target.kind === "ao") {
    if (!target.channel) return "allocation-control-cell__status--unknown"
    if (activeAoSubmittingSignalId.value === row.signal_id) return "allocation-control-cell__status--pending"
    if (target.channel.diagnostics?.hasError || target.channel.diagnostics?.quality === "fault") {
      return "allocation-control-cell__status--error"
    }
    if (target.channel.diagnostics?.quality === "pending") {
      return "allocation-control-cell__status--pending"
    }
    return "allocation-control-cell__status--ao"
  }
  if (!target.channel) return "allocation-control-cell__status--unknown"
  const stage = target.channel.ui?.stage ?? "idle"
  if (stage === "pending" || stage === "debounce") return "allocation-control-cell__status--pending"
  if (stage === "error") return "allocation-control-cell__status--error"
  return target.channel.state
    ? "allocation-control-cell__status--on"
    : "allocation-control-cell__status--off"
}

function controlStateIsOn(row: SignalAllocationRow): boolean {
  const target = resolveControlTarget(row)
  if (target?.kind !== "do") return false
  return Boolean(target?.channel?.state)
}

function aoValueLabel(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target || target.kind !== "ao") {
    return ""
  }
  if (!target.online || !target.channel) {
    return ""
  }
  return formatAoValue(target.channel.state)
}

function aoStatusTitle(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target || target.kind !== "ao") {
    return ""
  }
  if (!target.online) {
    return "Device is offline"
  }
  if (!target.channel) {
    return "AO runtime state is not available yet"
  }
  if (activeAoSubmittingSignalId.value === row.signal_id) {
    return "Waiting for AO confirmation from device"
  }
  if (target.channel.diagnostics?.hasError) {
    return "AO backend error is latched"
  }
  const quality = target.channel.diagnostics?.quality
  if (quality === "pending") return "AO diagnostics pending"
  if (quality === "fault") return "AO diagnostics fault"
  if (quality === "valid") return "AO diagnostics valid"
  return "AO runtime value"
}

function resolveCurrentAoChannel(target: AoControlTarget): AoChannel | null {
  const current = channelMap.value.get(target.channelId)
  return current && normalizedChannelType(current.type) === "ao"
    ? (current as AoChannel)
    : target.channel
}

function aoControlActive(row: SignalAllocationRow): boolean {
  return activeAoControlSignalId.value === row.signal_id
}

function beginAoControlEdit(row: SignalAllocationRow) {
  const target = resolveControlTarget(row)
  if (!target || target.kind !== "ao") return
  if (!target.online || controlBusy(row)) return
  activeAoControlSignalId.value = row.signal_id
  activeAoControlDraftValue.value = target.channel ? formatAoValue(target.channel.state) : "4.00"
}

function cancelAoControlEdit(signalId?: number | null) {
  if (signalId != null && activeAoControlSignalId.value !== signalId) {
    return
  }
  if (activeAoSubmittingSignalId.value != null && activeAoSubmittingSignalId.value === activeAoControlSignalId.value) {
    return
  }
  activeAoControlSignalId.value = null
  activeAoControlDraftValue.value = ""
}

function updateActiveAoControlDraftValue(value: string) {
  activeAoControlDraftValue.value = value
}

function waitForControlResult(target: ControlTarget, expectedState: boolean, timeoutMs = 2600): Promise<boolean> {
  if (target.kind !== "do") {
    return Promise.resolve(false)
  }
  const runtimeChannel = target.channel
  if (!runtimeChannel) {
    return Promise.resolve(false)
  }
  const startedAt = Date.now()
  return new Promise((resolve) => {
    const poll = () => {
      const stage = runtimeChannel.ui?.stage ?? "idle"
      if (stage === "error") {
        resolve(false)
        return
      }
      if (stage === "idle" && Boolean(runtimeChannel.state) === expectedState) {
        resolve(true)
        return
      }
      if (Date.now() - startedAt >= timeoutMs) {
        resolve(false)
        return
      }
      setTimeout(poll, 60)
    }
    poll()
  })
}

function waitForAoControlResult(target: AoControlTarget, expectedValue: number, timeoutMs = 2800): Promise<boolean> {
  const startedAt = Date.now()
  return new Promise((resolve) => {
    const poll = () => {
      const runtimeChannel = resolveCurrentAoChannel(target)
      if (runtimeChannel) {
        const currentValue = Number(runtimeChannel.state)
        const quality = runtimeChannel.diagnostics?.quality
        if (Math.abs(currentValue - expectedValue) <= 0.05 && quality !== "pending") {
          resolve(true)
          return
        }
      }
      if (Date.now() - startedAt >= timeoutMs) {
        resolve(false)
        return
      }
      setTimeout(poll, 80)
    }
    poll()
  })
}

async function sendControl(row: SignalAllocationRow, state: boolean): Promise<boolean> {
  const target = resolveControlTarget(row)
  if (!target) {
    toastStore.error("Channel not found")
    return false
  }
  if (!target.online) {
    toastStore.error("Device is offline")
    return false
  }
  if (target.kind !== "do") {
    return false
  }
  if (target.channel) {
    const stage = target.channel.ui?.stage ?? "idle"
    if (stage === "idle" && Boolean(target.channel.state) === state) {
      return false
    }
  }
  if (controlBusy(row) && target.channel && target.channel.ui?.target === state) {
    return false
  }

  try {
    channelStore.sendDoCommand(target.unitId, target.channelIndex, state)
    if (!target.channel) {
      void channelStore.ensureDeviceChannelsLoaded(target.deviceId)
        .then(() => {
          channelStore.requestStates(target.deviceId, { includeDiagnostics: false, silent: true })
        })
        .catch(() => {
          return
        })
      toastStore.info("Command sent. Runtime state will update after channel sync.")
      return true
    }

    const succeeded = await waitForControlResult(target, state)
    if (!succeeded) {
      toastStore.warning("Command not confirmed by device")
      return false
    }

    void signalSheetStore.markSignalsTested([row.signal_id], { optimistic: false }).catch(() => {
      return
    })
    return true
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
    return false
  }
}

async function sendAoControl(row: SignalAllocationRow): Promise<boolean> {
  const target = resolveControlTarget(row)
  if (!target || target.kind !== "ao") {
    toastStore.error("Analog output channel not found")
    return false
  }
  if (!target.online) {
    toastStore.error("Device is offline")
    return false
  }
  if (controlBusy(row)) {
    return false
  }

  const nextValue = parseAoInput(activeAoControlDraftValue.value)
  const currentValue = target.channel ? Number(target.channel.state) : NaN
  if (Number.isFinite(currentValue) && Math.abs(currentValue - nextValue) <= 0.05) {
    cancelAoControlEdit(row.signal_id)
    return false
  }

  activeAoControlSignalId.value = null
  activeAoControlDraftValue.value = ""
  activeAoSubmittingSignalId.value = row.signal_id
  try {
    channelStore.sendAoCommand(target.unitId, target.channelIndex, nextValue)

    const confirmed = await waitForAoControlResult(target, nextValue)
    if (!confirmed) {
      toastStore.warning("AO command not confirmed by device")
      return false
    }

    void signalSheetStore.markSignalsTested([row.signal_id], { optimistic: false }).catch(() => {
      return
    })
    return true
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
    return false
  } finally {
    activeAoSubmittingSignalId.value = null
  }
}

async function ensureRuntimeCatalogLoaded() {
  await Promise.all([
    deviceStore.ensureLoaded(),
    channelStore.ensureLoaded(),
  ])
}

function renderDefaultCell(context: DataGridAppCellRendererContext<GridRow>) {
  const displayValue = context.displayValue || "-"
  return h("span", { class: "signals-page__grid-cell" }, displayValue)
}

function renderTestedAtCell(context: DataGridAppCellRendererContext<GridRow>) {
  const raw = String(context.row?.tested_at ?? "").trim()
  if (!raw) {
    return h("span", { class: "signals-page__grid-cell" }, "-")
  }

  return h("span", { class: "signals-page__grid-cell" }, formatDate(raw))
}

const resolvedColumns = computed<DataGridAppColumnInput<GridRow>[]>(() => {
  const controlCellRenderVersion = `${activeAoControlSignalId.value ?? "idle"}:${activeAoSubmittingSignalId.value ?? "idle"}`
  const sourceColumns: DataGridAppColumnInput<GridRow>[] = sourceHeaders.value.map((header, index) => ({
    key: signalGridSourceColumnKey(index),
    label: header,
    minWidth: resolveSourceColumnMinWidth(header),
    initialState: { width: resolveSourceColumnInitialWidth(header) },
    presentation: { align: "left", headerAlign: "left" },
    capabilities: { editable: false },
    cellRenderer: renderDefaultCell,
  }))

  return [
    ...sourceColumns,
    {
      key: "internal_signal_type",
      label: "Internal Signal Type",
      minWidth: 96,
      initialState: { width: 140 },
      presentation: { align: "left", headerAlign: "left" },
      capabilities: { editable: false },
      cellRenderer: renderDefaultCell,
    },
    {
      key: "channel_select",
      label: "Unit/Channel",
      minWidth: 136,
      initialState: { width: 190, pin: "right" },
      presentation: { align: "left", headerAlign: "left" },
      capabilities: { editable: false, sortable: false },
      cellRenderer: ({ row }) => {
        const allocationRow = resolveLiveAllocationCellRow(asAllocationRow((row ?? {}) as GridRow))
        const assigned = Number.isFinite(allocationRow.channel_id as number)
        const signalLabel = allocationRow.signal_name || allocationRow.signal_key
        return h(AllocationChannelCell, {
          label: resolveSignalAllocationDisplayLabel(allocationRow),
          assigned,
          online: resolveAllocationOnlineState(allocationRow),
          active: allocationChannelPickerSignalId.value === allocationRow.signal_id,
          disabled: allocationChannelPickerSaving.value && allocationChannelPickerSignalId.value === allocationRow.signal_id,
          ariaLabel: assigned
            ? `Change hardware allocation for ${signalLabel}`
            : `Assign hardware for ${signalLabel}`,
          activate: () => {
            void openAllocationChannelPicker(allocationRow)
          },
        })
      },
    },
    {
      key: "tested_at",
      label: "Tested At",
      dataType: "datetime",
      minWidth: 128,
      initialState: { width: 176, pin: "right" },
      presentation: {
        align: "left",
        headerAlign: "left",
      },
      capabilities: { editable: false },
      cellRenderer: renderTestedAtCell,
    },
    {
      key: "control",
      label: "Control",
      minWidth: 136,
      initialState: { width: 188, pin: "right" },
      presentation: { align: "left", headerAlign: "left" },
      capabilities: { editable: false, sortable: false, filterable: false },
      cellInteraction: {
        click: true,
        keyboard: ["enter", "space"],
        role: "button",
        label: ({ row }) => {
          const controlRow = resolveLiveAllocationCellRow(asAllocationRow((row ?? {}) as GridRow))
          const target = resolveControlTarget(controlRow)
          const signalLabel = controlRow.signal_name || controlRow.signal_key

          if (target?.kind === "do") {
            return `Turn ${controlStateIsOn(controlRow) ? 'off' : 'on'} control for ${signalLabel}`
          }

          if (target?.kind === "ao") {
            return `Analog output control for ${signalLabel}`
          }

          return `Control is unavailable for ${signalLabel}`
        },
        disabled: ({ row }) => {
          const controlRow = resolveLiveAllocationCellRow(asAllocationRow((row ?? {}) as GridRow))
          return controlBusy(controlRow)
        },
        pressed: ({ row }) => {
          const controlRow = resolveLiveAllocationCellRow(asAllocationRow((row ?? {}) as GridRow))
          return resolveControlTarget(controlRow)?.kind === "do"
            ? controlStateIsOn(controlRow)
            : undefined
        },
        onInvoke: ({ row }) => {
          const controlRow = resolveLiveAllocationCellRow(asAllocationRow((row ?? {}) as GridRow))
          const target = resolveControlTarget(controlRow)

          if (target?.kind === "do") {
            void sendControl(controlRow, !controlStateIsOn(controlRow))
            return
          }

          if (target?.kind === "ao") {
            beginAoControlEdit(controlRow)
          }
        },
      },
      cellRenderer: ({ row, interactive }: DataGridAppCellRendererContext<GridRow>) => {
        const controlRow = resolveLiveAllocationCellRow(asAllocationRow((row ?? {}) as GridRow))
        const target = resolveControlTarget(controlRow)
        const isOn = controlStateIsOn(controlRow)

        return h(AllocationControlCell, {
          key: `${controlRow.signal_id}:${controlCellRenderVersion}`,
          mode: target?.kind ?? "none",
          lampTone: controlLampTone(controlRow),
          statusTone: controlStatusTone(controlRow),
          statusTag: controlStatusTag(controlRow),
          statusTitle: aoStatusTitle(controlRow),
          stateLabel: controlStateLabel(controlRow),
          disabled: interactive?.enabled !== true,
          isOn,
          activate: () => {
            invokeRenderedCellAction(interactive)
          },
          ariaLabel: interactive?.ariaLabel ?? (target?.kind === "ao"
            ? `Click to edit analog output for ${controlRow.signal_name || controlRow.signal_key}`
            : `Turn ${isOn ? 'off' : 'on'} control for ${controlRow.signal_name || controlRow.signal_key}`),
          ariaPressed: target?.kind === "do" ? (interactive?.ariaPressed ?? (isOn ? "true" : "false")) : undefined,
          aoActive: aoControlActive(controlRow),
          aoPending: activeAoSubmittingSignalId.value === controlRow.signal_id,
          aoValueLabel: aoValueLabel(controlRow),
          aoInputValue: aoControlActive(controlRow) ? activeAoControlDraftValue.value : aoValueLabel(controlRow),
          aoOpenHint: "Click to edit",
          beginAoEdit: () => {
            invokeRenderedCellAction(interactive)
          },
          cancelAoEdit: () => {
            cancelAoControlEdit(controlRow.signal_id)
          },
          commitAoEdit: () => {
            void sendAoControl(controlRow)
          },
          updateAoInput: updateActiveAoControlDraftValue,
        })
      },
    },
  ]
})

const virtualizationOptions = computed(() => ({
  rows: true,
  columns: true,
  rowOverscan: 10,
  columnOverscan: 2,
}))

const gridRows = signalGridRowModel.rows

function rebuildSignalGridRows() {
  signalGridRowModel.setRows(createSignalGridRows(signalAllocationProjectionRows(), sourceHeaders.value, signalGridRuntimeOverlay()))
}

async function refreshSignalsStatic(_reason: SignalStaticRefreshReason) {
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) {
    error.value = null
    return
  }

  error.value = null
  refreshingSignalsStatic.value = true

  try {
    await Promise.all([
      signalSheetStore.ensureSheetLoaded({ force: true }),
      signalSheetStore.ensureAllocationsLoaded({ force: true }),
      ensureRuntimeCatalogLoaded(),
    ])
  } catch (err) {
    error.value = err instanceof Error ? err.message : String(err)
  } finally {
    refreshingSignalsStatic.value = false
    tryApplyPendingSignalsGridSavedView()
  }
}

watch(
  () => workspaceStore.activeWorkspaceId,
  () => {
    const refreshPromise = refreshSignalsStatic("workspace_switch")
    restoreSignalsGridState()
    void refreshPromise
  },
)

watch(
  () => allocationRows.value,
  (rows) => {
    replaceSignalAllocationProjectionRows(rows)
    signalRuntimeStateCache.clear()
    patchSignalRuntimeStateFromStore(signalAllocationProjectionCache.getSignalIds())
    bumpSignalRuntimeStateVersion()
    rebuildSignalGridRows()
  },
  { flush: "post", immediate: true },
)

watch(
  sourceHeaders,
  () => {
    rebuildSignalGridRows()
  },
  { flush: "post" },
)

watch(
  [allocationGridRef, sourceHeaders, loading],
  () => {
    tryApplyPendingSignalsGridSavedView()
  },
  { flush: "post" },
)

watch(
  () => [allocationRevision.value, recentlyChangedSignalIds.value] as const,
  ([, signalIds]) => {
    applyStoreSignalGridAllocationPatches(signalIds)
  },
  { flush: "post" },
)

watch(
  () => [activeWorkspaceRevision.value, activeWorkspacePatchedSignalIds.value] as const,
  ([, signalIds]) => {
    patchSignalRuntimeStateFromStore(signalIds)
    signalGridPatchIngress.applyRuntimeSignals(signalIds, {
      reason: "signal-tested-at-realtime-patch",
      columns: ["tested_at"],
    })
  },
  { flush: "post" },
)

watch(
  () => [activeWorkspacePatchRevision.value, activeWorkspacePatchEvent.value] as const,
  ([, event]) => {
    if (!event) {
      return
    }
    void applySignalRowsPatchedEvent(event)
  },
  { flush: "post" },
)

watch(
  () => route.query.import,
  () => {
    syncImportModalFromRoute()
  },
  { immediate: true },
)

onMounted(() => {
  const refreshPromise = refreshSignalsStatic("initial_load")
  restoreSignalsGridState()
  void refreshPromise
  syncImportModalFromRoute()
})

onBeforeUnmount(() => {
  if (signalsGridStatePersistTimer !== null) {
    clearTimeout(signalsGridStatePersistTimer)
    signalsGridStatePersistTimer = null
  }
})
</script>

<style scoped>
.signals-page {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  height: 100%;
  min-height: 0;
  min-width: 0;
  padding: 0.75rem;
}

.signals-page__empty {
  align-items: center;
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  border: 1px dashed var(--color-neutral-300);
  border-radius: 1rem;
  color: var(--color-neutral-500);
  display: flex;
  flex: 1 1 auto;
  font-size: var(--text-sm);
  justify-content: center;
  padding: 2rem;
}

.signals-page__error {
  background: color-mix(in srgb, var(--color-rose-300) 14%, var(--color-white));
  border: 1px solid color-mix(in srgb, var(--color-rose-300) 70%, var(--color-white));
  border-radius: 1rem;
  color: var(--color-rose-700);
  font-size: var(--text-sm);
  padding: 0.75rem 1rem;
}

.signals-page__grid-section {
  flex: 1 1 auto;
  min-height: 0;
  min-width: 0;
  position: relative;
}

.signals-page__skeleton {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  box-shadow: var(--shadow-sm);
  display: flex;
  flex-direction: column;
  inset: 0;
  min-height: 0;
  overflow: hidden;
  pointer-events: none;
  position: absolute;
  z-index: 10;
}

.signals-page__skeleton-toolbar,
.signals-page__skeleton-head-row,
.signals-page__skeleton-row {
  align-items: center;
  border-bottom: 1px solid var(--color-neutral-200);
  gap: 0.75rem;
  padding-inline: 1rem;
}

.signals-page__skeleton-toolbar {
  display: flex;
  flex: 0 0 3rem;
  height: 3rem;
}

.signals-page__skeleton-head-row {
  display: grid;
  flex: 0 0 2.5rem;
  height: 2.5rem;
}

.signals-page__skeleton-body {
  flex: 1 1 auto;
  min-height: 0;
  overflow: hidden;
}

.signals-page__skeleton-row {
  border-bottom-color: var(--color-neutral-100);
  display: grid;
  height: 2.25rem;
}

.signals-page__skeleton-block {
  animation: signals-page-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
  background: var(--color-neutral-200);
  border-radius: var(--radius-sm);
  height: 0.75rem;
}

.signals-page__skeleton-block--toolbar-wide {
  height: 1rem;
  width: 7rem;
}

.signals-page__skeleton-block--toolbar-medium {
  height: 1rem;
  width: 5rem;
}

.signals-page__skeleton-block--toolbar-action {
  height: 1.75rem;
  margin-left: auto;
  width: 6rem;
}

.signals-page__skeleton-block--head,
.signals-page__skeleton-block--line {
  width: 100%;
}

.signals-page__skeleton-block--checkbox {
  width: 0.75rem;
}

.signals-page__skeleton-block--status {
  height: 1.25rem;
  width: 100%;
}

.signals-page__grid-cell {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
}

:global(.dark .signals-page__empty) {
  background: color-mix(in srgb, var(--color-neutral-900) 40%, transparent);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-400);
}

:global(.dark .signals-page__error) {
  background: color-mix(in srgb, var(--color-rose-700) 30%, var(--color-neutral-950));
  border-color: color-mix(in srgb, var(--color-rose-700) 60%, var(--color-neutral-950));
  color: var(--color-rose-300);
}

:global(.dark .signals-page__skeleton) {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-800);
}

:global(.dark .signals-page__skeleton-toolbar),
:global(.dark .signals-page__skeleton-head-row) {
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .signals-page__skeleton-row) {
  border-bottom-color: var(--color-neutral-900);
}

:global(.dark .signals-page__skeleton-block) {
  background: var(--color-neutral-800);
}

:global(.dark .signals-page__grid-cell) {
  color: var(--color-neutral-100);
}

@keyframes signals-page-pulse {
  50% {
    opacity: 0.5;
  }
}

@media (min-width: 768px) {
  .signals-page {
    padding: 1rem;
  }
}
</style>
