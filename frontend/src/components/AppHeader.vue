<template>
  <header class="flex justify-between items-center">
      <span class="text-xl font-bold">FAT-Simulator</span>
      <nav>
        <RouterLink to="/">Dashboard</RouterLink>
        <RouterLink to="/event-log">Events</RouterLink>
        <RouterLink to="/diagnostics">Diagnostics</RouterLink>
      </nav>
    </header>
</template>

<script setup>
import { RouterLink } from 'vue-router';
</script>
