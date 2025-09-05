<template>
  <div class="h-full w-full flex flex-col bg-neutral-100 dark:bg-neutral-800">
    <div class="flex items-center justify-between p-3 font-semibold">
      <span>Properties / Templates</span>
      <button
        class="w-6 h-6 flex items-center justify-center rounded hover:bg-neutral-200 dark:hover:bg-neutral-700"
        @click="$emit('collapse')"
      >
        ✖
      </button>
    </div>

    <div class="flex-1 p-3 text-sm text-neutral-600 dark:text-neutral-300">
      <p>This panel will show node properties, templates, etc.</p>
    </div>
  </div>
</template>

<script setup lang="ts">
defineEmits<{ (e: "collapse"): void }>()
</script>
