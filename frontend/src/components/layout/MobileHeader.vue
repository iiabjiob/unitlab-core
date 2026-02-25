<template>
  <header class="md:hidden flex items-center justify-between px-3 py-2 border-b border-neutral-200 dark:border-neutral-800">
    <div class="flex gap-2 items-center">

      <button
        type="button"
        class="w-9 h-9 inline-flex items-center justify-center rounded-xl hover:bg-neutral-100 dark:hover:bg-neutral-800"
        aria-label="Open menu"
        @pointerup.stop.prevent="$emit('open-drawer')"
        @touchend.stop.prevent="$emit('open-drawer')"
        @click.stop.prevent="$emit('open-drawer')"
      >
        <!-- burger icon -->
        <svg xmlns="http://www.w3.org/2000/svg" class="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h16M4 18h16"/>
        </svg>
      </button>

      <slot name="left">
      </slot>
    </div>

    <slot name="right" />
  </header>
</template>

<script setup lang="ts">
defineEmits<{
  (e: "open-drawer"): void
}>()
</script>
