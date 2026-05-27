<template>
  <UiModal :open="open" title="Import Signal List" max-width="3xl" desktop-height="80vh" @close="emitClose">
    <form id="signal-import-form" class="signal-import-modal__form" @submit.prevent="handleSubmit">
      <!-- <UiAlert
        type="info"
        message="Upload an Excel signal list (.xls, .xlsx, .xlsm). Include headers and any metadata columns required by your workspace schema."
      /> -->

      <div
        class="signal-import-modal__steps-card"
      >
        <div class="signal-import-modal__steps-row">
          <template v-for="(item, index) in stepItems" :key="item.id">
            <div class="signal-import-modal__step">
              <span :class="stepIndicatorClass(item.id)">{{ index + 1 }}. {{ item.label }}</span>
              <span v-if="index < stepItems.length - 1" class="signal-import-modal__step-separator">→</span>
            </div>
          </template>
        </div>
      </div>

      <div v-if="error" ref="errorAnchorRef">
        <UiAlert type="error" :message="error" />
      </div>

      <div v-if="step === 'upload'" class="signal-import-modal__upload-field">
        <label for="signal-import-file" class="signal-import-modal__strong-label">Signal list file</label>
        <div
          class="signal-import-modal__dropzone"
          :class="{
            'signal-import-modal__dropzone--active': dropActive,
            'signal-import-modal__dropzone--idle': !dropActive,
            'signal-import-modal__dropzone--disabled': parsing || loading,
          }"
          tabindex="0"
          role="button"
          aria-label="Upload signal list file"
          :aria-disabled="parsing || loading"
          @click="triggerFileDialog"
          @keydown.enter.prevent="triggerFileDialog"
          @keydown.space.prevent="triggerFileDialog"
          @dragenter.prevent="onDragEnter"
          @dragover.prevent="onDragOver"
          @dragleave.prevent="onDragLeave"
          @drop.prevent="onDrop"
        >
          <div class="signal-import-modal__dropzone-content">
            <p class="signal-import-modal__dropzone-title">Drag & drop your spreadsheet</p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs">
              or <span class="signal-import-modal__browse-link">browse files</span>
            </p>
          </div>
          <p class="signal-import-modal__file-help">Supported: .xls, .xlsx, .xlsm</p>
          <p class="signal-import-modal__file-meta" v-if="fileName">
            Selected: <span class="signal-import-modal__file-name">{{ fileName }}</span>
          </p>
          <p v-if="parsing" class="signal-import-modal__parsing-text">Analyzing workbook…</p>
        </div>
        <input
          ref="fileInput"
          type="file"
          autocomplete="off"
          id="signal-import-file"
          name="signal-import-file"
          accept=".xls,.xlsx,.xlsm"
          class="signal-import-modal__file-input"
          :disabled="parsing || loading"
          @change="onFileChange"
        />

        <div class="signal-import-modal__preset-block">
          <p class="signal-import-modal__eyebrow">
            Preset (optional)
          </p>
          <UiAffinoListbox
            v-model="selectedPresetId"
            :options="presetListboxOptions"
            placeholder="Manual wizard"
            aria-label="Preset"
            :disabled="loading || parsing"
          />
          <p class="signal-import-modal__muted signal-import-modal__muted--xs">
            Choose a saved preset to prefill sheet/column/type mapping in the wizard.
          </p>
          <div class="signal-import-modal__inline-actions">
            <UiButton
              v-if="selectedPreset"
              type="button"
              variant="ghost"
              size="xs"
              :disabled="loading || parsing"
              @click="requestDeleteSelectedPreset"
            >
              Delete selected preset
            </UiButton>
          </div>
        </div>
      </div>

      <template v-else>
        <div class="signal-import-modal__ready-card">
          <div class="signal-import-modal__ready-row">
            <div>
              <p class="signal-import-modal__ready-title">File ready</p>
              <p class="signal-import-modal__muted signal-import-modal__muted--xs">{{ fileName }}</p>
            </div>
            <UiButton
              type="button"
              variant="secondary"
              size="sm"
              class="signal-import-modal__nowrap"
              @click="replaceFile"
              :disabled="loading || parsing"
            >
              Choose different file
            </UiButton>
          </div>
          <UiAlert
            v-if="signalSheetStore.hasSheet"
            type="warning"
            class="signal-import-modal__card-alert"
            message="Re-import overwrites existing signal rows in this workspace. If you need to preserve existing test results, create a new workspace before importing."
          />
        </div>

        <div v-if="step === 'columns'" class="signal-import-modal__section-stack">
          <div class="signal-import-modal__field">
            <p class="signal-import-modal__strong-label">Worksheet</p>
            <UiAffinoListbox
              v-model="selectedSheetName"
              :options="worksheetListboxOptions"
              placeholder="Select worksheet"
              aria-label="Worksheet"
              :disabled="loading || parsing || worksheetListboxOptions.length === 0"
            />
            <p v-if="!selectedSheetName" class="signal-import-modal__warning-text">
              Choose a worksheet from the uploaded file to continue. The Next button stays disabled until selected.
            </p>
          </div>

          <div v-if="availableColumns.length">
            <div class="signal-import-modal__section-heading">
              <div>
                <p class="signal-import-modal__section-title">Columns</p>
                <p class="signal-import-modal__muted signal-import-modal__muted--xs">
                  {{ selectedColumnCount }} of {{ availableColumns.length }} selected
                </p>
              </div>
              <div class="signal-import-modal__button-pair">
                <UiButton type="button" variant="ghost" size="xs" @click="selectAllColumns" :disabled="loading">
                  Select all
                </UiButton>
                <UiButton type="button" variant="ghost" size="xs" @click="clearAllColumns" :disabled="loading">
                  Clear
                </UiButton>
              </div>
            </div>
            <div class="signal-import-modal__option-list">
              <label
                v-for="column in availableColumns"
                :key="column.index"
                class="signal-import-modal__option"
              >
                <input
                  type="checkbox"
                  autocomplete="off"
                  :id="`signal-import-column-${column.index}`"
                  :name="`signal-import-columns-${column.index}`"
                  class="signal-import-modal__checkbox"
                  :checked="isColumnSelected(column.index)"
                  @change="toggleColumn(column.index)"
                />
                <span class="signal-import-modal__option-label">{{ column.header }}</span>
              </label>
            </div>
          </div>
          <p v-else class="signal-import-modal__muted">
            Selected worksheet has no detectable header row. Choose another sheet or upload a different file.
          </p>
        </div>

        <div v-else-if="step === 'terminal'" class="signal-import-modal__section-stack">
          <div>
            <p class="signal-import-modal__section-title">Terminal column</p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs">
              Select column that contains terminal block values.
            </p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs signal-import-modal__spaced-xs">
              Terminal block of cabinet will be used for physical device connection to cabinet.
            </p>
          </div>

          <div class="signal-import-modal__field">
            <p class="signal-import-modal__label">Terminal column</p>
            <UiAffinoListbox
              v-model="terminalColumnIndex"
              id="signal-import-terminal-column"
              name="signal-import-terminal-column"
              :options="terminalColumnListboxOptions"
              placeholder="Select terminal column"
              aria-label="Terminal column"
              :disabled="loading || parsing || terminalColumnListboxOptions.length === 0"
            />
            <p v-if="terminalColumnIndex === null" class="signal-import-modal__warning-text">
              Select terminal column to continue.
            </p>
          </div>
        </div>

        <div v-else-if="step === 'types'" class="signal-import-modal__section-stack">
          <div>
            <p class="signal-import-modal__section-title">Type mapping</p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs">
              Select the column that contains vendor type codes, then map each code to an internal signal type.
            </p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs signal-import-modal__spaced-xs">
              Example: <span class="signal-import-modal__strong">SPS → DI</span>, <span class="signal-import-modal__strong">SPC → DO</span>. Unmapped codes are skipped.
            </p>
          </div>
          <div class="signal-import-modal__field">
            <p class="signal-import-modal__label">Type column</p>
            <UiAffinoListbox
              v-model="typeColumnIndex"
              :options="typeColumnListboxOptions"
              placeholder="Select type column"
              aria-label="Type column"
              :disabled="loading || parsing || typeColumnListboxOptions.length === 0"
            />
            <p v-if="typeColumnIndex === null" class="signal-import-modal__warning-text">
              Select the column that contains vendor type codes to continue import.
            </p>
          </div>
          <div v-if="typeValueOptions.length" class="signal-import-modal__mapping-list">
            <div
              v-for="option in typeValueOptions"
              :key="option.key"
              class="signal-import-modal__mapping-row"
            >
              <div>
                <p class="signal-import-modal__mapping-title">{{ option.label }}</p>
                <p class="signal-import-modal__muted signal-import-modal__muted--xs">{{ option.count }} rows</p>
              </div>
              <div class="signal-import-modal__mapping-control">
                <UiAffinoListbox
                  :model-value="typeMapping[option.key] ?? ''"
                  :options="typeMappingListboxOptions"
                  aria-label="Internal type mapping"
                  :disabled="loading || parsing"
                  @update:model-value="value => onTypeMappingChange(option.key, value)"
                />
              </div>
            </div>
          </div>
          <p v-else class="signal-import-modal__muted">
            Selected column has no recognizable values. Choose a different column.
          </p>
          <UiAlert
            type="warning"
            message="Rows with types left as 'Skip' will not be imported."
          />
          <div class="signal-import-modal__field">
            <label class="signal-import-modal__label">Save as preset (optional)</label>
            <input
              v-model="savePresetName"
              type="text"
              autocomplete="off"
              id="signal-import-save-preset-name"
              name="signal-import-save-preset-name"
              maxlength="120"
              class="signal-import-modal__input"
              placeholder="e.g. Project SCADA import"
            />
          </div>
        </div>
      </template>

    </form>

    <template #footer>
      <div class="signal-import-modal__footer">
        <UiButton type="button" variant="secondary" @click="emitClose" :disabled="loading || parsing">
          Cancel
        </UiButton>
        <UiButton
          v-if="canGoBack"
          type="button"
          variant="ghost"
          @click="goToPreviousStep"
          :disabled="loading || parsing"
        >
          Back
        </UiButton>
        <UiButton
          v-if="!isFinalStep"
          type="button"
          variant="primary"
          :disabled="!canAdvance || loading"
          @click="goToNextStep"
        >
          Next
        </UiButton>
        <UiButton
          v-else
          type="submit"
          form="signal-import-form"
          variant="primary"
          :disabled="!canSubmitFinal || loading"
        >
          <template v-if="loading">
            📥 Importing…
          </template>
          <template v-else>
            📥 Import
          </template>
        </UiButton>
      </div>
    </template>
  </UiModal>
  <ConfirmModal
    :open="deletePresetOpen"
    title="Delete preset"
    :message="deletePresetMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="deletePresetOpen = false"
    @confirm="confirmDeleteSelectedPreset"
  />
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"

