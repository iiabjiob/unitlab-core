<template>
  <li
    class="flex flex-col h-full p-3 rounded-md bg-white dark:bg-neutral-800 shadow-sm border dark:border-neutral-700 border-neutral-200"
  >
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div class="flex items-center gap-2 flex-wrap">
        <span class="font-mono font-semibold">
          {{ sequence.name }}
        </span>
      </div>

      <!-- меню действий -->
      <Menu as="div" class="relative inline-block text-left">
        <div>
          <MenuButton
            class="flex items-center justify-center rounded-full w-6 h-6 text-neutral-500 hover:bg-neutral-100 dark:hover:bg-neutral-700 cursor-pointer text-xl font-bold"
          >
            ⋮
          </MenuButton>
        </div>

        <Transition
          enter="transition ease-out duration-100"
          enter-from="transform opacity-0 scale-95"
          enter-to="transform opacity-100 scale-100"
          leave="transition ease-in duration-75"
          leave-from="opacity-100 scale-100"
          leave-to="opacity-0 scale-95"
        >
          <MenuItems
            class="absolute right-0 mt-2 w-40 origin-top-right rounded-md bg-white dark:bg-neutral-700 shadow-lg ring-1 ring-neutral-200 dark:ring-neutral-600 focus:outline-none z-10"
          >
            <div class="py-1">
              <MenuItem v-slot="{ active }">
                <button
                  @click="onResetAllDos"
                  :class="[
                    active ? 'bg-neutral-100 dark:bg-neutral-600' : '',
                    'block w-full px-4 py-2 text-sm text-left text-neutral-700 dark:text-neutral-200'
                  ]"
                >
                  Reset DOs
                </button>
              </MenuItem>
            </div>
          </MenuItems>
        </Transition>
      </Menu>
    </div>

    <!-- Описание -->
    <p class="mt-1 text-xs text-neutral-500">
      {{ sequence.description }}
    </p>

    <div class="flex gap-3 items-center py-3">
      <ButtonComponent size="sm" type="primary" @click="onStart" :disabled="store.isRunning(sequence)">Start</ButtonComponent>
      <ButtonComponent size="sm" type="secondary" @click="onStop" :disabled="!store.isRunning(sequence)">Stop</ButtonComponent>
      <ButtonComponent size="sm" type="secondary" @click="onReset" :disabled="store.isRunning(sequence)">Reset</ButtonComponent>

      <BadgeComponent class="text-xs">{{ statusLabel }}</BadgeComponent>
    </div>

    <!-- Progress bar -->
    <div class="mt-2 h-2 rounded bg-neutral-200 dark:bg-neutral-700 overflow-hidden">
      <div
        class="h-full bg-neutral-600 transition-all duration-300"
        :style="{ width: store.getProgress(sequence) + '%' }"
      ></div>
    </div>

    <!-- Steps checklist -->
    <ol class="mt-3 space-y-1 text-sm">
      <li
        v-for="(s, i) in sequence.steps"
        :key="i"
        class="flex items-center gap-2"
      >
        <span
          class="inline-flex h-4 w-4 items-center justify-center rounded border"
          :class="st.completed[i] ? 'bg-green-500 border-green-500' : 'bg-white dark:bg-neutral-900'"
        >
          <span v-if="st.completed[i]" class="text-[10px] text-white">✓</span>
        </span>
        <span class="font-mono text-xs text-neutral-500">#{{ i + 1 }}</span>
        <span>{{ store.getStepDescription(sequence, i) }}</span>
      </li>
    </ol>

    <!-- Error -->
    <p v-if="st.lastError" class="text-xs text-red-600 mt-2">
      Error: {{ st.lastError }}
    </p>
  </li>
</template>

<script setup lang="ts">
import { Menu, MenuButton, MenuItems, MenuItem } from "@headlessui/vue"
import { computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import type { SequenceDef } from "@/types/sequences"
import BadgeComponent from "@/components/ui/BadgeComponent.vue"
import ButtonComponent from "../ui/ButtonComponent.vue"

const props = defineProps<{ sequence: SequenceDef }>()
const store = useSequenceStore()

const st = computed(() => store.ensureState(props.sequence))

const statusLabel = computed(() => {
  switch (st.value.status) {
    case "idle": return "Idle"
    case "running": return "Running"
    case "completed": return "Completed"
    case "stopped": return "Stopped"
  }
})

async function onStart() {
  await store.start(props.sequence)
}
function onStop() {
  store.stop(props.sequence)
}
function onReset() {
  store.resetState(props.sequence)
}
function onResetAllDos() {
  store.resetAllDos(props.sequence)
}
</script>
