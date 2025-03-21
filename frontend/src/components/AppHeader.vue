<template>

  <header class="p-5 flex flex-col gap-1">

    <!-- Navigation & Logo -->
    <div class="flex flex-wrap justify-between items-center">

      <!-- Logo -->
      <AppLogo/>

      <div class="space-y-1">
        <!-- Time & Health status -->
        <div class="flex justify-end items-center gap-1">
          <div class="flex items-center gap-2 py-1 px-2 bg-gray-200 dark:bg-gray-800 rounded-md">
            <TimeSyncComponent/>
            <HealthStatusComponent/>
          </div>
        </div>

        <!-- Navigation -->
        <nav class="flex flex-wrap items-center gap-x-5">
          <NavlinkComponent  to="/">Dashboard</NavlinkComponent>
          <NavlinkComponent  to="/events">Events</NavlinkComponent>
          <NavlinkComponent  to="/settings">Settings</NavlinkComponent>
          <NavlinkComponent  to="/system">System</NavlinkComponent>
        </nav>
      </div>

    </div>
  </header>

</template>

<script setup>
import NavlinkComponent from './NavlinkComponent.vue';
import TimeSyncComponent from './TimeSyncComponent.vue';
import AppLogo from './AppLogo.vue';
import HealthStatusComponent from './HealthStatusComponent.vue';
</script>