import { getHttpErrorContext } from "@/api/httpErrors"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiAlert from "@/components/ui/UiAlert.vue"
import UiAffinoListbox from "@/components/ui/UiAffinoListbox.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"
import type { InternalSignalType, SignalImportMeta, SignalSheetPreset } from "@/types/signal"

const props = defineProps<{ open: boolean }>()

const emit = defineEmits<{ (e: "close"): void; (e: "imported", sheetId: number): void }>()

const signalSheetStore = useSignalSheetStore()
const toastStore = useToastStore()
const ALLOWED_EXTENSIONS = ["xls", "xlsx", "xlsm"]

const STEP_ITEMS = [
  { id: "upload", label: "Upload file" },
  { id: "columns", label: "Columns" },
  { id: "terminal", label: "Terminal" },
  { id: "types", label: "Type mapping" },
] as const
type WizardStep = (typeof STEP_ITEMS)[number]["id"]
const stepOrder: WizardStep[] = STEP_ITEMS.map(item => item.id)
const stepItems = STEP_ITEMS

const step = ref<WizardStep>("upload")
const file = ref<File | null>(null)
const fileName = ref("")
const loading = ref(false)
const error = ref<string | null>(null)
const errorAnchorRef = ref<HTMLElement | null>(null)
const fileInput = ref<HTMLInputElement | null>(null)
const parsing = ref(false)
const dropActive = ref(false)
const dragCounter = ref(0)

