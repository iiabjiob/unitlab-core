<template>

  <header class="px-5 py-2 flex bg-white dark:bg-gray-950 h-10 items-center justify-between sticky top-0 z-10">
    <HealthStatusComponent />
    <TimeSyncComponent />
  </header>

</template>

<script setup lang="ts">

import TimeSyncComponent from './TimeSyncComponent.vue';
import AppLogo from './AppLogo.vue';
import HealthStatusComponent from './HealthStatusComponent.vue';

</script>
