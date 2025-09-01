<template>
  <div class="flex items-center px-2 py-1 w-full border-b border-gray-300 dark:border-gray-700 last:border-0 text-xs">
    <!-- Имя канала -->
    <span>{{ channel.name || ("CH" + (channel.index + 1)) }}</span>

    <!-- Управление DO -->
    <div v-if="channel.type === 'DO'" class="flex flex-1 items-center justify-end gap-2">
      <ButtonComponent size="xs" type="secondary" class="min-w-[40px]" :disabled="!channel.state"
        @click="$emit('toggle', false)">
        Off
      </ButtonComponent>
      <ButtonComponent size="xs" type="secondary" class="min-w-[40px]" :disabled="channel.state"
        @click="$emit('toggle', true)">
        On
      </ButtonComponent>
    </div>

    <!-- AO -->
    <div v-else-if="channel.type === 'AO'" class="flex flex-1 items-center justify-end gap-2">
      <div class="flex items-center gap-1">
        <input type="number" min="0" max="24" step="0.01" v-model="inputValue" @input="onInput" @blur="onBlur"
          @keyup.enter="onConfirm"
          class="w-20 text-center px-1 border border-gray-300 dark:border-gray-700 rounded text-xs bg-gray-100 dark:bg-gray-900 p-0.5" />
        <span class="text-gray-400 text-xs">mA</span>
      </div>
      <ButtonComponent size="xs" type="primary" @click="onConfirm">
        Set
      </ButtonComponent>
    </div>


    <!-- DI -->
    <div v-else-if="channel.type === 'DI'" class="flex flex-1 items-center justify-end gap-2">
      <!-- Только статус -->
    </div>

    <!-- Статус -->
    <span class="ml-4 pl-4 border-l border-gray-300 dark:border-gray-700 text-right">
      <template v-if="channel.type === 'DO' || channel.type === 'DI'">
        {{ channel.state ? "🟢" : "⚪️" }}
      </template>
      <template v-else-if="channel.type === 'AO'">
        <div class="min-w-[50px] text-nowrap">
          {{ channel.state }}
          <span class="text-xs text-gray-400 ml-1">mA</span>
        </div>
      </template>
      <template v-else>n/a</template>
    </span>
  </div>
</template>

<script setup lang="ts">
import type { Channel } from "@/types/channel"
import ButtonComponent from "./ui/ButtonComponent.vue"
import { ref, watch } from "vue"

const props = defineProps<{
  channel: Channel
  disabled?: boolean
}>()

const emit = defineEmits(["toggle", "ao-change"])

const inputValue = ref(
  typeof props.channel.state === "number" ? formatValue(props.channel.state) : "4.00"
)

/**
 * Ограничение и автоформат при вводе
 */
function onInput(e: Event) {
  const target = e.target as HTMLInputElement
  let val = parseFloat(target.value)

  if (isNaN(val)) {
    inputValue.value = "0.00"
    return
  }

  // clamp
  if (val < 0) val = 0
  if (val > 24) val = 24

  inputValue.value = target.value // не форматируем сразу, чтобы ввод был "живым"
}

/**
 * При потере фокуса → формат в x.xx
 */
function onBlur() {
  let val = parseFloat(inputValue.value)
  if (isNaN(val)) val = 0
  if (val < 0) val = 0
  if (val > 24) val = 24
  inputValue.value = formatValue(val)
}

/**
 * Подтверждение → эмитим число
 */
function onConfirm() {
  let val = parseFloat(inputValue.value)
  if (isNaN(val)) val = 0
  if (val < 0) val = 0
  if (val > 24) val = 24
  emit("ao-change", val)
  inputValue.value = formatValue(val) // автоформат после отправки
}

/**
 * Утилита: формат до x.xx
 */
function formatValue(num: number): string {
  return num.toFixed(2)
}
</script>
