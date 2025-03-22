<template>

  <header class="p-5 flex flex-col gap-1">

    <!-- Main Navigation -->
    <div class="flex flex-wrap justify-between items-center">

      <!-- Logo & Status -->
      <div class="flex flex-wrap items-center gap-x-5">
        <AppLogo/>
        <HealthStatusComponent/>
      </div>

      <!-- Navigation & Time -->
      <nav class="flex flex-wrap items-center gap-x-5">
        <!-- Time -->
        <div class="flex items-center gap-2 py-1 px-2 bg-gray-200 dark:bg-gray-800 rounded-md">
          <TimeSyncComponent/>
        </div>
        <!-- Navigation -->
        <NavlinkComponent  to="/">Dashboard</NavlinkComponent>
        <NavlinkComponent  to="/events">Events</NavlinkComponent>
        <NavlinkComponent  to="/settings">Settings</NavlinkComponent>
        <NavlinkComponent  to="/system">System</NavlinkComponent>
      </nav>

    </div>
  </header>

</template>

<script setup>
import NavlinkComponent from './NavlinkComponent.vue';
import TimeSyncComponent from './TimeSyncComponent.vue';
import AppLogo from './AppLogo.vue';
import HealthStatusComponent from './HealthStatusComponent.vue';
</script>
