<template>
  <header class="flex justify-between items-center p-5">
      <span class="text-xl font-bold">FAT-Simulator</span>
      <nav class="space-x-5">
        <RouterLink to="/">Dashboard</RouterLink>
        <RouterLink to="/event-log">Events</RouterLink>
        <RouterLink to="/diagnostics">Diagnostics</RouterLink>
        <RouterLink to="/settings">Settings</RouterLink>
      </nav>
    </header>
</template>

<script setup>
import { RouterLink } from 'vue-router';
</script>
