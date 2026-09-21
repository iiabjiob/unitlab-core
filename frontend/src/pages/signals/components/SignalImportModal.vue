<template>
  <UiModal
    :open="open"
    title="Import Signal List"
    max-width="3xl"
    desktop-height="80vh"
    :content-scroll="false"
    @close="emitClose"
  >
    <form id="signal-import-form" class="signal-import-modal__form" @submit.prevent="handleSubmit">
      <input
        ref="fileInput"
        class="signal-import-modal__file-input"
        type="file"
        accept=".xls,.xlsx,.xlsm"
        tabindex="-1"
        aria-hidden="true"
        @change="onFileChange"
      />
      <div class="signal-import-modal__body">
        <div class="signal-import-modal__steps-card">
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

        <div
          v-if="!file"
          class="signal-import-modal__empty-state"
          @dragenter="onDragEnter"
          @dragover="onDragOver"
          @dragleave="onDragLeave"
          @drop="onDrop"
        >
          <p class="signal-import-modal__ready-title">Choose a signal list file to start.</p>
          <p class="signal-import-modal__muted signal-import-modal__muted--xs">
            You can drag and drop .xls, .xlsx, or .xlsm files here.
          </p>
          <div class="signal-import-modal__inline-actions signal-import-modal__inline-actions--start">
            <UiButton
              type="button"
              variant="secondary"
              size="sm"
              @click="triggerFileDialog"
              :disabled="loading || parsing"
            >
              Choose file
            </UiButton>
          </div>
        </div>

        <div v-else class="signal-import-modal__ready-card">
          <div class="signal-import-modal__ready-row">
            <div class="signal-import-modal__file-state">
              <span
                v-if="parsing"
                class="signal-import-modal__spinner"
                aria-hidden="true"
              />
              <div>
                <p class="signal-import-modal__ready-title">
                  {{ parsing ? "Loading signal list…" : "File ready" }}
                </p>
                <p class="signal-import-modal__muted signal-import-modal__muted--xs">
                  {{ parsing ? "Reading worksheets and preparing the import wizard." : fileName }}
                </p>
              </div>
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

        <div
          v-if="file && parsing"
          class="signal-import-modal__parsing-card"
          role="status"
          aria-live="polite"
        >
          <span class="signal-import-modal__parsing-spinner" aria-hidden="true" />
          <div>
            <p class="signal-import-modal__section-title">Loading workbook</p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs">
              Please wait while worksheets and headers are detected.
            </p>
          </div>
          <div class="signal-import-modal__parsing-progress" aria-hidden="true">
            <span />
          </div>
        </div>

        <div
          v-if="file && loading"
          class="signal-import-modal__parsing-card signal-import-modal__importing-card"
          role="status"
          aria-live="polite"
        >
          <span class="signal-import-modal__parsing-spinner" aria-hidden="true" />
          <div>
            <p class="signal-import-modal__section-title">Importing signal list</p>
            <p class="signal-import-modal__muted signal-import-modal__muted--xs">{{ importPhase }}</p>
          </div>
          <div class="signal-import-modal__importing-meta">
            <span>Elapsed: {{ formatImportDuration(importElapsedSeconds) }}</span>
            <span v-if="importRemainingSeconds !== null">
              Estimated remaining: ~{{ formatImportDuration(importRemainingSeconds) }}
            </span>
            <span v-else>Still processing…</span>
          </div>
          <div class="signal-import-modal__parsing-progress" aria-hidden="true">
            <span />
          </div>
        </div>

        <div v-else-if="file" class="signal-import-modal__step-content">
          <div v-if="step === 'columns'" class="signal-import-modal__section-stack signal-import-modal__section-stack--columns">
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

            <div v-if="availableColumns.length" class="signal-import-modal__columns-panel">
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
              <div class="signal-import-modal__option-scroll">
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
          </div>

          <div v-else-if="step === 'verification'" class="signal-import-modal__section-stack">
            <div>
              <p class="signal-import-modal__section-title">IEC 61850</p>
              <p class="signal-import-modal__muted signal-import-modal__muted--xs">
                Choose the exact columns that carry the IP address and IEC 61850 address for real MMS verification.
                Leave both empty to import without verification metadata.
              </p>
            </div>

            <div class="signal-import-modal__field">
              <p class="signal-import-modal__label">IP address column</p>
              <UiAffinoListbox
                v-model="transportHostColumnName"
                :options="verificationColumnListboxOptions"
                placeholder="Skip verification"
                aria-label="IP address column"
                :disabled="loading || parsing || verificationColumnListboxOptions.length === 0"
              />
            </div>

            <div class="signal-import-modal__field">
              <p class="signal-import-modal__label">IEC 61850 address column</p>
              <UiAffinoListbox
                v-model="iec61850AddressColumnName"
                :options="verificationColumnListboxOptions"
                placeholder="Skip verification"
                aria-label="IEC 61850 address column"
                :disabled="loading || parsing || verificationColumnListboxOptions.length === 0"
              />
            </div>

            <UiAlert
              :type="verificationSelectionNotice.type"
              class="signal-import-modal__card-alert"
              :message="verificationSelectionNotice.message"
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

          <div v-else-if="step === 'network'" class="signal-import-modal__section-stack">
            <div>
              <p class="signal-import-modal__section-title">Configure Network</p>
              <p class="signal-import-modal__muted signal-import-modal__muted--xs">
                Apply a static RJ45 address only when the Pi is connected to the project IEC 61850 network.
              </p>
            </div>

            <UiAlert
              v-if="!networkDeviceIps.length"
              type="info"
              message="No device IP addresses were detected from the selected import metadata. Network configuration is optional for this import."
            />

            <div v-if="networkDeviceIps.length" class="signal-import-modal__network-panel">
              <div class="signal-import-modal__network-grid">
                <div class="signal-import-modal__network-fact">
                  <span class="signal-import-modal__network-label">Detected subnet</span>
                  <span class="signal-import-modal__network-value">{{ selectedNetworkCidr || "Select subnet" }}</span>
                </div>
                <div class="signal-import-modal__network-fact">
                  <span class="signal-import-modal__network-label">Ethernet interface</span>
                  <span class="signal-import-modal__network-value">{{ detectedEthernetInterface?.interface_name || "Not detected" }}</span>
                </div>
                <div class="signal-import-modal__network-fact">
                  <span class="signal-import-modal__network-label">Current IP</span>
                  <span class="signal-import-modal__network-value">{{ detectedEthernetInterface?.local_ip || "No IPv4" }}</span>
                </div>
                <div class="signal-import-modal__network-fact">
                  <span class="signal-import-modal__network-label">Link status</span>
                  <span class="signal-import-modal__network-value">{{ ethernetLinkStatus }}</span>
                </div>
                <div class="signal-import-modal__network-fact">
                  <span class="signal-import-modal__network-label">Suggested IP</span>
                  <span class="signal-import-modal__network-value">{{ suggestedPiIp || "No free candidate" }}</span>
                </div>
                <div class="signal-import-modal__network-fact">
                  <span class="signal-import-modal__network-label">Subnet mask</span>
                  <span class="signal-import-modal__network-value">{{ selectedSubnetMask || "255.255.255.0" }}</span>
                </div>
              </div>

              <div v-if="networkSubnetOptions.length > 1" class="signal-import-modal__field">
                <p class="signal-import-modal__label">Project subnet</p>
                <UiAffinoListbox
                  v-model="selectedNetworkCidr"
                  :options="networkSubnetOptions"
                  placeholder="Select project subnet"
                  aria-label="Project subnet"
                  :disabled="loading || parsing || networkBusy"
                />
                <p class="signal-import-modal__warning-text">
                  Multiple /24 networks were found in the imported device IPs. Select the subnet connected to the Pi RJ45 port.
                </p>
              </div>

              <UiAlert
                v-if="networkWarning"
                type="warning"
                :message="networkWarning"
              />

              <div class="signal-import-modal__network-actions">
                <UiButton
                  type="button"
                  variant="secondary"
                  size="sm"
                  :disabled="!canApplyNetwork || networkBusy || loading || parsing"
                  @click="applySuggestedNetwork"
                >
                  {{ networkBusy ? "Applying..." : "Apply" }}
                </UiButton>
                <UiButton
                  v-if="coreNetworkStore.snapshot?.previous_host_network"
                  type="button"
                  variant="ghost"
                  size="sm"
                  :disabled="networkBusy || loading || parsing"
                  @click="restorePreviousNetwork"
                >
                  Restore previous config
                </UiButton>
              </div>

              <p v-if="networkProgress" class="signal-import-modal__muted signal-import-modal__muted--xs">
                {{ networkProgress }}
              </p>
              <UiAlert v-if="networkError" type="error" :message="networkError" />

              <div v-if="networkSummary" class="signal-import-modal__network-summary">
                <p class="signal-import-modal__mapping-title">Connectivity summary</p>
                <p class="signal-import-modal__muted signal-import-modal__muted--xs">
                  Reachable: {{ networkSummary.reachable.length }} · Not reachable: {{ networkSummary.notReachable.length }} · Unknown: {{ networkSummary.unknown.length }}
                </p>
                <p class="signal-import-modal__muted signal-import-modal__muted--xs">
                  Current config: {{ networkSummary.currentConfig }}
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>

    </form>

    <template #footer>
      <div class="signal-import-modal__footer">
        <UiButton type="button" variant="secondary" @click="emitClose" :disabled="loading || parsing || networkBusy">
          Cancel
        </UiButton>
        <UiButton
          v-if="canGoBack"
          type="button"
          variant="ghost"
          @click="goToPreviousStep"
          :disabled="loading || parsing || networkBusy"
        >
          Back
        </UiButton>
        <UiButton
          v-if="!isFinalStep"
          type="button"
          variant="primary"
          :disabled="!canAdvance || loading || networkBusy"
          @click="goToNextStep"
        >
          Next
        </UiButton>
        <UiButton
          v-else
          type="submit"
          form="signal-import-form"
          variant="primary"
          :disabled="!canSubmitFinal || loading || networkBusy"
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
import { computed, nextTick, onUnmounted, ref, watch } from "vue"

