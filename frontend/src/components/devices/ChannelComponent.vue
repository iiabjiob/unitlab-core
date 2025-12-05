<template>
  <button
    v-if="channel.type === 'do'"
    type="button"
    class="lamp lamp--do"
    :class="lampStateClass"
    :aria-pressed="isActive"
    :title="tooltip"
    @click.stop="emit('toggle', !isActive)"
  >
    <span class="lamp__halo"></span>
    <span class="lamp__core"></span>
    <span class="sr-only">{{ tooltip }}</span>
  </button>

  <span
    v-else-if="channel.type === 'di'"
    class="lamp lamp--di"
    :class="lampStateClass"
    role="status"
    :title="tooltip"
  >
    <span class="lamp__halo"></span>
    <span class="lamp__core"></span>
    <span class="sr-only">{{ tooltip }}</span>
  </span>

  <label
    v-else-if="channel.type === 'ao'"
    class="ao-control"
    :title="tooltip"
  >
    <span class="ao-control__label">AO</span>
    <input
      type="number"
      min="0"
      max="24"
      step="0.01"
      v-model="inputValue"
      @blur="commit"
      @keyup.enter.prevent="commit"
      class="ao-control__input"
    />
    <span class="ao-control__unit">mA</span>
  </label>

  <span v-else class="unknown-chip" :title="tooltip">?</span>
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { Channel } from "@/types/channel"
import { useChannelStore } from "@/stores/channelStore"
import { formatAoValue, parseAoInput } from "@/utils/channel"

const channelStore = useChannelStore()

const props = defineProps<{
  channel: Channel
}>()

const emit = defineEmits(["toggle", "ao-change"])

const label = computed(() => channelStore.resolveChannelLabel(props.channel))
const tooltip = computed(() => `${label.value} · #${props.channel.index + 1}`)
const isActive = computed(() => Boolean(props.channel.state))

const lampStateClass = computed(() => (isActive.value ? "lamp--on" : "lamp--off"))

const inputValue = ref(
  typeof props.channel.state === "number"
    ? formatAoValue(props.channel.state)
    : "4.00"
)

watch(
  () => props.channel.state,
  value => {
    if (typeof value === "number") {
      inputValue.value = formatAoValue(value)
    }
  }
)

function commit() {
  const num = parseAoInput(inputValue.value)
  emit("ao-change", num)
  inputValue.value = formatAoValue(num)
}
</script>

<style scoped>
.lamp {
  position: relative;
  width: 1.2rem;
  height: 1.2rem;
  border-radius: 9999px;
  border: 1px solid rgba(51, 65, 85, 0.35);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  transition: transform 120ms ease, border-color 120ms ease;
}

.lamp--do {
  cursor: pointer;
  touch-action: manipulation;
}

.lamp--do:active {
  transform: translateY(1px);
}

.lamp__halo,
.lamp__core {
  border-radius: inherit;
  position: absolute;
}

.lamp__halo {
  width: 120%;
  height: 120%;
  filter: blur(6px);
  opacity: 0;
  transition: opacity 200ms ease;
}

.lamp__core {
  width: 100%;
  height: 100%;
  background: linear-gradient(145deg, #e2e8f0, #cbd5f5);
  box-shadow: inset 0 2px 4px rgba(15, 23, 42, 0.2);
  transition: background 150ms ease;
}

.lamp--on {
  border-color: rgba(74, 222, 128, 0.7);
}

.lamp--on .lamp__core {
  background: radial-gradient(circle at 30% 30%, #bbf7d0, #22c55e);
  box-shadow: inset 0 2px 4px rgba(20, 83, 45, 0.4);
}

.lamp--on .lamp__halo {
  background: radial-gradient(circle, rgba(74, 222, 128, 0.35), transparent 70%);
  opacity: 1;
}

.lamp--off .lamp__core {
  background: radial-gradient(circle at 30% 30%, #f1f5f9, #cbd5f5);
}

.lamp--di {
  cursor: default;
  border-style: dashed;
}

.ao-control {
  align-items: center;
  border: 1px solid rgba(100, 116, 139, 0.4);
  border-radius: 9999px;
  color: #0f172a;
  display: inline-flex;
  gap: 0.25rem;
  padding: 0.05rem 0.4rem;
  font-size: 0.75rem;
}

.ao-control__label {
  font-size: 0.6rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: #64748b;
}

.ao-control__input {
  width: 3rem;
  background: transparent;
  border: none;
  text-align: right;
  font-size: 0.75rem;
  color: inherit;
}

.ao-control__input:focus {
  outline: none;
}

.ao-control__unit {
  font-size: 0.6rem;
  color: #94a3b8;
}

.unknown-chip {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 1.4rem;
  height: 1.4rem;
  border: 1px dashed rgba(148, 163, 184, 0.5);
  border-radius: 0.4rem;
}

:global(.dark) .lamp {
  border-color: rgba(148, 163, 184, 0.4);
}

:global(.dark) .lamp__core {
  background: radial-gradient(circle at 30% 30%, #1e293b, #0f172a);
  box-shadow: inset 0 2px 4px rgba(15, 23, 42, 0.6);
}

:global(.dark) .lamp--on .lamp__core {
  background: radial-gradient(circle at 30% 30%, #86efac, #16a34a);
}

:global(.dark) .ao-control {
  border-color: rgba(148, 163, 184, 0.35);
  color: #e2e8f0;
}

:global(.dark) .ao-control__label {
  color: #cbd5f5;
}

:global(.dark) .ao-control__unit {
  color: #94a3b8;
}
</style>
