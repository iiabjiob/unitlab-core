<template>
  <div class="ui-table__zoom" role="group" aria-label="Table zoom controls">
    <button
      type="button"
      class="ui-table__zoom-button"
      :disabled="!canDecrease"
      @click="decrease"
    >
      −
    </button>

    <input
      class="ui-table__zoom-slider"
      type="range"
      :min="minZoom"
      :max="maxZoom"
      :step="stepValue"
      :value="sliderValue"
      aria-label="Adjust table zoom"
      @input="onSliderInput"
    >

    <button
      type="button"
      class="ui-table__zoom-button"
      :disabled="!canIncrease"
      @click="increase"
    >
      +
    </button>

    <button
      type="button"
      class="ui-table__zoom-display"
      @click="openDialog"
    >
      {{ percentLabel }}
    </button>
  </div>

  <teleport to="body">
    <div
      v-if="dialogOpen"
      class="ui-table__zoom-dialog-backdrop"
      @click.self="closeDialog"
    >
      <div class="ui-table__zoom-dialog" role="dialog" aria-modal="true" aria-label="Zoom presets">
        <header class="ui-table__zoom-dialog-header">
          <h3>Zoom level</h3>
        </header>
        <form class="ui-table__zoom-dialog-body" @submit.prevent="applyDialog">
          <fieldset class="ui-table__zoom-dialog-presets">
            <legend>Presets</legend>
            <div class="ui-table__zoom-dialog-preset-grid">
              <label
                v-for="option in presetPercents"
                :key="option"
                class="ui-table__zoom-dialog-option"
              >
                <input
                  v-model="selectedPreset"
                  type="radio"
                  name="zoom-preset"
                  :value="option"
                >
                <span>{{ option }}%</span>
              </label>
            </div>
          </fieldset>

          <div class="ui-table__zoom-dialog-custom">
            <label for="ui-table-custom-zoom">Custom</label>
            <div class="ui-table__zoom-dialog-input">
              <input
                id="ui-table-custom-zoom"
                ref="customInputRef"
                v-model="customPercent"
                type="number"
                inputmode="decimal"
                :min="minPercent"
                :max="maxPercent"
                step="1"
                aria-label="Custom zoom percentage"
              >
              <span class="unit">%</span>
            </div>
          </div>

          <div class="ui-table__zoom-dialog-actions">
            <button type="button" class="ui-table__zoom-dialog-btn ui-table__zoom-dialog-btn--ghost" @click="closeDialog">Cancel</button>
            <button type="submit" class="ui-table__zoom-dialog-btn ui-table__zoom-dialog-btn--primary">Apply</button>
          </div>
        </form>
      </div>
    </div>
  </teleport>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, ref, watch } from "vue"
import { clamp, MAX_ZOOM, MIN_ZOOM, ZOOM_STEP } from "../../core/utils/constants"

const props = withDefaults(defineProps<{
  modelValue: number
  min?: number
  max?: number
  step?: number
}>(), {
  min: MIN_ZOOM,
  max: MAX_ZOOM,
  step: ZOOM_STEP,
})

const emit = defineEmits<{ (e: "update:modelValue", value: number): void }>()

const minZoom = computed(() => Math.max(props.min, 0.05))
const maxZoom = computed(() => Math.max(props.max, minZoom.value))
const stepValue = computed(() => (props.step && props.step > 0 ? props.step : ZOOM_STEP))

const dialogOpen = ref(false)
const selectedPreset = ref<number | null>(null)
const customPercent = ref("")
const customInputRef = ref<HTMLInputElement | null>(null)

const sliderValue = computed(() => Number(clampZoom(props.modelValue).toFixed(3)))
const percentLabel = computed(() => `${Math.round(sliderValue.value * 100)}%`)
const minPercent = computed(() => Math.round(minZoom.value * 100))
const maxPercent = computed(() => Math.round(maxZoom.value * 100))

const presetPercents = computed(() => {
  const base = [minPercent.value, 75, 100, 125, 150, maxPercent.value]
  const filtered = base.filter(value => value >= minPercent.value && value <= maxPercent.value)
  const unique = Array.from(new Set(filtered))
  unique.sort((a, b) => a - b)
  return unique
})

customPercent.value = formatPercent(sliderValue.value)

const canDecrease = computed(() => sliderValue.value > minZoom.value + stepValue.value / 4)
const canIncrease = computed(() => sliderValue.value < maxZoom.value - stepValue.value / 4)

watch(sliderValue, value => {
  if (dialogOpen.value) {
    return
  }
  customPercent.value = formatPercent(value)
})

watch(customPercent, value => {
  if (!dialogOpen.value) return
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) {
    selectedPreset.value = null
    return
  }
  const rounded = Math.round(numeric)
  if (presetPercents.value.includes(rounded)) {
    selectedPreset.value = rounded
  } else {
    selectedPreset.value = null
  }
})

watch(selectedPreset, value => {
  if (!dialogOpen.value) return
  if (value == null) return
  customPercent.value = value.toString()
})

watch(dialogOpen, open => {
  if (open) {
    window.addEventListener("keydown", onDialogKeydown)
    customPercent.value = formatPercent(sliderValue.value)
    selectedPreset.value = findClosestPreset(Number(customPercent.value))
    nextTick(() => {
      customInputRef.value?.focus()
      customInputRef.value?.select()
    })
  } else {
    window.removeEventListener("keydown", onDialogKeydown)
  }
})

onBeforeUnmount(() => {
  window.removeEventListener("keydown", onDialogKeydown)
})

function clampZoom(value: number) {
  return clamp(value, minZoom.value, maxZoom.value)
}

function snapZoom(value: number) {
  const step = stepValue.value
  if (!step) return value
  return Math.round(value / step) * step
}

function updateZoom(value: number) {
  const snapped = snapZoom(clampZoom(value))
  emit("update:modelValue", Number(snapped.toFixed(3)))
}

function decrease() {
  if (!canDecrease.value) return
  updateZoom(sliderValue.value - stepValue.value)
}

function increase() {
  if (!canIncrease.value) return
  updateZoom(sliderValue.value + stepValue.value)
}

function onSliderInput(event: Event) {
  const target = event.target as HTMLInputElement
  const numeric = Number(target.value)
  if (!Number.isFinite(numeric)) return
  updateZoom(numeric)
}

function openDialog() {
  dialogOpen.value = true
}

function closeDialog() {
  dialogOpen.value = false
}

function findClosestPreset(currentPercent: number | null): number | null {
  if (currentPercent == null || !Number.isFinite(currentPercent)) return null
  const rounded = Math.round(currentPercent)
  return presetPercents.value.includes(rounded) ? rounded : null
}

function formatPercent(value: number) {
  return Math.round(value * 100).toString()
}

function applyDialog() {
  const numericPercent = Number(customPercent.value)
  if (!Number.isFinite(numericPercent)) {
    return
  }
  const clampedPercent = clamp(numericPercent, minPercent.value, maxPercent.value)
  updateZoom(clampedPercent / 100)
  closeDialog()
}

function onDialogKeydown(event: KeyboardEvent) {
  if (event.key === "Escape") {
    closeDialog()
  }
}
</script>