import { getHttpErrorContext } from "@/api/httpErrors"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiAlert from "@/components/ui/UiAlert.vue"
import UiAffinoListbox from "@/components/ui/UiAffinoListbox.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import { useCoreNetworkStore } from "@/stores/coreNetworkStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"
import type { CoreNetAddressProbeSnapshot } from "@/types/coreNetwork"
import type {
  InternalSignalType,
  SignalImportMeta,
  SignalImportVerificationMeta,
  SignalSheetPreset,
} from "@/types/signal"
import {
  collectUniqueDeviceIps,
  inferProjectSubnets,
  isIpInSubnet,
  selectRj45Interface,
  subnetMaskForCidr,
  suggestStaticPiAddress,
} from "@/pages/signals/utils/importNetworkConfiguration"

const props = defineProps<{ open: boolean; seedFile?: File | null }>()

const emit = defineEmits<{ (e: "close"): void; (e: "imported", sheetId: number): void }>()

const coreNetworkStore = useCoreNetworkStore()
const signalSheetStore = useSignalSheetStore()
const toastStore = useToastStore()
const ALLOWED_EXTENSIONS = ["xls", "xlsx", "xlsm"]
const NETWORK_POLL_DELAY_MS = 1000
const NETWORK_APPLY_TIMEOUT_MS = 15000
const NETWORK_PROBE_TIMEOUT_MS = 12000
const NETWORK_PROBE_BATCH_SIZE = 256

