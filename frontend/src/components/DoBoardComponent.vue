<template>
  <div class="border border-gray-300 dark:border-gray-700 rounded p-4 bg-white dark:bg-gray-800 shadow w-full space-y-3">
    <!-- Header -->
    <div class="flex items-center justify-between mb-4">
      <div>
        <div class="text-sm text-gray-500 dark:text-gray-400">Digital Outputs Board</div>
        <div class="font-bold text-lg">{{ boardName }}</div>
        <div class="flex items-center gap-2 text-sm mt-1">
          <!-- Group ON/OFF -->
          <div class="flex gap-1">
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
          </div>
          <div class="flex items-center gap-1">
            <input type="number" name="delay_ms" class="w-16 px-1 py-0.5 rounded border border-gray-500 text-xs" v-model.number="delay" min="0"
              title="Delay between outputs (ms)" />
            <span>ms</span>
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
              :disabled="outputStore.states[`${boardName}/${signal.name}`] || outputStore.pending[`${boardName}/${signal.name}`]"
              @click="toggleSignal(signal, true)"
              >
              ON
            </ButtonComponent>
            <ButtonComponent
              type="secondary"
              size="xs"
              :disabled="!outputStore.states[`${boardName}/${signal.name}`] || outputStore.pending[`${boardName}/${signal.name}`]"
              @click="toggleSignal(signal, false)"
            >
            OFF
            </ButtonComponent>
          </div>

          <!-- Status -->
          <span class="cursor-default" :title="outputStore.failed?.[`${boardName}/${signal.name}`]
            ? 'FAILED'
            : outputStore.pending[`${boardName}/${signal.name}`]
              ? 'WAITING'
              : outputStore.states[`${boardName}/${signal.name}`]
                ? 'ON'
                : 'OFF'">
            <template v-if="outputStore.failed?.[`${boardName}/${signal.name}`]">❌</template>
            <template v-else-if="visiblePending[key]">⏳</template>
            <template v-else>{{ outputStore.states[`${boardName}/${signal.name}`] ? '🟢' : '⚪' }}</template>
          </span>

        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch } from 'vue'
import { useOutputStore } from '@/stores/output'
import { useWebSocketStore } from '@/stores/websocket'
import ButtonComponent from '@/components/ui/ButtonComponent.vue'
import {
  getDoGroupTopic,
  getDoSetTopic
} from '@/utils/topics' // подключаем централизованные топики

const props = defineProps({
  boardName: String,
  signals: Array
})

const delay = ref(50)
const visiblePending = ref({})
const groupPending = ref(false)

const wsStore = useWebSocketStore()
const outputStore = useOutputStore()

function toggleAll(state) {
  groupPending.value = true

  const actions = props.signals.map(signal => ({
    index: parseInt(signal.name.replace(/\D/g, '')),
    name: signal.name,
    key: `${props.boardName}/${signal.name}`
  }))

  // 1️⃣ MQTT-публикация через топик
  const payload = outputStore.buildGroupPayload(props.signals, state, delay.value)
  wsStore.send({
    action: 'publish',
    topic: getDoGroupTopic(props.boardName),
    payload
  })

  // 2️⃣ Отображаем ⏳ локально
  actions.forEach(({ key }, i) => {
    setTimeout(() => {
      outputStore.requestToggle(key, state)
    }, i * delay.value)
  })

  // 3️⃣ Сброс блокировки по таймеру
  const totalDelay = delay.value * (actions.length - 1) + 2000
  setTimeout(() => {
    groupPending.value = false
  }, totalDelay)
}

function toggleSignal(signal, state) {
  const key = `${props.boardName}/${signal.name}`
  if (outputStore.states[key] === state) return

  outputStore.requestToggle(key, state)

  wsStore.send({
    action: 'publish',
    topic: getDoSetTopic(parseInt(signal.name.replace(/\D/g, '')), props.boardName),
    payload: { state }
  })
}

watch(
  () => outputStore.pending,
  (newPending) => {
    for (const key in newPending) {
      if (!visiblePending.value[key]) {
        visiblePending.value[key] = false
        setTimeout(() => {
          if (outputStore.pending[key]) {
            visiblePending.value[key] = true
          }
        }, 100)
      }
    }

    for (const key in visiblePending.value) {
      if (!newPending[key]) {
        delete visiblePending.value[key]
      }
    }
  },
  { deep: true }
)
</script>