type SheetColumn = {
  header: string
  index: number
}

const sheetColumns = ref<Record<string, SheetColumn[]>>({})
const sheetRows = ref<Record<string, unknown[][]>>({})
const selectedSheetName = ref<string | null>(null)
const selectedColumnsBySheet = ref<Record<string, number[]>>({})
const terminalColumnIndex = ref<number | null>(null)
const typeColumnIndex = ref<number | null>(null)
const typeMapping = ref<Record<string, InternalSignalType>>({})
const selectedPresetId = ref<number | null>(null)
const savePresetName = ref("")
const applyingPreset = ref(false)
const suppressTypeMappingReset = ref(false)
const deletePresetOpen = ref(false)

const internalTypeOptions: Array<{ value: InternalSignalType; label: string }> = [
  { value: "di", label: "Digital input (DI)" },
  { value: "do", label: "Digital output (DO)" },
  { value: "ao", label: "Analog output (AO)" },
  { value: "ai", label: "Analog input (AI)" },
]
const typeMappingListboxOptions: Array<{ value: InternalSignalType | ""; label: string }> = [
  { value: "", label: "Skip" },
  ...internalTypeOptions,
]

const sheetNames = computed(() => Object.keys(sheetColumns.value))
const availableColumns = computed(() => (selectedSheetName.value ? sheetColumns.value[selectedSheetName.value] ?? [] : []))
const selectedColumnIndexes = computed(() =>
  selectedSheetName.value ? selectedColumnsBySheet.value[selectedSheetName.value] ?? [] : []
)
const selectedColumnSet = computed(() => new Set(selectedColumnIndexes.value))
const selectedColumnCount = computed(() => selectedColumnIndexes.value.length)
const selectedColumnOptions = computed(() =>
  selectedSheetName.value
    ? (sheetColumns.value[selectedSheetName.value] ?? []).filter(column => selectedColumnSet.value.has(column.index))
    : []
)
const terminalColumnListboxOptions = computed(() =>
  selectedColumnOptions.value.map(column => ({ value: column.index, label: column.header }))
)
const typeValueOptions = computed(() => buildTypeValueOptions())
const hasTypeMappings = computed(() => Object.keys(typeMapping.value).length > 0)
const activeStepIndex = computed(() => stepOrder.indexOf(step.value))
const isFinalStep = computed(() => step.value === "types")
const canGoBack = computed(() => step.value === "terminal" || step.value === "types")
const columnsStepValid = computed(
  () => !!selectedSheetName.value && availableColumns.value.length > 0 && selectedColumnCount.value > 0,
)
const terminalStepValid = computed(
  () => columnsStepValid.value && terminalColumnIndex.value !== null,
)
const typeStepValid = computed(
  () => terminalStepValid.value && typeColumnIndex.value !== null && typeValueOptions.value.length > 0 && hasTypeMappings.value,
)
const canAdvance = computed(() => {
  if (step.value === "columns") return columnsStepValid.value
  if (step.value === "terminal") return terminalStepValid.value
  return false
})
const canSubmitFinal = computed(() => step.value === "types" && typeStepValid.value)
const presets = computed(() => signalSheetStore.presets)
const presetListboxOptions = computed(() => [
  { value: null, label: "Manual wizard" },
  ...presets.value.map(preset => ({ value: preset.id, label: preset.name })),
])
const worksheetListboxOptions = computed(() =>
  sheetNames.value.map(sheet => ({ value: sheet, label: sheet })),
)
const typeColumnListboxOptions = computed(() =>
  selectedColumnOptions.value.map(column => ({ value: column.index, label: column.header })),
)
const selectedPreset = computed<SignalSheetPreset | null>(() =>
  presets.value.find(item => item.id === selectedPresetId.value) ?? null,
)
const deletePresetMessage = computed(() => {
  const preset = selectedPreset.value
  if (!preset) return ""
  return `Preset "${preset.name}" will be deleted.`
})

function stepIndicatorClass(target: WizardStep) {
  const targetIndex = stepOrder.indexOf(target)
  const currentIndex = activeStepIndex.value
  if (targetIndex === currentIndex) return "signal-import-modal__step-label signal-import-modal__step-label--active"
  if (targetIndex < currentIndex) return "signal-import-modal__step-label signal-import-modal__step-label--complete"
  return "signal-import-modal__step-label signal-import-modal__step-label--upcoming"
}

function goToNextStep() {
  if (loading.value || parsing.value) return
  if (!canAdvance.value) return
  const currentIndex = activeStepIndex.value
  if (currentIndex === -1 || currentIndex >= stepOrder.length - 1) return
  step.value = stepOrder[currentIndex + 1]
}

function goToPreviousStep() {
  if (loading.value || parsing.value) return
  const currentIndex = activeStepIndex.value
  if (currentIndex <= 0) return
  step.value = stepOrder[currentIndex - 1]
}

function emitClose() {
  if (loading.value || parsing.value) return
  deletePresetOpen.value = false
  resetWorkflowState()
  emit("close")
}

async function onFileChange(e: Event) {
  const target = e.target as HTMLInputElement
  const selected = target.files?.[0] ?? null
  await handleSelectedFile(selected)
}