const BASE_STEP_ITEMS = [
  { id: "columns", label: "Columns" },
  { id: "terminal", label: "Terminal" },
  { id: "types", label: "Type mapping" },
] as const
const VERIFICATION_STEP_ITEM = { id: "verification", label: "IEC61850" } as const
const NETWORK_STEP_ITEM = { id: "network", label: "Configure Network" } as const
type WizardStep =
  | (typeof BASE_STEP_ITEMS)[number]["id"]
  | typeof VERIFICATION_STEP_ITEM.id
  | typeof NETWORK_STEP_ITEM.id

const step = ref<WizardStep>("columns")
const file = ref<File | null>(null)
const fileName = ref("")
const loading = ref(false)
const error = ref<string | null>(null)
const errorAnchorRef = ref<HTMLElement | null>(null)
const fileInput = ref<HTMLInputElement | null>(null)
const parsing = ref(false)
const importPhase = ref("Preparing workbook…")
const importElapsedSeconds = ref(0)
let importStartedAtMs: number | null = null
let importElapsedTimer: ReturnType<typeof setInterval> | null = null
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
const transportHostColumnName = ref<string | null>(null)
const iec61850AddressColumnName = ref<string | null>(null)
const applyingPreset = ref(false)
const suppressTypeMappingReset = ref(false)
const deletePresetOpen = ref(false)
const lastSeedFileKey = ref("")
const selectedNetworkCidr = ref<string | null>(null)
const networkBusy = ref(false)
const networkProgress = ref<string | null>(null)
const networkError = ref<string | null>(null)
const occupiedCandidateIps = ref<string[]>([])
const networkSummary = ref<{
  reachable: string[]
  notReachable: string[]
  unknown: string[]
  currentConfig: string
} | null>(null)

const selectedImportRowCount = computed(() => {
  if (!selectedSheetName.value) return 0
  return Math.max(0, (sheetRows.value[selectedSheetName.value]?.length ?? 0) - 1)
})
const estimatedImportSeconds = computed(() => {
  const fileSizeMb = (file.value?.size ?? 0) / (1024 * 1024)
  return Math.max(3, Math.ceil(2 + selectedImportRowCount.value / 2_000 + fileSizeMb / 2))
})
const importRemainingSeconds = computed(() => {
  if (!loading.value || importElapsedSeconds.value >= estimatedImportSeconds.value) {
    return loading.value ? null : 0
  }
  return Math.max(1, estimatedImportSeconds.value - importElapsedSeconds.value)
})

