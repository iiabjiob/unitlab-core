<template>
  <UiMenu>
    <UiMenuTrigger as-child trigger="contextmenu">
      <div
        class="device-channel-item"
        :class="{ 'device-channel-item--disabled': disabled }"
      >
        <div v-if="effectiveType === 'ao'" class="device-channel-item__ao">
          <div class="device-channel-item__ao-grid">

            <div class="device-channel-item__label device-channel-item__label--ao">
              <span class="device-channel-item__primary-label">{{ primaryChannelLabel }}</span>
              <span v-if="secondaryChannelLabel" class="device-channel-item__secondary-label">{{ secondaryChannelLabel }}</span>
            </div>

            <div class="device-channel-item__ao-controls">
              <div class="device-channel-item__ao-inline">
                <label
                  class="device-channel-item__ao-field"
                  :for="`device-channel-ao-${channel.id}`"
                >
                  <input
                    ref="aoInputRef"
                    :id="`device-channel-ao-${channel.id}`"
                    :name="`device-channel-ao-${channel.id}`"
                    v-model="aoDraftValue"
                    type="number"
                    inputmode="decimal"
                    min="4"
                    max="20"
                    step="0.1"
                    autocomplete="off"
                    class="device-channel-item__ao-input"
                    :disabled="disabled"
                    @keydown.enter.prevent="submitAoValue"
                  />
                  <span class="device-channel-item__ao-unit">mA</span>
                </label>
                <UiButton
                  type="button"
                  size="xs"
                  variant="secondary"
                  class="device-channel-item__ao-button"
                  :disabled="!aoCanSubmit"
                  :title="aoSetButtonTitle"
                  @click.stop="submitAoValue"
                >
                  {{ aoSetButtonLabel }}
                </UiButton>

                <div class="device-channel-item__ao-actual">
                  <span class="device-channel-item__ao-actual-label">Value</span>
                  <span class="device-channel-item__ao-actual-value">{{ aoActualValueLabel }}</span>
                  <span class="device-channel-item__ao-actual-unit">mA</span>
                </div>

                <span
                  v-if="aoStatusClass"
                  class="device-channel-item__ao-status"
                  :class="aoStatusClass"
                  :title="aoStatusTitle"
                >
                  {{ aoStatusLabel }}
                </span>
              </div>
            </div>
          </div>
        </div>

        <div v-else class="device-channel-item__digital">

          <div
            v-if="effectiveType === 'do'"
            class="device-channel-item__do-control"
            :class="[doControlClass, { 'device-channel-item__do-control--disabled': isWaiting || disabled }]"
            @click.stop="onToggleClick"
          >
            <span
              v-if="isWaiting"
              class="device-channel-item__spinner"
            />
            <span v-else-if="isError" class="device-channel-item__state-mark">!</span>
            <span v-else-if="channel.state" class="device-channel-item__state-mark">✓</span>
          </div>

          <div
            v-else-if="effectiveType === 'di'"
            class="device-channel-item__di-indicator"
            :class="diIndicatorClass"
          />

          <div class="device-channel-item__label device-channel-item__label--digital">
            <span class="device-channel-item__primary-label">{{ primaryChannelLabel }}</span>
            <span v-if="secondaryChannelLabel" class="device-channel-item__secondary-label">{{ secondaryChannelLabel }}</span>
          </div>
        </div>
      </div>
    </UiMenuTrigger>

    <UiMenuContent>
      <UiMenuItem class="device-channel-item__menu-item" @select="openRename">
        Rename
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>

  <RenameModal
    :open="renameOpen"
    title="Rename channel"
    label="Name"
    v-model="renameValue"
    :loading="renaming"
    :error="renameError"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />
</template>


<script setup lang="ts">
import { computed, ref, watch } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import { useChannelStore } from "@/stores/channelStore"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"
import type { AoChannel, Channel, DiChannel, DoChannel } from "@/types/channel"
import { formatAoValue } from "@/utils/channel"

const props = withDefaults(defineProps<{
  channel: Channel
  disabled?: boolean
  deviceType?: "do" | "di" | "ao"
}>(), {
  disabled: false,
  deviceType: undefined,
})

const emit = defineEmits<{
  (e: "toggle", ch: Channel): void
  (e: "set-ao", payload: { channel: Channel; value: number }): void
}>()

const channelStore = useChannelStore()
const renameOpen = ref(false)
const renameValue = ref(props.channel.name ?? "")
const renaming = ref(false)
const renameError = ref("")

const effectiveType = computed<Channel["type"] | null>(() => {
  const channelType = props.channel.type
  const expectedType = props.deviceType
  if (!expectedType) {
    return channelType
  }
  return channelType === expectedType ? channelType : null
})

