<template>
  <Menu as="div" class="relative inline-block text-left" @click.stop>
    <!-- Kebab button -->
    <MenuButton
      class="flex items-center justify-center rounded-full w-6 h-6 text-neutral-500 hover:bg-neutral-100 dark:hover:bg-neutral-700 cursor-pointer text-xl font-bold"
      aria-label="Open menu"
    >
      ⋮
    </MenuButton>

    <!-- Animated dropdown -->
    <Transition
      enter="transition ease-out duration-100"
      enter-from="transform opacity-0 scale-95"
      enter-to="transform opacity-100 scale-100"
      leave="transition ease-in duration-75"
      leave-from="transform opacity-100 scale-100"
      leave-to="transform opacity-0 scale-95"
    >
      <MenuItems
        class="absolute right-0 mt-2 w-40 origin-top-right rounded-md bg-white dark:bg-neutral-700 shadow-lg ring-1 ring-neutral-200 dark:ring-neutral-600 focus:outline-none z-10"
      >
        <div class="py-1">
          <slot />
        </div>
      </MenuItems>
    </Transition>
  </Menu>
</template>

<script setup lang="ts">
import { Menu, MenuButton, MenuItems } from "@headlessui/vue"
</script>