const stepItems = computed(() => [...BASE_STEP_ITEMS, VERIFICATION_STEP_ITEM, NETWORK_STEP_ITEM])
const stepOrder = computed<WizardStep[]>(() => stepItems.value.map(item => item.id))
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
const activeStepIndex = computed(() => stepOrder.value.indexOf(step.value))
const isFinalStep = computed(() => activeStepIndex.value === stepOrder.value.length - 1)
const canGoBack = computed(() => activeStepIndex.value > 0)
const columnsStepValid = computed(
  () => !!selectedSheetName.value && availableColumns.value.length > 0 && selectedColumnCount.value > 0,
)
const terminalStepValid = computed(
  () => columnsStepValid.value && terminalColumnIndex.value !== null,
)
const typeStepValid = computed(
  () => terminalStepValid.value && typeColumnIndex.value !== null && typeValueOptions.value.length > 0 && hasTypeMappings.value,
)
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
const verificationColumnListboxOptions = computed(() =>
  availableColumns.value.map(column => ({ value: column.header, label: column.header })),
)
const transportHostColumnIndex = computed(() => {
  if (!transportHostColumnName.value) return null
  return availableColumns.value.find(column => column.header === transportHostColumnName.value)?.index ?? null
})
const networkDeviceIps = computed(() => {
  const rows = selectedSheetName.value ? sheetRows.value[selectedSheetName.value] ?? [] : []
  return collectUniqueDeviceIps(rows, transportHostColumnIndex.value)
})
const networkSubnets = computed(() => inferProjectSubnets(networkDeviceIps.value))
const networkSubnetOptions = computed(() =>
  networkSubnets.value.map(subnet => ({
    value: subnet.cidr,
    label: `${subnet.cidr} · ${subnet.deviceCount} device${subnet.deviceCount === 1 ? "" : "s"}`,
  })),
)
const detectedEthernetInterface = computed(() =>
  selectRj45Interface(coreNetworkStore.interfaces, coreNetworkStore.hostNetwork?.interface),
)
const selectedSubnetMask = computed(() => subnetMaskForCidr(selectedNetworkCidr.value))
const suggestedPiIp = computed(() =>
  suggestStaticPiAddress(selectedNetworkCidr.value, networkDeviceIps.value, occupiedCandidateIps.value),
)
const ethernetLinkStatus = computed(() => {
  const iface = detectedEthernetInterface.value
  if (!iface) return "Not detected"
  if (iface.carrier === true) return iface.oper_state || iface.state || "link detected"
  if (iface.carrier === false) return iface.oper_state || iface.state || "no carrier"
  return iface.oper_state || iface.state || "unknown"
})
const networkWarning = computed(() => {
  if (!networkDeviceIps.value.length) return null
  if (!detectedEthernetInterface.value) return "No wired RJ45 interface was detected. Wi-Fi will not be modified."
  if (detectedEthernetInterface.value.carrier === false) return "RJ45 link is down. Connect the project network before applying."
  if (
    detectedEthernetInterface.value.local_ip
    && selectedNetworkCidr.value
    && !isIpInSubnet(detectedEthernetInterface.value.local_ip, selectedNetworkCidr.value)
  ) {
    return "Current RJ45 IP is not in the imported project subnet."
  }
  return null
})
const canApplyNetwork = computed(() =>
  !!selectedNetworkCidr.value
  && !!detectedEthernetInterface.value
  && !!suggestedPiIp.value
  && networkDeviceIps.value.length > 0,
)
const verificationSelectionState = computed(() => {
  const host = transportHostColumnName.value
  const address = iec61850AddressColumnName.value
  if (!host && !address) return "empty"
  if (host && address) {
    return host === address ? "conflict" : "ready"
  }
  return "partial"
})
const verificationSelectionNotice = computed(() => {
  if (verificationSelectionState.value === "partial") {
    return {
      type: "warning" as const,
      message: "Choose both columns to save MMS verification metadata, or clear both to import without it.",
    }
  }
  if (verificationSelectionState.value === "conflict") {
    return {
      type: "warning" as const,
      message: "Choose two different columns, or clear both to import without it.",
    }
  }
  return {
    type: "info" as const,
    message: "Both columns are optional. Leave them empty to import without MMS verification metadata.",
  }
})
const deletePresetMessage = computed(() => {
  const preset = selectedPreset.value
  if (!preset) return ""
  return `Preset "${preset.name}" will be deleted.`
})
const canAdvance = computed(() => {
  if (step.value === "columns") return columnsStepValid.value
  if (step.value === "terminal") return terminalStepValid.value
  if (step.value === "types") return typeStepValid.value
  if (step.value === "verification") {
    return typeStepValid.value && (verificationSelectionState.value === "empty" || verificationSelectionState.value === "ready")
  }
  return false
})
const canSubmitFinal = computed(() => {
  if (!isFinalStep.value) {
    return false
  }
  if (step.value === "network") {
    return typeStepValid.value && (verificationSelectionState.value === "empty" || verificationSelectionState.value === "ready")
  }
  return typeStepValid.value
})

function stepIndicatorClass(target: WizardStep) {
  const targetIndex = stepOrder.value.indexOf(target)
  const currentIndex = activeStepIndex.value
  if (targetIndex === currentIndex) return "signal-import-modal__step-label signal-import-modal__step-label--active"
  if (targetIndex < currentIndex) return "signal-import-modal__step-label signal-import-modal__step-label--complete"
  return "signal-import-modal__step-label signal-import-modal__step-label--upcoming"
}

function goToNextStep() {
  if (loading.value || parsing.value) return
  if (!canAdvance.value) return
  const currentIndex = activeStepIndex.value
  if (currentIndex === -1 || currentIndex >= stepOrder.value.length - 1) return
  step.value = stepOrder.value[currentIndex + 1]
  if (step.value === "network") {
    void coreNetworkStore.refreshInterfaces().catch(() => undefined)
  }
}

function goToPreviousStep() {
  if (loading.value || parsing.value) return
  const currentIndex = activeStepIndex.value
  if (currentIndex <= 0) return
  step.value = stepOrder.value[currentIndex - 1]
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
  stopImportElapsedTimer()
  clearWorkbookState()
  resetNetworkState()
  file.value = null
  fileName.value = ""
  importPhase.value = "Preparing workbook…"
  importElapsedSeconds.value = 0
  terminalColumnIndex.value = null
  typeColumnIndex.value = null
  typeMapping.value = {}
  savePresetName.value = ""
  transportHostColumnName.value = null
  iec61850AddressColumnName.value = null
  if (!options.preserveError) {
    error.value = null
  }
  if (fileInput.value) {
    fileInput.value.value = ""
  }
  dropActive.value = false
  dragCounter.value = 0
}