function resetWorkflowState(options: { preserveError?: boolean } = {}) {
  clearWorkbookState()
  file.value = null
  fileName.value = ""
  terminalColumnIndex.value = null
  typeColumnIndex.value = null
  typeMapping.value = {}
  savePresetName.value = ""
  if (!options.preserveError) {
    error.value = null
  }
  if (fileInput.value) {
    fileInput.value.value = ""
  }
  dropActive.value = false
  dragCounter.value = 0
}

function clearWorkbookState() {
  sheetColumns.value = {}
  sheetRows.value = {}
  selectedSheetName.value = null
  selectedColumnsBySheet.value = {}
  terminalColumnIndex.value = null
  step.value = "upload"
}

async function handleSubmit() {
  if (!canSubmitFinal.value || loading.value || parsing.value || step.value !== "types") return
  loading.value = true
  error.value = null
  try {
    const payload = await buildPreparedImportPayload()
    const sheet = await signalSheetStore.importSheet(payload.file, {
      metadata: payload.metadata,
      presetId: selectedPresetId.value,
      savePresetName: savePresetName.value.trim() || null,
    })
    emit("imported", sheet.id)
    resetWorkflowState()
    emit("close")
  } catch (err) {
    error.value = toReadableImportError(err)
    await nextTick()
    errorAnchorRef.value?.scrollIntoView({ behavior: "smooth", block: "nearest" })
  } finally {
    loading.value = false
  }
}

function toReadableImportError(err: unknown): string {
  const context = getHttpErrorContext(err)
  if (context.isHttpError) {
    const code = context.code.toUpperCase()
    const status = Number(context.status)
    const detail = context.detail
    const normalized = detail.replace(/^Unable to parse workbook:\s*/i, "").trim()

    if (code === "ECONNABORTED") {
      return "Import timed out on the first attempt. Please retry; if it repeats, reduce file size or check device load."
    }
    if (context.status === null && (code === "ERR_NETWORK" || code === "ECONNRESET" || code === "ETIMEDOUT")) {
      return "Network connection was interrupted during import. Please retry."
    }

    if (status === 413) {
      return "The uploaded file is too large for the server limit. Reduce file size and try again."
    }
    if (status === 415) {
      return "Unsupported file type. Upload .xls, .xlsx, or .xlsm."
    }
    if (status === 502 || status === 503 || status === 504) {
      return "Import service is temporarily unavailable. Please try again in a moment."
    }

    const limit = detail.match(/Import limit exceeded:\s*(\d+)\s*rows\s*\(max\s*(\d+)\)/i)
    if (limit) {
      return `The file is too large: ${limit[1]} rows. The maximum allowed is ${limit[2]} rows.`
    }

    if (/Uploaded file is empty/i.test(detail)) {
      return "The file is empty. Upload a signal list file with data."
    }

    if (/Workbook does not contain any worksheets/i.test(normalized)) {
      return "No worksheets were found in the file. Check the file and try again."
    }

    if (/Unable to find any column headers/i.test(normalized)) {
      return "Column headers could not be found. Make sure at least one row contains column names."
    }

    if (/Preset metadata is invalid/i.test(detail)) {
      return "The selected preset is invalid. Choose another preset or use manual setup."
    }

    if (/Invalid metadata JSON/i.test(detail)) {
      return "Invalid import settings. Reopen the dialog and try again."
    }

    if (detail.trim()) {
      return normalized || detail
    }

    return "Could not import the file. Check the data and try again."
  }

  if (err instanceof Error && err.message.trim()) {
    return err.message
  }

  return "Could not import the file. Please try again."
}

async function handleSelectedFile(selected: File | null) {
  if (!selected) {
    resetWorkflowState()
    return
  }

  const ext = selected.name.split(".").pop()?.toLowerCase()
  if (!ext || !ALLOWED_EXTENSIONS.includes(ext)) {
    error.value = "Unsupported file type. Please upload .xls, .xlsx, or .xlsm."
    resetWorkflowState({ preserveError: true })
    if (fileInput.value) fileInput.value.value = ""
    return
  }

  try {
    await parseWorkbook(selected)
    file.value = selected
    fileName.value = selected.name
    error.value = null
  } catch {
    if (fileInput.value) fileInput.value.value = ""
  }
}

function isColumnSelected(index: number) {
  return selectedColumnSet.value.has(index)
}

function toggleColumn(index: number) {
  if (!selectedSheetName.value) return
  const next = new Set(selectedColumnIndexes.value)
  if (next.has(index)) {
    next.delete(index)
  } else {
    next.add(index)
  }
  selectedColumnsBySheet.value[selectedSheetName.value] = Array.from(next).sort((a, b) => a - b)
}

function selectAllColumns() {
  if (!selectedSheetName.value) return
  const columns = sheetColumns.value[selectedSheetName.value] ?? []
  selectedColumnsBySheet.value[selectedSheetName.value] = columns.map(column => column.index)
}

function clearAllColumns() {
  if (!selectedSheetName.value) return
  selectedColumnsBySheet.value[selectedSheetName.value] = []
}

async function replaceFile() {
  if (loading.value || parsing.value) return
  resetWorkflowState()
  await nextTick()
  fileInput.value?.click()
}

function triggerFileDialog() {
  if (loading.value || parsing.value) return
  fileInput.value?.click()
}

function onDragEnter(event: DragEvent) {
  if (loading.value || parsing.value) return
  event.preventDefault()
  dragCounter.value += 1
  dropActive.value = true
}

function onDragOver(event: DragEvent) {
  if (loading.value || parsing.value) return
  event.preventDefault()
  dropActive.value = true
  if (dragCounter.value === 0) {
    dragCounter.value = 1
  }
  if (event.dataTransfer) {
    event.dataTransfer.dropEffect = "copy"
  }
}

