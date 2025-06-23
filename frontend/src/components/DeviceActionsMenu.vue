<template>
  <Menu as="div" class="relative inline-block text-left">

    <MenuButton class="btn btn-secondary p-0">
      <span class="sr-only">Open actions</span>
      <EllipsisVerticalIcon size="24"/>
    </MenuButton>

    <transition
      enter-active-class="transition duration-100 ease-out"
      enter-from-class="transform scale-95 opacity-0"
      enter-to-class="transform scale-100 opacity-100"
      leave-active-class="transition duration-75 ease-in"
      leave-from-class="transform scale-100 opacity-100"
      leave-to-class="transform scale-95 opacity-0"
    >
      <MenuItems
        class="absolute right-0 mt-2 w-36 origin-top-right rounded-md bg-white dark:bg-gray-900 shadow-lg ring-1 ring-black/10 dark:ring-white/10 focus:outline-none z-10 border border-gray-100 dark:border-gray-800"
      >
        <div class="py-1">
          <MenuItem v-slot="{ active }">
            <button
              @click="$emit('toggle')"
              class="w-full flex items-center px-3 py-2 text-xs rounded transition
                    hover:bg-gray-50 dark:hover:bg-gray-800
                    text-gray-800 dark:text-gray-200
                    "
              :class="active ? 'bg-gray-100 dark:bg-gray-800' : ''"
            >
              <svg class="w-4 h-4 mr-2" fill="none" stroke="currentColor" stroke-width="2"
                viewBox="0 0 24 24" aria-hidden="true">
                <path v-if="isActive"
                  stroke-linecap="round" stroke-linejoin="round"
                  d="M18 12H6"/>
                <path v-else
                  stroke-linecap="round" stroke-linejoin="round"
                  d="M12 6v12m6-6H6"/>
              </svg>
              {{ isActive ? 'Disable' : 'Enable' }}
            </button>
          </MenuItem>
          <MenuItem v-slot="{ active }">
            <button
              @click="$emit('delete')"
              class="w-full flex items-center px-3 py-2 text-xs rounded transition
                    hover:bg-red-50 dark:hover:bg-gray-800
                    text-red-700 dark:text-red-400"
              :class="active ? 'bg-red-100 dark:bg-gray-800' : ''"
            >
              <svg class="w-4 h-4 mr-2" fill="none" stroke="currentColor" stroke-width="2"
                viewBox="0 0 24 24" aria-hidden="true">
                <path stroke-linecap="round" stroke-linejoin="round"
                  d="M6 18L18 6M6 6l12 12"/>
              </svg>
              Delete
            </button>
          </MenuItem>
        </div>
      </MenuItems>
    </transition>
  </Menu>
</template>

<script setup lang="ts">
import { Menu, MenuButton, MenuItems, MenuItem } from '@headlessui/vue'
import EllipsisVerticalIcon from './icons/EllipsisVerticalIcon.vue';

defineProps<{ isActive: boolean }>()
defineEmits(['toggle', 'delete'])
</script>