const status = computed(() =>
  doChannel.value?.ui?.stage ?? "idle"
)
const isWaiting = computed(() => status.value === "pending" || status.value === "debounce")
const isError = computed(() => status.value === "error")

const doChannel = computed<DoChannel | null>(() => (
  effectiveType.value === "do" && props.channel.type === "do" ? props.channel : null
))

const diChannel = computed<DiChannel | null>(() => (
  effectiveType.value === "di" && props.channel.type === "di" ? props.channel : null
))

const aoChannel = computed<AoChannel | null>(() => (
  effectiveType.value === "ao" && props.channel.type === "ao" ? props.channel : null
))

const aoInputRef = ref<HTMLInputElement | null>(null)
const aoDraftValue = ref("")

const fallbackChannelLabel = computed(() => `CH${props.channel.index + 1}`)
const primaryChannelLabel = computed(() => {
  const explicitName = String(props.channel.name ?? "").trim()
  if (explicitName) {
    return explicitName
  }
  const resolved = String(props.channel.resolved_name ?? "").trim()
  return resolved || fallbackChannelLabel.value
})

const secondaryChannelLabel = computed(() => {
  const explicitName = String(props.channel.name ?? "").trim()
  if (!explicitName) {
    return ""
  }
  return fallbackChannelLabel.value
})

watch(
  () => [aoChannel.value?.id ?? null, aoChannel.value?.state ?? null] as const,
  ([, state]) => {
    if (typeof state === "number" && Number.isFinite(state)) {
      aoDraftValue.value = formatAoValue(state)
      return
    }
    aoDraftValue.value = ""
  },
  { immediate: true }
)

watch(
  () => props.channel.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value ?? ""
    }
  },
)

const diAlertActive = computed(() => {
  const diag = diChannel.value?.diDiagnostics
  if (!diag) {
    return false
  }
  return Boolean(diag.stuck || diag.lost)
})

const diIndicatorClass = computed(() => {
  if (effectiveType.value !== "di") {
    return ""
  }
  if (diAlertActive.value) {
    return "device-channel-item__di-indicator--alert"
  }
  return props.channel.state
    ? "device-channel-item__di-indicator--on"
    : "device-channel-item__di-indicator--off"
})

const doControlClass = computed(() => {
  if (effectiveType.value !== "do") {
    return ""
  }
  if (isError.value) {
    return "device-channel-item__do-control--error"
  }
  if (isWaiting.value) {
    return "device-channel-item__do-control--waiting"
  }
  if (doChannel.value?.state) {
    return "device-channel-item__do-control--on"
  }
  return "device-channel-item__do-control--off"
})

const aoStatus = computed(() => aoChannel.value?.diagnostics?.quality)
const aoHasError = computed(() => Boolean(aoChannel.value?.diagnostics?.hasError))
const aoActualValueLabel = computed(() => {
  if (typeof aoChannel.value?.state !== "number" || !Number.isFinite(aoChannel.value.state)) {
    return "—"
  }
  return formatAoValue(aoChannel.value.state)
})

const aoDraftNumber = computed<number | null>(() => {
  const raw = aoDraftValue.value.trim()
  if (!raw) return null
  const parsed = Number.parseFloat(raw)
  if (!Number.isFinite(parsed)) return null
  return parsed
})

const aoDraftChanged = computed(() => {
  if (aoDraftNumber.value == null || aoChannel.value == null) return false
  return formatAoValue(aoDraftNumber.value) !== formatAoValue(aoChannel.value.state)
})

const aoCanSubmit = computed(() => !props.disabled && aoDraftNumber.value !== null)

const aoSetButtonLabel = computed(() => {
  if (aoStatus.value === "pending") return "Wait"
  if (aoHasError.value) return "Retry"
  return "Set"
})

const aoSetButtonTitle = computed(() => {
  if (props.disabled) return "Device is offline"
  if (!aoDraftValue.value.trim()) return "Enter a numeric AO value"
  if (aoDraftNumber.value == null) return "Value must be numeric"
  if (!aoDraftChanged.value) return `Send ${formatAoValue(aoDraftNumber.value)} mA again`
  return `Send ${formatAoValue(aoDraftNumber.value)} mA to device`
})

const aoStatusLabel = computed(() => {
  if (aoStatus.value === "valid") return "OK"
  if (aoStatus.value === "pending") return "PEND"
  if (aoStatus.value === "fault") return "FAULT"
  return ""
})

const aoStatusClass = computed(() => {
  if (!aoStatus.value) {
    return ""
  }
  if (aoStatus.value === "valid") {
    return aoHasError.value
      ? "device-channel-item__ao-status--valid-error"
      : "device-channel-item__ao-status--valid"
  }
  if (aoStatus.value === "pending") {
    return "device-channel-item__ao-status--pending"
  }
  return "device-channel-item__ao-status--fault"
})

