<template>
  <div class="border border-gray-300 dark:border-gray-700 rounded p-4 bg-white dark:bg-gray-800 shadow w-full space-y-3">
    <!-- Header -->
    <div class="flex items-center justify-between mb-4">
      <div>
        <div class="text-sm text-gray-500 dark:text-gray-400">Digital Outputs Unit</div>
        <div class="font-bold text-lg">{{ unitId }}</div>
        <div class="flex items-center gap-2 text-sm mt-1">
          <!-- Group ON/OFF -->
          <div class="flex gap-1 items-center">
            <ButtonComponent type="secondary"
            size="xs"
            :disabled="groupPending"
            @click="toggleAll(true)">
            ON</ButtonComponent>
            <ButtonComponent type="secondary"
            size="xs"
            :disabled="groupPending"
            @click="toggleAll(false)">
            OFF</ButtonComponent>
            <div v-if="groupPending" class="text-xs text-gray-500 dark:text-gray-400">
              ⏳ Group in progress...
            </div>
          </div>

        </div>
      </div>
    </div>

    <!-- Body -->
    <div class="divide-y divide-gray-300 dark:divide-gray-700">
      <!-- Signal Rows -->
      <div v-for="(signal, index) in signals" :key="index" class="flex items-center justify-between py-1">

        <!-- Signal Name -->
        <div>{{ signal.name }}</div>

        <div class="flex items-center gap-4">

          <!-- ON / OFF Buttons -->
          <div class="flex gap-1">
            <ButtonComponent
              type="secondary"
              size="xs"
              :disabled="isOnDisabled(`${unitId}/${signal.index}`)"
              @click="toggleSignal(signal, true)"
              >
              ON
            </ButtonComponent>
            <ButtonComponent
              type="secondary"
              size="xs"
              :disabled="isOffDisabled(`${unitId}/${signal.index}`)"
              @click="toggleSignal(signal, false)"
            >
            OFF
            </ButtonComponent>
          </div>

          <!-- Status -->
          <span class="cursor-default" :title="signalStore.failed?.[`${unitId}/${signal.index}`]
            ? 'FAILED'
            : signalStore.pending[`${unitId}/${signal.index}`]
              ? 'WAITING'
              : signalStore.states[`${unitId}/${signal.index}`]
                ? 'ON'
                : 'OFF'">
            <template v-if="signalStore.failed?.[`${unitId}/${signal.index}`]">❌</template>
            <template v-else-if="visiblePending[`${unitId}/${signal.index}`]">⏳</template>
            <template v-else>{{ signalStore.states[`${unitId}/${signal.index}`] ? '🟢' : '⚪' }}</template>
          </span>

        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue'
import { useSignalStore } from '@/stores/useSignalStore'
import ButtonComponent from '@/components/ui/ButtonComponent.vue'

import type { Signal } from '@/types/signal'


const props = defineProps<{
  unitId: string
  signals: Signal[]
}>()

// ✅ refs
const visiblePending = ref<Record<string, boolean>>({})
const groupPending = ref(false)

const signalStore = useSignalStore()

function toggleSignal(signal: Signal, state: boolean) {
  signalStore.toggleSignal(props.unitId, signal, state)
}

function toggleAll(state: boolean) {
  groupPending.value = true

  props.signals.forEach(signal => {
    signalStore.toggleSignal(props.unitId, signal, state)
  })

  const maxDelay = Math.max(...props.signals.map(s => s.delayMs ?? 0))
  const totalDelay = maxDelay + 1000

  setTimeout(() => {
    groupPending.value = false
  }, totalDelay)
}

function isOnDisabled(key: string): boolean {
  return groupPending.value || signalStore.states[key] || key in signalStore.pending
}

function isOffDisabled(key: string): boolean {
  return groupPending.value || !signalStore.states[key] || key in signalStore.pending
}

// ✅ Автоотображение ⏳
watch(() => signalStore.pending, (newPending) => {
  Object.keys(newPending).forEach((key) => {
    if (!(key in visiblePending.value)) {
      visiblePending.value[key] = false
      setTimeout(() => {
        if (signalStore.pending[key]) {
          visiblePending.value[key] = true
        }
      }, 100)
    }
  })

  Object.keys(visiblePending.value).forEach((key) => {
    if (!newPending[key]) {
      delete visiblePending.value[key]
    }
  })
}, { deep: true })

</script>