function startImportElapsedTimer() {
  stopImportElapsedTimer()
  importStartedAtMs = Date.now()
  importElapsedSeconds.value = 0
  importElapsedTimer = setInterval(() => {
    if (importStartedAtMs === null) return
    importElapsedSeconds.value = Math.floor((Date.now() - importStartedAtMs) / 1000)
  }, 250)
}

function stopImportElapsedTimer() {
  if (importElapsedTimer !== null) {
    clearInterval(importElapsedTimer)
    importElapsedTimer = null
  }
  importStartedAtMs = null
}

onUnmounted(stopImportElapsedTimer)

function formatImportDuration(seconds: number): string {
  const normalized = Math.max(0, Math.round(seconds))
  if (normalized < 60) return `${normalized}s`
  const minutes = Math.floor(normalized / 60)
  return `${minutes}m ${String(normalized % 60).padStart(2, "0")}s`
}

function clearWorkbookState() {
  sheetColumns.value = {}
  sheetRows.value = {}
  selectedSheetName.value = null
  selectedColumnsBySheet.value = {}
  terminalColumnIndex.value = null
  step.value = "columns"
}

function resetNetworkState() {
  selectedNetworkCidr.value = null
  networkBusy.value = false
  networkProgress.value = null
  networkError.value = null
  occupiedCandidateIps.value = []
  networkSummary.value = null
}

