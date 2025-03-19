<template>
  <div class="flex flex-wrap gap-x-3 items-center">

    <RouterLink
      to="/"
      class="text-xl font-bold text-emerald-500">
      FAT Simulator
    </RouterLink>

    <HealthStatusComponent/>
  </div>
</template>

<script setup>
import { RouterLink } from 'vue-router';
import HealthStatusComponent from './HealthStatusComponent.vue';
</script>
