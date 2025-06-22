<template>
  <div class="flex items-center px-2 py-1 w-full border-b border-gray-300 dark:border-gray-700 last:border-0 text-xs">
    <!-- Имя канала -->
    <span>{{ channel.name || ('CH' + (channel.index + 1)) }}</span>

    <!-- Управление (DO) -->
    <div v-if="channel.type === 'DO'" class="flex flex-1 items-center justify-end gap-2">
      <ButtonComponent size="xs" type="secondary" class="min-w-[44px]" :disabled="channel.state"
        @click="$emit('toggle', false)">
        Off
      </ButtonComponent>
      <ButtonComponent size="xs" type="secondary" class="min-w-[44px]" :disabled="!channel.state"
        @click="$emit('toggle', true)">
        On
      </ButtonComponent>
    </div>

    <!-- AO -->
    <div v-else-if="channel.type === 'AO'" class="flex flex-1 items-center justify-end gap-1">
      <!-- Инпут по всей доступной ширине -->
      <input
        type="number"
        name="ao-value-input"
        min="4"
        max="20"
        step="0.1"
        :value="inputValue"
        @input="onAoInput"
        @blur="onAoConfirm"
        @keyup.enter="onAoConfirm"
        @keyup.tab="onAoConfirm"
        class="flex-1 min-w-0 text-right px-1 border rounded text-xs max-w-[80px] ml-3"
      />
      <input
        type="range"
        name="ao-value-range"
        min="4"
        max="20"
        step="0.1"
        :value="aoValue"
        @input="onAoSliderMove"
        @change="onAoSliderChange"
        class="flex-1 min-w-0 h-1 accent-gray-500"
      />
      <span class="text-xs text-gray-400 ml-1">mA</span>

    </div>

    <!-- DI -->
    <div v-else-if="channel.type === 'DI'" class="flex flex-1 items-center justify-end gap-2">
      <!-- Для DI управления нет, только статус -->
    </div>

    <!-- Status с вертикальным divider -->
    <span class="ml-4 pl-4 border-l border-gray-300 dark:border-gray-700 text-right">
      <template v-if="channel.type === 'DO' || channel.type === 'DI'">
        {{ channel.state ? '🟢' : '⚪️' }}
      </template>
      <template v-else-if="channel.type === 'AO'">
        <div class="min-w-[50px] text-nowrap">

          {{ channel.state }}
          <span class="text-xs text-gray-400 ml-1">mA</span>
        </div>
      </template>
      <template v-else>
        n/a
      </template>
    </span>
  </div>
</template>

<script setup lang="ts">
import type { Channel } from '@/types/channel'
import ButtonComponent from './ui/ButtonComponent.vue'
import { ref, watch } from 'vue'

const props = defineProps<{ channel: Channel; disabled?: boolean }>()
const emit = defineEmits(['toggle', 'pulse', 'ao-change'])

const aoValue = ref(typeof props.channel.state === 'number' ? props.channel.state : 4)
const inputValue = ref(aoValue.value) // локальное состояние input[type=number]

watch(() => props.channel.state, (val) => {
  if (typeof val === 'number') {
    aoValue.value = val
    inputValue.value = val
  }
})

function onAoSliderMove(event: Event) {
  const val = +(event.target as HTMLInputElement).value
  aoValue.value = val
  inputValue.value = val
}

function onAoSliderChange(event: Event) {
  const val = +(event.target as HTMLInputElement).value
  aoValue.value = val
  inputValue.value = val
  emit('ao-change', val)
  console.log('Send to device:', val)
}

// Когда меняем инпут — только inputValue, но не отправляем наверх!
function onAoInput(event: Event) {
  inputValue.value = +(event.target as HTMLInputElement).value
}

// Только на blur/Enter/Tab реально меняем значение (и slider)
function onAoConfirm() {
  aoValue.value = inputValue.value
  emit('ao-change', inputValue.value)
  console.log(inputValue.value)
}

</script>
