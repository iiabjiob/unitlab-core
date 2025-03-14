<template>
  <header class="p-5 flex flex-col gap-1">

    <div class="flex justify-between items-center">
        <!-- Logo -->
        <RouterLink
          to="/"
          class="text-xl font-bold text-emerald-500">
          FAT Simulator
        </RouterLink>

        <!-- Navigation -->
        <nav class="inline-flex items-center gap-x-5">

          <TimeSyncComponent/>

          <NavlinkComponent  to="/">Dashboard</NavlinkComponent>

          <NavlinkComponent  to="/events">Events</NavlinkComponent>

          <NavlinkComponent  to="/settings">Settings</NavlinkComponent>

          <NavlinkComponent  to="/system">System</NavlinkComponent>

        </nav>
    </div>
    <div>
      <ServerStatusComponent/>
    </div>

    </header>
</template>

<script setup>
import { RouterLink } from 'vue-router';
import NavlinkComponent from './NavlinkComponent.vue';
import TimeSyncComponent from './TimeSyncComponent.vue';
import ServerStatusComponent from './ServerStatusComponent.vue';
</script>
