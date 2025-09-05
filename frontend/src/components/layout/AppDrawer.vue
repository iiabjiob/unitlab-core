<template>
  <teleport to="body">
    <div v-if="open" class="fixed inset-0 z-40 flex">
      <!-- Backdrop -->
      <div class="flex-1 bg-black/50" @click="$emit('close')" />

      <!-- Slide-over panel -->
      <div
        class="w-72 max-w-[80%] h-full bg-white dark:bg-neutral-900 border-r border-neutral-200 dark:border-neutral-800 shadow-xl
               transform transition-transform duration-200 ease-out"
      >
        <!-- Header -->
        <div class="flex items-center justify-between px-3 py-2 border-b border-neutral-200 dark:border-neutral-800">
          <span class="text-sm font-semibold">Menu</span>
          <button
            class="w-8 h-8 inline-flex items-center justify-center rounded-lg hover:bg-neutral-100 dark:hover:bg-neutral-800"
            aria-label="Close menu"
            @click="$emit('close')"
          >
            <svg xmlns="http://www.w3.org/2000/svg" class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
              <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
            </svg>
          </button>
        </div>

        <!-- Menu content -->
        <div
          class="flex-1 overflow-y-auto"
          @click="$emit('close')"
        >
          <slot />
        </div>
      </div>
    </div>
  </teleport>
</template>

<script setup lang="ts">
defineProps<{ open: boolean }>()
defineEmits<{
  (e: "close"): void
}>()
</script>
