<template>
  <div class="flex h-full flex-col items-center justify-center gap-4 text-center text-neutral-500 dark:text-neutral-400">
    <div class="space-y-1">
      <p class="text-lg font-semibold text-neutral-800 dark:text-neutral-100">Select a test run</p>
      <p class="text-sm">Choose an existing run from the sidebar or create a new one.</p>
    </div>
    <RouterLink
      class="rounded-lg bg-neutral-900 px-4 py-2 text-sm font-semibold text-white shadow hover:bg-neutral-800 dark:bg-neutral-100 dark:text-neutral-900 dark:hover:bg-white"
      :to="{ name: 'testRuns.new' }"
    >
      + New Test Run
    </RouterLink>
  </div>
</template>