async function handleSubmit() {
  if (!canSubmitFinal.value || loading.value || parsing.value || !isFinalStep.value) return
  loading.value = true
  importPhase.value = "Preparing workbook…"
  startImportElapsedTimer()
  error.value = null
  try {
    const payload = await buildPreparedImportPayload()
    importPhase.value = "Uploading and saving signal rows…"
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
    stopImportElapsedTimer()
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
    return
  }

  const ext = selected.name.split(".").pop()?.toLowerCase()
  if (!ext || !ALLOWED_EXTENSIONS.includes(ext)) {
    error.value = "Unsupported file type. Please upload .xls, .xlsx, or .xlsm."
    if (fileInput.value) fileInput.value.value = ""
    return
  }

  try {
    resetWorkflowState()
    file.value = selected
    fileName.value = selected.name
    await parseWorkbook(selected)
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

function openFileDialog() {
  if (loading.value || parsing.value) return
  const input = fileInput.value
  if (!input) return

  input.value = ""
  const picker = input as HTMLInputElement & { showPicker?: () => void }
  try {
    if (typeof picker.showPicker === "function") {
      picker.showPicker()
      return
    }
  } catch {
    // Fallback below.
  }

  input.click()
}

function replaceFile() {
  openFileDialog()
}

function triggerFileDialog() {
  openFileDialog()
}

defineExpose({
  triggerFileDialog,
})

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
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function applyPresetToSelection(meta: SignalImportMeta) {
  suppressTypeMappingReset.value = true
  applyingPreset.value = true
  try {
    typeMapping.value = {}
    const verification = meta.verification
    transportHostColumnName.value = verification?.transport_host_column
      ?? verification?.transport_host_column_hint?.column
      ?? verification?.transport_reference_column_hint?.column
      ?? null
    iec61850AddressColumnName.value = verification?.iec61850_address_column
      ?? verification?.iec61850_address_column_hint?.column
      ?? null
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
    throw new Error("Select a file before importing.")
  }

  const verificationMetadata = buildVerificationMetadata()
  const selectedHeaders = new Set(orderedColumns.map(column => column.header))
  if (verificationMetadata) {
    if (verificationMetadata.transport_host_column) {
      selectedHeaders.add(verificationMetadata.transport_host_column)
    }
    if (verificationMetadata.iec61850_address_column) {
      selectedHeaders.add(verificationMetadata.iec61850_address_column)
    }
  }

  const metadata: SignalImportMeta = {
    sheet_name: selectedSheetName.value,
    source_sheet_name: selectedSheetName.value,
    selected_columns: Array.from(selectedHeaders),
    terminal_column: terminalColumn.header,
    type_column: typeColumn.header,
    type_mapping: presetTypeMapping,
    verification: verificationMetadata,
  }

  return { file: file.value, metadata }
}

function buildVerificationMetadata(): SignalImportVerificationMeta | null {
  const hostColumn = transportHostColumnName.value?.trim() || null
  const iecColumn = iec61850AddressColumnName.value?.trim() || null
  if (!hostColumn && !iecColumn) {
    return null
  }
  if (!hostColumn || !iecColumn) {
    throw new Error("Choose both IP and IEC 61850 columns, or clear both to import without verification metadata.")
  }

  return {
    enabled: true,
    transport_host_column: hostColumn,
    iec61850_address_column: iecColumn,
  }
}

async function applySuggestedNetwork() {
  const iface = detectedEthernetInterface.value
  if (!iface || !selectedNetworkCidr.value) return
  networkBusy.value = true
  networkError.value = null
  networkSummary.value = null
  try {
    let candidate = suggestedPiIp.value
    while (candidate) {
      networkProgress.value = `Checking ${candidate} for address conflict.`
      const conflictProbe = await requestAddressProbe(iface.interface_name, [candidate])
      const result = conflictProbe?.results.find(item => item.address === candidate)
      if (result?.reachable === true) {
        occupiedCandidateIps.value = [...occupiedCandidateIps.value, candidate]
        candidate = suggestStaticPiAddress(selectedNetworkCidr.value, networkDeviceIps.value, occupiedCandidateIps.value)
        continue
      }
      break
    }

    if (!candidate) {
      throw new Error("No available static IP candidate was found in the selected subnet.")
    }

    const prefix = selectedNetworkCidr.value.split("/", 2)[1] || "24"
    networkProgress.value = `Applying ${candidate}/${prefix} to ${iface.interface_name}.`
    await coreNetworkStore.applySettings({
      interface: iface.interface_name,
      profile: coreNetworkStore.hostNetwork?.profile || "unitlab-lan",
      ipv4_mode: "manual",
      address_cidr: `${candidate}/${prefix}`,
      gateway: null,
      dns_servers: [],
      proxy_url: coreNetworkStore.hostNetwork?.proxy_url ?? null,
      proxy_no_proxy: coreNetworkStore.hostNetwork?.proxy_no_proxy ?? [],
    })
    await waitForAppliedAddress(iface.interface_name, candidate)

    networkProgress.value = "Testing reachability of imported device IPs."
    const reachability = await requestAddressProbes(iface.interface_name, networkDeviceIps.value)
    networkSummary.value = buildNetworkSummary(reachability)
    networkProgress.value = "Network configuration applied and verified."
    toastStore.success("Network configuration applied")
  } catch (err) {
    networkError.value = err instanceof Error ? err.message : String(err)
  } finally {
    networkBusy.value = false
  }
}

async function restorePreviousNetwork() {
  networkBusy.value = true
  networkError.value = null
  try {
    networkProgress.value = "Restoring previous RJ45 configuration."
    await coreNetworkStore.restoreSettings()
    await delay(NETWORK_POLL_DELAY_MS)
    await coreNetworkStore.refreshState({ force: true })
    networkProgress.value = "Previous network configuration restore requested."
  } catch (err) {
    networkError.value = err instanceof Error ? err.message : String(err)
  } finally {
    networkBusy.value = false
  }
}

async function requestAddressProbe(interfaceName: string, addresses: string[]): Promise<CoreNetAddressProbeSnapshot | null> {
  const uniqueAddresses = Array.from(new Set(addresses)).filter(Boolean)
  if (!uniqueAddresses.length) return null
  const accepted = await coreNetworkStore.probeAddresses({
    interface: interfaceName,
    addresses: uniqueAddresses,
    timeout_sec: 1,
  })
  const deadline = Date.now() + NETWORK_PROBE_TIMEOUT_MS
  while (Date.now() < deadline) {
    await delay(NETWORK_POLL_DELAY_MS)
    await coreNetworkStore.refreshState({ force: true })
    const probe = coreNetworkStore.snapshot?.last_address_probe
    if (probe?.request_id === accepted.request_id) {
      return probe
    }
  }
  return coreNetworkStore.snapshot?.last_address_probe ?? null
}

async function requestAddressProbes(interfaceName: string, addresses: string[]): Promise<CoreNetAddressProbeSnapshot | null> {
  const uniqueAddresses = Array.from(new Set(addresses)).filter(Boolean)
  if (!uniqueAddresses.length) return null
  const probes: CoreNetAddressProbeSnapshot[] = []
  for (let index = 0; index < uniqueAddresses.length; index += NETWORK_PROBE_BATCH_SIZE) {
    const batch = uniqueAddresses.slice(index, index + NETWORK_PROBE_BATCH_SIZE)
    const probe = await requestAddressProbe(interfaceName, batch)
    if (probe) probes.push(probe)
  }
  if (!probes.length) return null
  const lastProbe = probes[probes.length - 1]
  return {
    request_id: lastProbe?.request_id ?? null,
    interface: interfaceName,
    checked_at: lastProbe?.checked_at ?? new Date().toISOString(),
    results: probes.flatMap(probe => probe.results),
  }
}

async function waitForAppliedAddress(interfaceName: string, expectedIp: string) {
  const deadline = Date.now() + NETWORK_APPLY_TIMEOUT_MS
  while (Date.now() < deadline) {
    await delay(NETWORK_POLL_DELAY_MS)
    await coreNetworkStore.refreshState({ force: true })
    const iface = coreNetworkStore.interfaces.find(item => item.interface_name === interfaceName)
    if (iface?.local_ip === expectedIp) {
      return
    }
  }
  throw new Error(`Timed out waiting for ${interfaceName} to report ${expectedIp}.`)
}

function buildNetworkSummary(probe: CoreNetAddressProbeSnapshot | null) {
  const reachable: string[] = []
  const notReachable: string[] = []
  const unknown: string[] = []
  const results = probe?.results ?? []
  const resultByAddress = new Map(results.map(result => [result.address, result]))
  for (const ip of networkDeviceIps.value) {
    const result = resultByAddress.get(ip)
    if (!result || result.reachable === null) {
      unknown.push(ip)
    } else if (result.reachable) {
      reachable.push(ip)
    } else {
      notReachable.push(ip)
    }
  }
  const iface = detectedEthernetInterface.value
  const currentConfig = iface
    ? `${iface.interface_name} ${iface.local_ip || "no-ip"}${iface.netmask ? `/${iface.netmask}` : ""}`
    : "RJ45 interface not detected"
  return { reachable, notReachable, unknown, currentConfig }
}

function delay(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms))
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
  () => availableColumns.value.map(column => column.header).join("\u0000"),
  () => {
    const availableHeaders = new Set(availableColumns.value.map(column => column.header))
    if (transportHostColumnName.value && !availableHeaders.has(transportHostColumnName.value)) {
      transportHostColumnName.value = null
    }
    if (iec61850AddressColumnName.value && !availableHeaders.has(iec61850AddressColumnName.value)) {
      iec61850AddressColumnName.value = null
    }
  },
  { immediate: true },
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
  networkSubnets,
  subnets => {
    if (!subnets.length) {
      selectedNetworkCidr.value = null
      occupiedCandidateIps.value = []
      networkSummary.value = null
      return
    }
    if (!selectedNetworkCidr.value || !subnets.some(subnet => subnet.cidr === selectedNetworkCidr.value)) {
      selectedNetworkCidr.value = subnets[0]?.cidr ?? null
      occupiedCandidateIps.value = []
      networkSummary.value = null
    }
  },
  { immediate: true },
)

