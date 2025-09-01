<template>
  <div class="p-5">

    <div class="flex gap-5 mb-5">

      <ButtonComponent @click="onStart" :disabled="!store.active || store.status === 'running'">
        Start
      </ButtonComponent>

      <ButtonComponent type="secondary" @click="store.resetState" :disabled="!store.active">
        Reset state
      </ButtonComponent>

      <ButtonComponent type="secondary" @click="store.resetAllDos(unitId)" :disabled="!unitId">
        Reset all DOs
      </ButtonComponent>
    </div>

    <!-- Progress bar -->
    <div class="h-2 rounded bg-gray-200 dark:bg-gray-700 overflow-hidden">
      <div class="h-full bg-gray-600" :style="{ width: store.progress + '%' }"></div>
    </div>


    <!-- Steps checklist -->
    <ol class="mt-2 space-y-1 text-sm">
      <li v-for="(s, i) in store.active?.steps || []" :key="i" class="flex items-center gap-2">
        <span class="inline-flex h-4 w-4 items-center justify-center rounded border"
          :class="store.completed[i] ? 'bg-green-500 border-green-500' : 'bg-white dark:bg-gray-900'">
          <span v-if="store.completed[i]" class="text-[10px] text-white">✓</span>
        </span>
        <span class="font-mono text-xs text-gray-500">#{{ i + 1 }}</span>
        <span>{{ store.debugDescribe(i) }}</span>
      </li>
    </ol>


    <!-- Error note -->
    <p v-if="store.lastError" class="text-xs text-red-600">Error: {{ store.lastError }}</p>

  </div>
</template>


<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { buildPilotSequence } from "@/sequences/demoSequences"
import ButtonComponent from "@/components/ui/ButtonComponent.vue"


const deviceStore = useDeviceStore()
const store = useSequenceStore()
const unitId = ref<string>("")


const doDevices = computed(() => deviceStore.devices
  .filter(d => d.type === 'do' && d.status === 'online'))

function refreshSeq() {
  if (!unitId.value) return
  store.setSequence(buildPilotSequence(unitId.value))
}


async function onStart() {
  if (!store.active) refreshSeq()
  await store.start()
}


// Auto-select first available DO device
onMounted(() => {
  if (!unitId.value && doDevices.value.length) {
    unitId.value = doDevices.value[0].unit_id
    refreshSeq()
  }
})


// Rebuild sequence when the unit changes
watch(unitId, () => refreshSeq())
</script>