function onDragLeave(event: DragEvent) {
  if (loading.value || parsing.value) return
  event.preventDefault()
  dragCounter.value = Math.max(0, dragCounter.value - 1)
  if (dragCounter.value === 0) {
    dropActive.value = false
  }
}

async function onDrop(event: DragEvent) {
  if (loading.value || parsing.value) return
  event.preventDefault()
  dropActive.value = false
  dragCounter.value = 0
  const dropped = event.dataTransfer?.files?.[0]
  await handleSelectedFile(dropped ?? null)
}

async function parseWorkbook(selected: File) {
  parsing.value = true
  try {
    const preview = await signalSheetStore.previewImportSheet(selected)
    if (!preview.sheets.length) {
      throw new Error("Workbook does not contain any worksheets.")
    }

    const columnsBySheet: Record<string, SheetColumn[]> = {}
    const rowsBySheet: Record<string, unknown[][]> = {}
    preview.sheets.forEach(sheet => {
      const sheetName = String(sheet.name || "Sheet")
      const headerRow = Array.isArray(sheet.headers) ? sheet.headers : []
      const columns: SheetColumn[] = []
      headerRow.forEach((value, idx) => {
        const normalized = normalizeHeaderValue(value)
        if (normalized) {
          columns.push({ header: normalized, index: idx })
        }
      })

      const orderedHeaders = columns.map(column => column.header)
      const matrix: unknown[][] = orderedHeaders.length ? [orderedHeaders] : []
      if (orderedHeaders.length) {
        const parsedRows = Array.isArray(sheet.rows) ? sheet.rows : []
        parsedRows.forEach(rawRow => {
          const row = rawRow && typeof rawRow === "object" && !Array.isArray(rawRow)
            ? (rawRow as Record<string, unknown>)
            : {}
          matrix.push(orderedHeaders.map(header => row[header] ?? null))
        })
      }

      columnsBySheet[sheetName] = columns
      rowsBySheet[sheetName] = matrix
    })

    if (!Object.values(columnsBySheet).some(columns => columns.length)) {
      throw new Error("Unable to find any column headers. Make sure at least one row contains column names.")
    }

    sheetColumns.value = columnsBySheet
    sheetRows.value = rowsBySheet
    selectedColumnsBySheet.value = Object.fromEntries(
      Object.entries(columnsBySheet).map(([sheetName, columns]) => [sheetName, columns.map(column => column.index)])
    )
    selectedSheetName.value = null
    applySelectedPreset()
    step.value = "columns"
  } catch (err) {
    const message =
      err instanceof Error
        ? err.message
        : "Unable to parse the uploaded workbook. Please verify the file contents and try again."
    error.value = message
    resetWorkflowState({ preserveError: true })
    throw err
  } finally {
    parsing.value = false
  }
}

function applySelectedPreset() {
  const preset = selectedPreset.value
  if (!preset) return
  applyPresetToSelection(preset.import_meta)

  if (step.value !== "upload" && typeStepValid.value) {
    step.value = "types"
  }
}

function requestDeleteSelectedPreset() {
  if (!selectedPreset.value || loading.value || parsing.value) return
  deletePresetOpen.value = true
}

async function confirmDeleteSelectedPreset() {
  const preset = selectedPreset.value
  if (!preset) {
    deletePresetOpen.value = false
    return
  }
  try {
    await signalSheetStore.deletePreset(preset.id)
    if (selectedPresetId.value === preset.id) {
      selectedPresetId.value = null
    }
    deletePresetOpen.value = false
    toastStore.success("Preset deleted")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function applyPresetToSelection(meta: SignalImportMeta) {
  suppressTypeMappingReset.value = true
  applyingPreset.value = true
  try {
    typeMapping.value = {}

    const requestedSheetName = meta.source_sheet_name || meta.sheet_name || null
    if (requestedSheetName && sheetColumns.value[requestedSheetName]) {
      selectedSheetName.value = requestedSheetName
    }

    if (!selectedSheetName.value) {
      return
    }

    const columns = sheetColumns.value[selectedSheetName.value] ?? []
    if (!columns.length) return

    const selectedFromPreset = resolveSelectedColumnIndexes(columns, meta.selected_columns ?? [])
    const selectedSet = new Set<number>(
      selectedFromPreset.length ? selectedFromPreset : columns.map(column => column.index),
    )

    if (meta.type_column) {
      const typeColumn = findColumnByHeader(columns, meta.type_column)
      if (typeColumn) {
        typeColumnIndex.value = typeColumn.index
        selectedSet.add(typeColumn.index)
      }
    }

    if (meta.terminal_column) {
      const terminalColumn = findColumnByHeader(columns, meta.terminal_column)
      if (terminalColumn) {
        terminalColumnIndex.value = terminalColumn.index
        selectedSet.add(terminalColumn.index)
      }
    }

    selectedColumnsBySheet.value[selectedSheetName.value] = Array.from(selectedSet).sort((a, b) => a - b)

    if (meta.type_mapping) {
      const options = buildTypeValueOptions()
      const optionsByKey = new Map(options.map(item => [item.key, item]))
      const validInternalTypes = new Set<InternalSignalType>(["di", "do", "ai", "ao"])
      const mapped: Record<string, InternalSignalType> = {}
      for (const [rawVendorType, rawDirection] of Object.entries(meta.type_mapping)) {
        const direction = String(rawDirection).trim().toLowerCase() as InternalSignalType
        if (!validInternalTypes.has(direction)) {
          continue
        }

        const normalizedVendorType = normalizeTypeKey(rawVendorType)
        let option = optionsByKey.get(normalizedVendorType)
        if (!option) {
          option = options.find(item => item.label.trim().toLowerCase() === rawVendorType.trim().toLowerCase())
        }
        if (!option) continue
        mapped[option.key] = direction
      }
      typeMapping.value = mapped
    }
  } finally {
    setTimeout(() => {
      applyingPreset.value = false
      suppressTypeMappingReset.value = false
    }, 0)
  }
}

function resolveSelectedColumnIndexes(columns: SheetColumn[], selectedHeaders: string[]): number[] {
  if (!selectedHeaders.length) return []
  const indices: number[] = []
  for (const header of selectedHeaders) {
    const column = findColumnByHeader(columns, header)
    if (!column) continue
    indices.push(column.index)
  }
  return Array.from(new Set(indices)).sort((a, b) => a - b)
}

function findColumnByHeader(columns: SheetColumn[], headerName: string): SheetColumn | undefined {
  const exact = columns.find(column => column.header === headerName)
  if (exact) return exact
  const normalizedHeader = headerName.trim().toLowerCase()
  return columns.find(column => column.header.trim().toLowerCase() === normalizedHeader)
}

function normalizeHeaderValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value.trim()
  if (value instanceof Date) return value.toISOString()
  return String(value).trim()
}

function sampleColumnValues(columnIndex: number | null, limit = 5): string[] {
  if (!selectedSheetName.value || columnIndex === null) return []
  const rows = sheetRows.value[selectedSheetName.value] ?? []
  const samples: string[] = []
  for (let rowIndex = 1; rowIndex < rows.length; rowIndex += 1) {
    const row = Array.isArray(rows[rowIndex]) ? rows[rowIndex] : []
    const formatted = formatCellValue(row[columnIndex])
    if (!formatted) continue
    samples.push(formatted)
    if (samples.length >= limit) break
  }
  return samples
}

function formatCellValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") {
    const trimmed = value.trim()
    return trimmed || "(blank)"
  }
  if (value instanceof Date) return value.toISOString()
  const stringValue = String(value).trim()
  return stringValue || "(blank)"
}

