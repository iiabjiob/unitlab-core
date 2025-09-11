<template>
  <div
    class="selectable-row flex items-center w-full border-b border-neutral-300 dark:border-neutral-700 last:border-0 text-xs"
    :class="selected ? 'selectable-row--selected' : 'selectable-row--idle'"
    @click.stop="$emit('select', channel)"
  >
    <!-- Имя канала -->
    <span>{{ channel.name || ("CH" + (channel.index + 1)) }}</span>

    <!-- Управление DO -->
    <div v-if="type === 'DO'" class="flex flex-1 items-center justify-end gap-2">
      <ButtonComponent
        size="xs"
        type="secondary"
        class="min-w-[40px]"
        :disabled="Boolean(!channel.state)"
        @click.stop="$emit('toggle', false)"
      >
        Off
      </ButtonComponent>
      <ButtonComponent
        size="xs"
        type="secondary"
        class="min-w-[40px]"
        :disabled="Boolean(channel.state)"
        @click.stop="$emit('toggle', true)"
      >
        On
      </ButtonComponent>
    </div>

    <!-- AO -->
    <div v-else-if="type === 'AO'" class="flex flex-1 items-center justify-end gap-2">
      <div class="flex items-center gap-1">
        <input
          type="number"
          min="0"
          max="24"
          step="0.01"
          v-model="inputValue"
          @input="onInput"
          @blur="onBlur"
          @keyup.enter="onConfirm"
          class="w-20 text-center px-1 border border-neutral-300 dark:border-neutral-700 rounded text-xs bg-neutral-100 dark:bg-neutral-900 p-0.5"
        />
        <span class="text-neutral-400 text-xs">mA</span>
      </div>
      <ButtonComponent size="xs" type="primary" @click.stop="onConfirm">
        Set
      </ButtonComponent>
    </div>

    <!-- DI -->
    <div v-else-if="type === 'DI'" class="flex flex-1 items-center justify-end gap-2">
      <!-- Только статус -->
    </div>

    <!-- Статус -->
    <span class="ml-4 pl-4 border-l border-neutral-300 dark:border-neutral-700 text-right">
      <template v-if="type === 'DO' || type === 'DI'">
        {{ channel.state ? "🟢" : "⚪️" }}
      </template>
      <template v-else-if="type === 'AO'">
        <div class="min-w-[50px] text-nowrap">
          {{ channel.state }}
          <span class="text-xs text-neutral-400 ml-1">mA</span>
        </div>
      </template>
      <template v-else>n/a</template>
    </span>
  </div>
</template>

<script setup lang="ts">
import type { Channel, ChannelType } from "@/types/channel"
import ButtonComponent from "../ui/ButtonComponent.vue"
import { ref } from "vue"

const props = defineProps<{
  type: ChannelType
  channel: Channel
  selected?: boolean
  disabled?: boolean
}>()

const emit = defineEmits(["toggle", "ao-change", "select"])

const inputValue = ref(
  typeof props.channel.state === "number" ? formatValue(props.channel.state) : "4.00"
)

function onInput(e: Event) {
  const target = e.target as HTMLInputElement
  let val = parseFloat(target.value)

  if (isNaN(val)) {
    inputValue.value = "0.00"
    return
  }
  if (val < 0) val = 0
  if (val > 24) val = 24
  inputValue.value = target.value
}

function onBlur() {
  let val = parseFloat(inputValue.value)
  if (isNaN(val)) val = 0
  if (val < 0) val = 0
  if (val > 24) val = 24
  inputValue.value = formatValue(val)
}

function onConfirm() {
  let val = parseFloat(inputValue.value)
  if (isNaN(val)) val = 0
  if (val < 0) val = 0
  if (val > 24) val = 24
  emit("ao-change", val)
  inputValue.value = formatValue(val)
}

function formatValue(num: number): string {
  return num.toFixed(2)
}
</script>