const aoStatusTitle = computed(() => {
  if (!aoStatus.value) {
    return "AO diagnostics unavailable"
  }
  if (aoHasError.value) {
    return `AO ${aoStatus.value.toUpperCase()} (backend error latched)`
  }
  return `AO ${aoStatus.value.toUpperCase()}`
})

function onToggleClick() {
  if (effectiveType.value !== "do" || isWaiting.value || props.disabled) {
    return
  }
  emit("toggle", props.channel)
}

function openRename() {
  renameValue.value = props.channel.name ?? ""
  renameError.value = ""
  renameOpen.value = true
}

function cancelRename() {
  if (renaming.value) return
  renameOpen.value = false
  renameError.value = ""
  renameValue.value = props.channel.name ?? ""
}

async function confirmRename() {
  if (renaming.value) return

  const trimmed = renameValue.value.trim()
  const nextName = trimmed.length > 0 ? trimmed : null
  const currentName = String(props.channel.name ?? "").trim() || null

  if (nextName === currentName) {
    renameOpen.value = false
    return
  }

  renameError.value = ""
  renaming.value = true

  try {
    await channelStore.updateChannelField(props.channel.id, { name: nextName })
    renameOpen.value = false
  } catch (error) {
    renameError.value = error instanceof Error ? error.message : "Failed to rename channel"
  } finally {
    renaming.value = false
  }
}

function submitAoValue() {
  const raw = aoInputRef.value?.value?.trim() ?? aoDraftValue.value.trim()
  if (!raw) {
    return
  }
  const exactValue = Number.parseFloat(raw)
  if (!Number.isFinite(exactValue) || props.disabled) {
    return
  }
  aoDraftValue.value = raw
  emit("set-ao", { channel: props.channel, value: exactValue })
}
</script>

<style scoped>
.device-channel-item {
  padding: 0.375rem 0.5rem;
  border-radius: var(--radius-md);
  font-size: var(--text-sm);
  user-select: none;
}

.device-channel-item--disabled {
  opacity: 0.6;
}

.device-channel-item__ao {
  display: flex;
  justify-content: center;
}

.device-channel-item__ao-grid {
  display: grid;
  width: 100%;
  max-width: 40rem;
  min-width: 0;
  grid-template-columns: 9rem minmax(0, 1fr);
  align-items: start;
  gap: 0.75rem;
}

.device-channel-item__label {
  min-width: 0;
  line-height: 1.25;
}

.device-channel-item__label--ao {
  display: grid;
  gap: 0.125rem;
  text-align: center;
}

.device-channel-item__label--digital {
  grid-column: 2;
  grid-row: 1;
}

.device-channel-item__primary-label,
.device-channel-item__secondary-label {
  display: block;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.device-channel-item__primary-label {
  color: var(--color-neutral-800);
  font-size: var(--text-xs);
  font-weight: 600;
}

.device-channel-item__secondary-label {
  color: var(--color-neutral-500);
  font-size: 0.625rem;
}

.device-channel-item__ao-controls {
  display: flex;
  min-width: 0;
  flex: 1 1 auto;
  align-items: center;
  gap: 0.5rem;
}

.device-channel-item__ao-inline {
  display: flex;
  min-width: 0;
  align-items: center;
  gap: 0.5rem;
}

.device-channel-item__ao-field {
  display: flex;
  min-width: 0;
  flex: 1 1 auto;
  align-items: center;
  gap: 0.375rem;
  color: var(--color-neutral-500);
  font-size: 0.625rem;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.device-channel-item__ao-input {
  width: 6rem;
  min-width: 0;
  flex: 1 1 auto;
  padding: 0.25rem 0.5rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-sm);
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-xs);
  font-weight: 600;
  text-align: right;
  outline: none;
  transition: border-color 150ms ease, background 150ms ease;
}

.device-channel-item__ao-input:focus {
  border-color: var(--color-sky-500);
}

.device-channel-item__ao-unit,
.device-channel-item__ao-actual-label,
.device-channel-item__ao-actual-unit {
  flex-shrink: 0;
  color: var(--color-neutral-400);
}

