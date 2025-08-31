<template>
  <aside class="bg-white dark:bg-gray-800 flex flex-col w-72">
    <!-- Header -->
    <div class="flex items-center h-10 border-b border-gray-200 dark:border-gray-700 px-5 gap-3">
      <AppLogo/>
      <HealthStatusComponent/>
    </div>

    <!-- Menu -->
    <nav class="flex-1 py-2 space-y-1 px-2">

      <RouterLink
        to="/devices"
        class="flex items-center rounded hover:bg-gray-100 dark:hover:bg-gray-700 h-8"
      >

        <DraftIcon class="w-16" size="20" />

        <span>Devices</span>
      </RouterLink>
    </nav>


  </aside>
</template>

<script setup lang="ts">
import { ref } from "vue";
import DraftIcon from "./icons/DraftIcon.vue";
import AppLogo from "./AppLogo.vue";
import HealthStatusComponent from "./HealthStatusComponent.vue";

</script>
