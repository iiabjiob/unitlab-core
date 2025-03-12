<template>
  <header class="flex justify-between items-center p-5">
      <RouterLink
        to="/"
        class="text-xl font-bold text-emerald-500">
        FAT Simulator
      </RouterLink>

      <nav class="space-x-5">

        <NavlinkComponent  to="/">Dashboard</NavlinkComponent>

        <NavlinkComponent  to="/events">Events</NavlinkComponent>

        <NavlinkComponent  to="/settings">Settings</NavlinkComponent>

        <NavlinkComponent  to="/system">System</NavlinkComponent>

      </nav>
    </header>
</template>

<script setup>
import { RouterLink } from 'vue-router';
import NavlinkComponent from './NavlinkComponent.vue';
</script>
