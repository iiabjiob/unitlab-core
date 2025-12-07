<template>
  <label
    v-if="channel.type === 'do'"
    class="do-toggle"
    :class="{ 'do-toggle--on': isActive }"
    :title="tooltip"
  >
    <input
      type="checkbox"
      class="sr-only"
      :checked="isActive"
      @change.stop="handleToggle"
    />
    <span class="do-toggle__box">
      <span class="do-toggle__indicator"></span>
    </span>
    <span class="do-toggle__label" aria-hidden="true">{{ label }}</span>
    <span class="sr-only">{{ tooltip }}</span>
  </label>

  <span
    v-else-if="channel.type === 'di'"
    class="circuit"
    :class="{ 'circuit--on': isActive }"
    role="status"
    :title="tooltip"
  >
    <span class="circuit__wire"></span>
    <span class="circuit__node circuit__node--in"></span>
    <span class="circuit__node circuit__node--out"></span>
    <span class="circuit__spark"></span>
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

function handleToggle() {
  emit("toggle", !isActive.value)
}
</script>

<style scoped>
.do-toggle {
  display: inline-flex;
  align-items: center;
  gap: 0.35rem;
  padding: 0.35rem 0.6rem;
  border-radius: 9999px;
  border: 1px solid rgba(100, 116, 139, 0.4);
  background: rgba(248, 250, 252, 0.7);
  cursor: pointer;
  transition: border-color 150ms ease, background 150ms ease, box-shadow 150ms ease;
  font-size: 0.65rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
}

.do-toggle__box {
  width: 1rem;
  height: 1rem;
  border-radius: 0.3rem;
  border: 1px solid rgba(100, 116, 139, 0.5);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  background: #fff;
  transition: border-color 150ms ease, background 150ms ease;
}

.do-toggle__indicator {
  width: 0.55rem;
  height: 0.55rem;
  border-radius: 0.2rem;
  background: rgba(148, 163, 184, 0.4);
  transition: background 150ms ease, transform 150ms ease;
}

.do-toggle--on {
  border-color: rgba(34, 197, 94, 0.6);
  box-shadow: 0 0 12px rgba(34, 197, 94, 0.25);
  background: rgba(240, 253, 244, 0.85);
}

.do-toggle--on .do-toggle__box {
  border-color: rgba(34, 197, 94, 0.8);
  background: #ecfdf5;
}

.do-toggle--on .do-toggle__indicator {
  background: #22c55e;
  transform: scale(1.05);
}

.do-toggle__label {
  font-weight: 600;
  color: #0f172a;
}

.circuit {
  position: relative;
  width: 1.8rem;
  height: 1.2rem;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 0.75rem;
  background: linear-gradient(90deg, rgba(15, 23, 42, 0.2), rgba(15, 23, 42, 0.05));
  border: 1px solid rgba(100, 116, 139, 0.4);
  overflow: hidden;
  transition: border-color 150ms ease, background 150ms ease;
}

.circuit__wire {
  position: absolute;
  width: 80%;
  height: 2px;
  background: rgba(148, 163, 184, 0.8);
}

.circuit__node {
  position: absolute;
  width: 0.4rem;
  height: 0.4rem;
  border-radius: 9999px;
  background: rgba(148, 163, 184, 0.85);
}

.circuit__node--in {
  left: 0.2rem;
}

.circuit__node--out {
  right: 0.2rem;
}

.circuit__spark {
  position: absolute;
  width: 0.3rem;
  height: 0.3rem;
  border-radius: 9999px;
  background: rgba(248, 250, 252, 0.9);
  opacity: 0;
  box-shadow: 0 0 8px rgba(248, 250, 252, 0.9);
  transition: transform 200ms ease, opacity 200ms ease;
}

.circuit--on {
  border-color: rgba(250, 204, 21, 0.85);
  background: radial-gradient(circle at 50% 50%, rgba(250, 204, 21, 0.4), rgba(15, 23, 42, 0.05));
}

.circuit--on .circuit__wire {
  background: rgba(250, 204, 21, 0.95);
}

.circuit--on .circuit__node {
  background: rgba(250, 204, 21, 0.95);
  box-shadow: 0 0 8px rgba(250, 204, 21, 0.7);
}

.circuit--on .circuit__spark {
  opacity: 1;
  transform: scale(1.4);
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

:global(.dark) .do-toggle {
  background: rgba(15, 23, 42, 0.35);
  border-color: rgba(148, 163, 184, 0.4);
  color: #e2e8f0;
}

:global(.dark) .do-toggle__box {
  background: rgba(15, 23, 42, 0.7);
  border-color: rgba(148, 163, 184, 0.5);
}

:global(.dark) .do-toggle__indicator {
  background: rgba(148, 163, 184, 0.55);
}

:global(.dark) .do-toggle--on {
  background: rgba(22, 101, 52, 0.35);
  border-color: rgba(74, 222, 128, 0.7);
}

:global(.dark) .do-toggle--on .do-toggle__indicator {
  background: #4ade80;
}

:global(.dark) .circuit {
  border-color: rgba(71, 85, 105, 0.7);
  background: linear-gradient(90deg, rgba(15, 23, 42, 0.6), rgba(15, 23, 42, 0.2));
}

:global(.dark) .circuit__wire {
  background: rgba(148, 163, 184, 0.65);
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