function normalizeTypeKey(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (value instanceof Date) return value.toISOString().trim().toLowerCase()
  const asString = typeof value === "string" ? value : String(value)
  return asString.trim().toLowerCase()
}

function formatTypeLabel(value: unknown): string {
  if (value === null || value === undefined) return "(blank)"
  if (typeof value === "string") {
    const trimmed = value.trim()
    return trimmed || "(blank)"
  }
  if (value instanceof Date) return value.toISOString()
  const stringValue = String(value).trim()
  return stringValue || "(blank)"
}

function buildTypeValueOptions() {
  if (!selectedSheetName.value || typeColumnIndex.value === null) return []
  const rows = sheetRows.value[selectedSheetName.value] ?? []
  const map = new Map<string, { label: string; count: number }>()
  for (let rowIndex = 1; rowIndex < rows.length; rowIndex += 1) {
    const row = Array.isArray(rows[rowIndex]) ? rows[rowIndex] : []
    const normalized = normalizeTypeKey(row[typeColumnIndex.value])
    if (!normalized) continue
    const label = formatTypeLabel(row[typeColumnIndex.value])
    if (map.has(normalized)) {
      map.get(normalized)!.count += 1
    } else {
      map.set(normalized, { label, count: 1 })
    }
  }
  return Array.from(map.entries()).map(([key, value]) => ({ key, ...value }))
}

function ensureValidColumnSelections() {
  const columns = selectedColumnOptions.value
  if (!columns.length) {
    terminalColumnIndex.value = null
    typeColumnIndex.value = null
    return
  }

  if (!columns.some(column => column.index === terminalColumnIndex.value)) {
    terminalColumnIndex.value = applyingPreset.value ? guessTerminalColumnIndex(columns) : null
  }

  if (!columns.some(column => column.index === typeColumnIndex.value)) {
    if (applyingPreset.value) {
      typeColumnIndex.value = guessTypeColumnIndex(columns)
    } else {
      typeColumnIndex.value = null
    }
  }
}

function guessTerminalColumnIndex(columns: SheetColumn[]): number | null {
  const heuristics = [/terminal/i, /клем/i, /клемм/i, /xt/i]
  for (const pattern of heuristics) {
    const match = columns.find(column => pattern.test(column.header))
    if (match) return match.index
  }
  return null
}

function guessTypeColumnIndex(columns: SheetColumn[]): number | null {
  const heuristics = [/type/i, /тип/i, /category/i]
  for (const pattern of heuristics) {
    const match = columns.find(column => pattern.test(column.header))
    if (match) return match.index
  }
  return columns[0]?.index ?? null
}

function onTypeMappingChange(key: string, value: string | number | null) {
  const normalizedValue = (typeof value === "string" ? value : "") as InternalSignalType | ""
  const next = { ...typeMapping.value }
  if (!normalizedValue) {
    delete next[key]
  } else {
    next[key] = normalizedValue
  }
  typeMapping.value = next
}

