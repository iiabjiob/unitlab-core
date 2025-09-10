<!-- src/components/switchgear/SwitchgearMenu.vue -->
<template>
  <Menu as="div" class="relative inline-block text-left" @click.stop>
    <MenuButton
      class="flex items-center justify-center rounded-full w-6 h-6 text-neutral-500 hover:bg-neutral-100 dark:hover:bg-neutral-700 cursor-pointer text-xl font-bold"
      @click.stop
    >⋮</MenuButton>

    <Transition enter="transition ease-out duration-100" enter-from="transform opacity-0 scale-95" enter-to="transform opacity-100 scale-100"
                leave="transition ease-in duration-75" leave-from="opacity-100 scale-100" leave-to="opacity-0 scale-95">
      <MenuItems
        class="absolute right-0 mt-2 w-40 origin-top-right rounded-md bg-white dark:bg-neutral-700 shadow-lg ring-1 ring-neutral-200 dark:ring-neutral-600 ring-opacity-5 focus:outline-none z-10"
        @click.stop
      >
        <div class="py-1">
          <MenuItem v-slot="{ active }">
            <button
              :class="[active ? 'bg-neutral-100 dark:bg-neutral-600' : '', 'block w-full px-4 py-2 text-sm text-left text-red-600 dark:text-red-400']"
              @click="$emit('delete')"
            >
              Delete
            </button>
          </MenuItem>
        </div>
      </MenuItems>
    </Transition>
  </Menu>
</template>

<script setup lang="ts">
import { Menu, MenuButton, MenuItems, MenuItem } from "@headlessui/vue"
defineEmits<{ (e: "delete"): void }>()
</script>
