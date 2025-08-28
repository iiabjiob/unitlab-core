<template>
  <aside
    :class="[
      'bg-white dark:bg-gray-800 shadow-md flex flex-col',
      isCollapsed ? 'w-16' : 'w-64'
    ]"
  >
    <!-- Header -->
    <div class="flex items-center h-10 border-b border-gray-200 dark:border-gray-700">
      <div class="flex items-center justify-center w-16">
        <button
          class="flex items-center justify-center w-8 h-8 rounded hover:bg-gray-100 dark:hover:bg-gray-700"
          @click="toggle"
        >
          <component :is="isCollapsed ? ChevronRightIcon : ChevronLeftIcon" size="20" />
        </button>
      </div>
      <AppLogo v-if="!isCollapsed"/>
    </div>

    <!-- Menu -->
    <nav class="flex-1 py-2 space-y-1">
      <!-- <RouterLink
        to="/signal-list"
        class="flex items-center rounded hover:bg-gray-100 dark:hover:bg-gray-700 h-8"
      >
        <div class="flex items-center justify-center w-16">
          <ClockIcon size="20" />
        </div>
        <span v-if="!isCollapsed" class="truncate">Signal List</span>
      </RouterLink> -->

      <RouterLink
        to="/devices"
        class="flex items-center rounded hover:bg-gray-100 dark:hover:bg-gray-700 h-8"
      >
        <div class="flex items-center justify-center w-16">
          <DraftIcon size="20" />
        </div>
        <span v-if="!isCollapsed" class="truncate">Devices</span>
      </RouterLink>
    </nav>


    <!-- Event Log -->
    <EventLogAside :isCollapsed="isCollapsed" />

  </aside>
</template>

<script setup lang="ts">
import { ref } from "vue";
import ChevronRightIcon from "./icons/ChevronRightIcon.vue";
import ChevronLeftIcon from "./icons/ChevronLeftIcon.vue";
import DraftIcon from "./icons/DraftIcon.vue";
import ClockIcon from "./icons/ClockIcon.vue";
import AppLogo from "./AppLogo.vue";

const isCollapsed = ref(false);
const toggle = () => (isCollapsed.value = !isCollapsed.value);
</script>
