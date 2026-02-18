<template>
  <div class="flex h-full items-center justify-center">
    <UiMenu v-if="canControl" :options="persistentControlMenuOptions">
      <UiMenuTrigger asChild>
        <button
          type="button"
          class="w-[120px] rounded border border-neutral-300 bg-white px-2 py-1 text-xs font-semibold text-neutral-700 hover:bg-neutral-100 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800"
          @click.stop
        >
          <span class="inline-flex w-full items-center gap-1.5 whitespace-nowrap">
            <span
              class="h-2 w-2 shrink-0 rounded-full"
              :class="lampClass"
            ></span>
            <span>Control</span>
            <span
              class="ml-auto text-[10px] font-semibold uppercase tracking-[0.08em]"
              :class="statusClass"
            >
              {{ statusTag }}
            </span>
          </span>
        </button>
      </UiMenuTrigger>
      <UiMenuContent>
        <UiMenuLabel>
          <span class="inline-flex items-center gap-2 text-xs font-semibold">
            <span
              class="h-2.5 w-2.5 shrink-0 rounded-full"
              :class="lampClass"
            ></span>
            <span>State: {{ stateLabel }}</span>
          </span>
        </UiMenuLabel>
        <UiMenuSeparator />
        <UiMenuItem
          :disabled="disabled"
          @select="emit('setOn')"
        >
          ON
        </UiMenuItem>
        <UiMenuItem
          :disabled="disabled"
          @select="emit('setOff')"
        >
          OFF
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
    <span v-else class="text-xs text-neutral-400">—</span>
  </div>
</template>

<script setup lang="ts">
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
  UiMenuTrigger,
} from "@affino/menu-vue"

const props = defineProps<{
  canControl: boolean
  lampClass: string
  statusClass: string
  statusTag: string
  stateLabel: string
  disabled: boolean
}>()

const emit = defineEmits<{
  (event: "setOn"): void
  (event: "setOff"): void
}>()

const persistentControlMenuOptions = { closeOnSelect: false }
</script>