async function buildPreparedImportPayload(): Promise<{ file: File; metadata: SignalImportMeta }> {
  if (!selectedSheetName.value) {
    throw new Error("Select a worksheet before importing.")
  }
  const rows = sheetRows.value[selectedSheetName.value]
  if (!rows?.length) {
    throw new Error("Selected worksheet contains no data rows.")
  }
  const selectedIndexes = selectedColumnIndexes.value
  if (!selectedIndexes.length) {
    throw new Error("Select at least one column to continue.")
  }
  const selectionSet = new Set(selectedIndexes)
  const orderedColumns = (sheetColumns.value[selectedSheetName.value] ?? []).filter(column =>
    selectionSet.has(column.index)
  )
  if (!orderedColumns.length) {
    throw new Error("Select at least one column to continue.")
  }
  if (typeColumnIndex.value === null) {
    throw new Error("Choose a type column before importing.")
  }
  if (terminalColumnIndex.value === null) {
    throw new Error("Choose a terminal column before importing.")
  }
  const typeColumn = orderedColumns.find(column => column.index === typeColumnIndex.value)
  if (!typeColumn) {
    throw new Error("Type column must be part of the selection.")
  }
  const terminalColumn = orderedColumns.find(column => column.index === terminalColumnIndex.value)
  if (!terminalColumn) {
    throw new Error("Terminal column must be part of the selection.")
  }
  const mappingEntries = Object.entries(typeMapping.value)
  if (!mappingEntries.length) {
    throw new Error("Map at least one vendor type to an internal type.")
  }
  const normalizedMapping = Object.fromEntries(mappingEntries)
  const presetTypeMapping: Record<string, InternalSignalType> = Object.fromEntries(mappingEntries)

  let matchedRows = 0
  for (let rowIndex = 1; rowIndex < rows.length; rowIndex += 1) {
    const row = Array.isArray(rows[rowIndex]) ? rows[rowIndex] : []
    const normalized = normalizeTypeKey(row[typeColumnIndex.value])
    if (!normalized) continue
    const mappedType = normalizedMapping[normalized]
    if (!mappedType) continue
    matchedRows += 1
  }
  if (matchedRows === 0) {
    throw new Error("No rows match the selected type mapping.")
  }
  if (!file.value) {
    throw new Error("Upload file before importing.")
  }

  const metadata: SignalImportMeta = {
    sheet_name: selectedSheetName.value,
    source_sheet_name: selectedSheetName.value,
    selected_columns: orderedColumns.map(column => column.header),
    terminal_column: terminalColumn.header,
    type_column: typeColumn.header,
    type_mapping: presetTypeMapping,
  }

  return { file: file.value, metadata }
}

watch(
  () => selectedSheetName.value,
  sheet => {
    if (!sheet) return
    if (!(sheet in selectedColumnsBySheet.value)) {
      selectedColumnsBySheet.value[sheet] = sheetColumns.value[sheet]?.map(column => column.index) ?? []
    }
  },
)

watch(
  [() => selectedSheetName.value, () => selectedColumnIndexes.value],
  () => {
    ensureValidColumnSelections()
  },
  { immediate: true },
)

watch(
  () => ({ sheet: selectedSheetName.value, column: typeColumnIndex.value }),
  (current, previous) => {
    if (applyingPreset.value || suppressTypeMappingReset.value) {
      return
    }
    if (!previous || current.sheet !== previous.sheet || current.column !== previous.column) {
      typeMapping.value = {}
    }
  },
)

watch(
  () => typeValueOptions.value,
  options => {
    const valid = new Set(options.map(option => option.key))
    const next: Record<string, InternalSignalType> = {}
    Object.entries(typeMapping.value).forEach(([key, value]) => {
      if (valid.has(key)) {
        next[key] = value
      }
    })
    if (Object.keys(next).length !== Object.keys(typeMapping.value).length) {
      typeMapping.value = next
    }
  },
  { deep: true },
)

watch(
  () => selectedPresetId.value,
  () => {
    applySelectedPreset()
  },
)

watch(
  () => props.open,
  (open) => {
    if (!open) return
    void signalSheetStore.refreshPresets()
  },
  { immediate: true },
)
</script>

