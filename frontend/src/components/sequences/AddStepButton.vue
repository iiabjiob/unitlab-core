<template>
  <Menu as="div" class="relative inline-block text-left">
    <MenuButton
      class="flex items-center gap-1 px-2 py-1 text-xs border border-dashed cursor-pointer
             border-neutral-400 rounded text-neutral-500 hover:text-neutral-700"
    >
      + Add Step
    </MenuButton>

    <MenuItems
      class="absolute mt-1 w-40 origin-top-left bg-white dark:bg-neutral-800
              border border-neutral-200 dark:border-neutral-700 rounded shadow-lg z-10"
    >
      <MenuItem v-for="kind in kinds" :key="kind">
        <button
          class="w-full text-left px-2 py-1 text-sm hover:bg-neutral-100
                  dark:hover:bg-neutral-700"
          @click="$emit('add', kind)"
        >
          {{ kind }}
        </button>
      </MenuItem>
    </MenuItems>

  </Menu>
</template>

<script setup lang="ts">
import type { StepKind } from "@/types/sequences";
import { Menu, MenuButton, MenuItems, MenuItem, Portal } from "@headlessui/vue"

const props = defineProps<{
  kinds: StepKind[]
}>()

const emit = defineEmits<{
  (e: "add", kind: StepKind): void
}>()
</script>