.device-channel-item .device-channel-item__ao-button {
  height: 1.25rem;
  flex-shrink: 0;
  padding-right: 0.375rem;
  padding-left: 0.375rem;
  font-size: 0.625rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.device-channel-item__ao-actual {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
  padding: 0.125rem;
}

.device-channel-item__ao-actual-label,
.device-channel-item__ao-actual-unit {
  font-size: 0.5625rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.device-channel-item__ao-actual-value {
  color: var(--color-neutral-600);
  font-family: var(--font-mono);
  font-size: 0.625rem;
}

.device-channel-item__ao-status {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.375rem;
  border: 1px solid currentColor;
  border-radius: var(--radius-sm);
  font-size: 0.625rem;
  font-weight: 500;
}

.device-channel-item__ao-status--valid-error {
  border-color: var(--color-amber-500);
  background: color-mix(in srgb, var(--color-amber-900) 40%, transparent);
  color: color-mix(in srgb, var(--color-amber-300) 70%, var(--color-white));
}

.device-channel-item__ao-status--valid {
  border-color: var(--color-emerald-500);
  background: color-mix(in srgb, var(--color-emerald-900) 40%, transparent);
  color: color-mix(in srgb, var(--color-emerald-300) 70%, var(--color-white));
}

.device-channel-item__ao-status--pending {
  border-color: var(--color-yellow-400);
  background: color-mix(in srgb, var(--color-yellow-900) 50%, transparent);
  color: var(--color-yellow-100);
  animation: device-channel-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
}

.device-channel-item__ao-status--fault {
  border-color: var(--color-red-500);
  background: color-mix(in srgb, var(--color-red-900) 50%, transparent);
  color: var(--color-red-100);
  animation: device-channel-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
}

.device-channel-item__digital {
  display: grid;
  min-width: 0;
  grid-template-columns: 1.25rem minmax(0, 1fr);
  align-items: start;
  column-gap: 0.75rem;
  row-gap: 0.125rem;
}

.device-channel-item__do-control {
  display: flex;
  width: 1.25rem;
  height: 1.25rem;
  grid-column: 1;
  grid-row: 1;
  align-items: center;
  justify-content: center;
  margin-top: 0.125rem;
  border: 1px solid transparent;
  border-radius: var(--radius-sm);
  cursor: default;
  transition: background 150ms ease, border-color 150ms ease, color 150ms ease, opacity 150ms ease;
}

.device-channel-item__do-control--disabled {
  cursor: not-allowed;
  opacity: 0.7;
}

.device-channel-item__do-control--error {
  border-color: var(--color-red-500);
  background: var(--color-red-500);
  color: var(--color-white);
  animation: device-channel-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
}

.device-channel-item__do-control--waiting {
  border-color: var(--color-yellow-400);
  background: color-mix(in srgb, var(--color-yellow-400) 80%, transparent);
  color: var(--color-yellow-900);
}

.device-channel-item__do-control--on {
  border-color: var(--color-green-600);
  background: var(--color-green-400);
  color: var(--color-white);
}

.device-channel-item__do-control--off {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-300);
}

.device-channel-item__do-control--off:hover {
  background: var(--color-neutral-600);
}

.device-channel-item__spinner {
  width: 1rem;
  height: 1rem;
  border: 2px solid color-mix(in srgb, var(--color-white) 85%, transparent);
  border-top-color: transparent;
  border-radius: 999px;
  animation: device-channel-spin 1s linear infinite;
}

.device-channel-item__state-mark {
  color: var(--color-white);
  font-size: 0.6875rem;
  font-weight: 600;
  line-height: 1;
}

.device-channel-item__di-indicator {
  width: 1rem;
  height: 1rem;
  grid-column: 1;
  grid-row: 1;
  margin-top: 0.125rem;
  border: 1px solid transparent;
  border-radius: 999px;
  transition: background 200ms ease, border-color 200ms ease, opacity 200ms ease;
}

.device-channel-item__di-indicator--alert {
  border-color: var(--color-red-500);
  background: var(--color-red-500);
  animation: device-channel-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
}

.device-channel-item__di-indicator--on {
  border-color: var(--color-green-600);
  background: var(--color-green-400);
}

.device-channel-item__di-indicator--off {
  border-color: var(--color-neutral-500);
  background: var(--color-neutral-600);
}

:global(.device-channel-item__menu-item) {
  color: var(--color-neutral-900);
}

:global(.dark .device-channel-item__primary-label) {
  color: var(--color-neutral-100);
}

:global(.dark .device-channel-item__secondary-label),
:global(.dark .device-channel-item__ao-field) {
  color: var(--color-neutral-400);
}

:global(.dark .device-channel-item__ao-input) {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-900);
  color: var(--color-neutral-100);
}

:global(.dark .device-channel-item__ao-unit),
:global(.dark .device-channel-item__ao-actual-label),
:global(.dark .device-channel-item__ao-actual-unit) {
  color: var(--color-neutral-500);
}

:global(.dark .device-channel-item__ao-actual-value) {
  color: var(--color-neutral-300);
}

:global(.dark .device-channel-item__do-control--off) {
  background: var(--color-neutral-700);
}

:global(.dark .device-channel-item__menu-item) {
  color: var(--color-neutral-100);
}

@keyframes device-channel-spin {
  to {
    transform: rotate(360deg);
  }
}

@keyframes device-channel-pulse {
  50% {
    opacity: 0.5;
  }
}
</style>