<style scoped>
.signal-import-modal__form {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.signal-import-modal__steps-card {
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0.025em;
  padding: 0.75rem;
  text-transform: uppercase;
}

.signal-import-modal__steps-row,
.signal-import-modal__step,
.signal-import-modal__dropzone-content,
.signal-import-modal__preset-block,
.signal-import-modal__section-stack,
.signal-import-modal__mapping-list {
  display: flex;
}

.signal-import-modal__steps-row {
  align-items: center;
  flex-wrap: wrap;
  gap: 0.5rem;
}

.signal-import-modal__step {
  align-items: center;
  gap: 0.5rem;
}

.signal-import-modal__step-label--active {
  color: var(--color-blue-600);
}

.signal-import-modal__step-label--complete {
  color: var(--color-neutral-500);
}

.signal-import-modal__step-label--upcoming,
.signal-import-modal__step-separator {
  color: var(--color-neutral-400);
}

.signal-import-modal__upload-field,
.signal-import-modal__field {
  display: block;
}

.signal-import-modal__strong-label,
.signal-import-modal__label {
  color: var(--color-neutral-700);
  display: block;
  font-size: var(--text-sm);
  margin-bottom: 0.25rem;
}

.signal-import-modal__strong-label {
  font-weight: 600;
}

.signal-import-modal__label {
  font-weight: 500;
}

.signal-import-modal__dropzone {
  align-items: center;
  border: 2px dashed;
  border-radius: 1rem;
  display: flex;
  flex-direction: column;
  font-size: var(--text-sm);
  justify-content: center;
  padding: 2.5rem 1.5rem;
  text-align: center;
  transition: background-color 0.15s, border-color 0.15s, color 0.15s, opacity 0.15s;
}

.signal-import-modal__dropzone--active {
  background: var(--color-emerald-50);
  border-color: var(--color-emerald-500);
  color: var(--color-emerald-700);
}

.signal-import-modal__dropzone--idle {
  background: var(--color-white);
  border-color: var(--color-neutral-300);
  color: var(--color-neutral-600);
}

.signal-import-modal__dropzone--idle:hover {
  background: var(--color-blue-100);
  border-color: var(--color-blue-500);
}

.signal-import-modal__dropzone--disabled {
  opacity: 0.6;
  pointer-events: none;
}

.signal-import-modal__dropzone-content {
  align-items: center;
  flex-direction: column;
  gap: 0.5rem;
}

.signal-import-modal__dropzone-title {
  color: var(--color-neutral-800);
  font-size: var(--text-base);
  font-weight: 600;
}

.signal-import-modal__muted {
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.signal-import-modal__muted--xs {
  font-size: var(--text-xs);
}

.signal-import-modal__browse-link {
  color: var(--color-blue-600);
  text-underline-offset: 2px;
}

.signal-import-modal__dropzone--idle:hover .signal-import-modal__browse-link {
  text-decoration: underline;
}

.signal-import-modal__file-help {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  margin-top: 1rem;
}

.signal-import-modal__file-meta {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  margin-top: 0.25rem;
}

.signal-import-modal__file-name {
  font-weight: 500;
}

.signal-import-modal__parsing-text {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  margin-top: 0.5rem;
}

.signal-import-modal__file-input {
  border: 0;
  clip: rect(0 0 0 0);
  height: 1px;
  margin: -1px;
  overflow: hidden;
  padding: 0;
  position: absolute;
  white-space: nowrap;
  width: 1px;
}

.signal-import-modal__preset-block {
  flex-direction: column;
  gap: 0.5rem;
  margin-top: 1rem;
}

.signal-import-modal__eyebrow {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0.025em;
  text-transform: uppercase;
}

.signal-import-modal__inline-actions {
  display: flex;
  justify-content: flex-end;
}

.signal-import-modal__ready-card {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  font-size: var(--text-sm);
  padding: 0.75rem;
}

.signal-import-modal__ready-row {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.signal-import-modal__ready-title,
.signal-import-modal__section-title,
.signal-import-modal__mapping-title {
  color: var(--color-neutral-800);
}

.signal-import-modal__ready-title,
.signal-import-modal__section-title {
  font-size: var(--text-sm);
  font-weight: 600;
}

.signal-import-modal__mapping-title {
  font-weight: 500;
}

.signal-import-modal__nowrap {
  white-space: nowrap;
}

.signal-import-modal__card-alert {
  margin-top: 0.75rem;
}

.signal-import-modal__section-stack {
  flex-direction: column;
  gap: 1rem;
}

.signal-import-modal__warning-text {
  color: var(--color-amber-700);
  font-size: var(--text-xs);
  margin-top: 0.5rem;
}

.signal-import-modal__section-heading {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.signal-import-modal__button-pair {
  display: flex;
  gap: 0.5rem;
}

.signal-import-modal__option-list {
  display: grid;
  gap: 0.5rem;
  margin-top: 0.5rem;
}

.signal-import-modal__option {
  align-items: center;
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  display: flex;
  gap: 0.5rem;
  padding: 0.5rem 0.75rem;
}

.signal-import-modal__checkbox {
  accent-color: var(--color-blue-600);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-sm);
  height: 1rem;
  width: 1rem;
}

.signal-import-modal__checkbox:focus {
  outline: 2px solid var(--color-blue-500);
  outline-offset: 2px;
}

.signal-import-modal__option-label {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
}

.signal-import-modal__spaced-xs {
  margin-top: 0.25rem;
}

.signal-import-modal__strong {
  color: var(--color-neutral-700);
  font-weight: 600;
}

.signal-import-modal__mapping-list {
  flex-direction: column;
  gap: 0.5rem;
}

.signal-import-modal__mapping-row {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.75rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  padding: 0.5rem 0.75rem;
}

.signal-import-modal__input {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-lg);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  padding: 0.5rem 0.75rem;
  width: 100%;
}

.signal-import-modal__input:focus {
  outline: 2px solid var(--color-blue-500);
  outline-offset: 1px;
}

.signal-import-modal__footer {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  justify-content: flex-end;
  width: 100%;
}

:global(.dark .signal-import-modal__steps-card) {
  background: color-mix(in srgb, var(--color-neutral-900) 40%, transparent);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-300);
}

:global(.dark .signal-import-modal__step-label--active) {
  color: var(--color-blue-400);
}

:global(.dark .signal-import-modal__step-label--complete) {
  color: var(--color-neutral-300);
}

:global(.dark .signal-import-modal__step-label--upcoming),
:global(.dark .signal-import-modal__step-separator),
:global(.dark .signal-import-modal__muted),
:global(.dark .signal-import-modal__file-help),
:global(.dark .signal-import-modal__file-meta),
:global(.dark .signal-import-modal__parsing-text),
:global(.dark .signal-import-modal__eyebrow) {
  color: var(--color-neutral-400);
}

:global(.dark .signal-import-modal__strong-label),
:global(.dark .signal-import-modal__label),
:global(.dark .signal-import-modal__strong) {
  color: var(--color-neutral-200);
}

:global(.dark .signal-import-modal__dropzone-title),
:global(.dark .signal-import-modal__ready-title),
:global(.dark .signal-import-modal__section-title),
:global(.dark .signal-import-modal__mapping-title),
:global(.dark .signal-import-modal__option-label),
:global(.dark .signal-import-modal__input) {
  color: var(--color-neutral-100);
}

:global(.dark .signal-import-modal__dropzone--active) {
  background: color-mix(in srgb, var(--color-emerald-500) 20%, transparent);
  border-color: var(--color-emerald-400);
  color: var(--color-emerald-300);
}

:global(.dark .signal-import-modal__dropzone--idle),
:global(.dark .signal-import-modal__ready-card),
:global(.dark .signal-import-modal__option),
:global(.dark .signal-import-modal__mapping-row),
:global(.dark .signal-import-modal__input) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-import-modal__dropzone--idle:hover) {
  background: var(--color-neutral-800);
  border-color: var(--color-blue-500);
}

:global(.dark .signal-import-modal__browse-link) {
  color: var(--color-blue-300);
}

:global(.dark .signal-import-modal__warning-text) {
  color: var(--color-amber-300);
}

@media (min-width: 640px) {
  .signal-import-modal__ready-row,
  .signal-import-modal__section-heading,
  .signal-import-modal__mapping-row {
    align-items: center;
    flex-direction: row;
    justify-content: space-between;
  }

  .signal-import-modal__mapping-control {
    width: 13rem;
  }

  .signal-import-modal__option-list {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (min-width: 1024px) {
  .signal-import-modal__option-list {
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }
}
</style>