watch(
  () => selectedNetworkCidr.value,
  () => {
    occupiedCandidateIps.value = []
    networkSummary.value = null
    networkError.value = null
  },
)

watch(
  () => props.open,
  (open) => {
    if (!open) {
      coreNetworkStore.stopMonitoring()
      return
    }
    coreNetworkStore.startMonitoring()
    void signalSheetStore.refreshPresets()
  },
  { immediate: true },
)

watch(
  [() => props.open, () => props.seedFile],
  ([open, seedFile]) => {
    if (!open) {
      lastSeedFileKey.value = ""
      return
    }
    if (!seedFile) {
      return
    }

    const seedKey = `${seedFile.name}:${seedFile.size}:${seedFile.lastModified}`
    if (seedKey === lastSeedFileKey.value) {
      return
    }

    lastSeedFileKey.value = seedKey
    void handleSelectedFile(seedFile)
  },
  { immediate: true },
)
</script>

<style scoped>
.signal-import-modal__form {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 1rem;
  height: auto;
  min-height: 0;
}

.signal-import-modal__body {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 1rem;
  min-height: 0;
  overflow-y: auto;
  overscroll-behavior: contain;
  padding-bottom: 1rem;
  padding-right: 0.25rem;
  scroll-padding-bottom: 1rem;
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
.signal-import-modal__mapping-list,
.signal-import-modal__network-panel,
.signal-import-modal__network-actions,
.signal-import-modal__network-summary {
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
  border-radius: var(--radius-2xl);
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

.signal-import-modal__file-state {
  align-items: center;
  display: flex;
  gap: 0.625rem;
  min-width: 0;
}

.signal-import-modal__spinner,
.signal-import-modal__parsing-spinner {
  animation: signal-import-modal-spin 0.8s linear infinite;
  border: 2px solid var(--color-neutral-200);
  border-radius: var(--radius-pill);
  border-top-color: var(--color-blue-500);
  display: inline-block;
  flex: 0 0 auto;
  height: 1rem;
  width: 1rem;
}

.signal-import-modal__parsing-card {
  align-items: center;
  background: color-mix(in srgb, var(--color-blue-500) 7%, var(--color-white));
  border: 1px solid color-mix(in srgb, var(--color-blue-500) 32%, var(--color-neutral-200));
  border-radius: var(--radius-lg);
  display: grid;
  gap: 0.75rem;
  grid-template-columns: auto minmax(0, 1fr);
  padding: 1rem;
}

.signal-import-modal__parsing-spinner {
  border-width: 3px;
  height: 1.5rem;
  width: 1.5rem;
}

.signal-import-modal__parsing-progress {
  background: color-mix(in srgb, var(--color-blue-500) 14%, transparent);
  border-radius: var(--radius-pill);
  grid-column: 1 / -1;
  height: 0.25rem;
  overflow: hidden;
  width: 100%;
}

.signal-import-modal__parsing-progress span {
  animation: signal-import-modal-progress 1.4s ease-in-out infinite;
  background: var(--color-blue-500);
  border-radius: inherit;
  display: block;
  height: 100%;
  width: 38%;
}

.signal-import-modal__importing-meta {
  color: var(--color-neutral-600);
  display: flex;
  flex-wrap: wrap;
  font-size: var(--text-xs);
  gap: 0.75rem 1rem;
  grid-column: 1 / -1;
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

@keyframes signal-import-modal-spin {
  to { transform: rotate(360deg); }
}

@keyframes signal-import-modal-progress {
  0% { transform: translateX(-110%); }
  50% { transform: translateX(170%); }
  100% { transform: translateX(290%); }
}

.signal-import-modal__empty-state {
  align-items: flex-start;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.signal-import-modal__inline-actions--start {
  justify-content: flex-start;
}

.signal-import-modal__step-content {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 1rem;
  min-height: 0;
}

.signal-import-modal__section-stack {
  flex-direction: column;
  gap: 1rem;
}

.signal-import-modal__section-stack--columns {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
}

.signal-import-modal__columns-panel {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 0.75rem;
  min-height: 0;
}

.signal-import-modal__option-scroll {
  flex: 1 1 auto;
  height: 0;
  min-height: 0;
  overflow-y: auto;
  overscroll-behavior: contain;
  padding-right: 0.25rem;
  scroll-padding-bottom: 1.5rem;
}

.signal-import-modal__warning-text {
  color: var(--color-amber-700);
  font-size: var(--text-xs);
  margin-top: 0.5rem;
}

.signal-import-modal__verification-toggle {
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-xl);
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
  padding: 0.75rem;
}

.signal-import-modal__verification-checkbox-row {
  align-items: center;
  display: flex;
  gap: 0.5rem;
}

.signal-import-modal__verification-checkbox {
  accent-color: var(--color-blue-600);
  height: 1rem;
  width: 1rem;
}

.signal-import-modal__verification-checkbox-label {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 500;
}

.signal-import-modal__verification-grid {
  display: grid;
  gap: 0.75rem;
}

.signal-import-modal__verification-row {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-xl);
  display: grid;
  gap: 0.5rem;
  padding: 0.75rem;
}

.signal-import-modal__verification-label {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 600;
}

.signal-import-modal__verification-value {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}

.signal-import-modal__verification-column {
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
  font-weight: 500;
}

.signal-import-modal__verification-samples {
  display: flex;
  flex-wrap: wrap;
  gap: 0.375rem;
}

.signal-import-modal__verification-sample {
  background: var(--color-neutral-100);
  border-radius: var(--radius-pill);
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  padding: 0.125rem 0.5rem;
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
  padding-bottom: 1.5rem;
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
  border-radius: var(--radius-xl);
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  padding: 0.5rem 0.75rem;
}

.signal-import-modal__network-panel {
  flex-direction: column;
  gap: 1rem;
}

.signal-import-modal__network-grid {
  display: grid;
  gap: 0.5rem;
  grid-template-columns: 1fr;
}

.signal-import-modal__network-fact {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
  min-width: 0;
  padding: 0.625rem 0.75rem;
}

.signal-import-modal__network-label {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
  text-transform: uppercase;
}

.signal-import-modal__network-value {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 600;
  overflow-wrap: anywhere;
}

.signal-import-modal__network-actions {
  flex-wrap: wrap;
  gap: 0.5rem;
}

.signal-import-modal__network-summary {
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  flex-direction: column;
  gap: 0.375rem;
  padding: 0.75rem;
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
:global(.dark .signal-import-modal__network-value),
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
:global(.dark .signal-import-modal__parsing-card),
:global(.dark .signal-import-modal__option),
:global(.dark .signal-import-modal__mapping-row),
:global(.dark .signal-import-modal__network-fact),
:global(.dark .signal-import-modal__input) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-import-modal__parsing-card) {
  background: color-mix(in srgb, var(--color-blue-500) 14%, var(--color-neutral-900));
  border-color: color-mix(in srgb, var(--color-blue-400) 42%, var(--color-neutral-700));
}

:global(.dark .signal-import-modal__spinner),
:global(.dark .signal-import-modal__parsing-spinner) {
  border-color: var(--color-neutral-700);
  border-top-color: var(--color-blue-400);
}

:global(.dark .signal-import-modal__importing-meta) {
  color: var(--color-neutral-400);
}

:global(.dark .signal-import-modal__network-summary) {
  background: color-mix(in srgb, var(--color-neutral-900) 55%, transparent);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-import-modal__network-label) {
  color: var(--color-neutral-400);
}

:global(.dark .signal-import-modal__dropzone--idle:hover) {
  background: var(--color-neutral-800);
  border-color: var(--color-blue-500);
}

:global(.dark .signal-import-modal__verification-toggle) {
  background: color-mix(in srgb, var(--color-neutral-900) 55%, transparent);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-import-modal__verification-checkbox-label),
:global(.dark .signal-import-modal__verification-label),
:global(.dark .signal-import-modal__verification-column) {
  color: var(--color-neutral-100);
}

:global(.dark .signal-import-modal__verification-row) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-import-modal__verification-sample) {
  background: var(--color-neutral-800);
  color: var(--color-neutral-200);
}

:global(.dark .signal-import-modal__option-scroll) {
  scrollbar-color: var(--color-neutral-600) var(--color-neutral-900);
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

  .signal-import-modal__network-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (min-width: 1024px) {
  .signal-import-modal__option-list {
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }
}
</style>
