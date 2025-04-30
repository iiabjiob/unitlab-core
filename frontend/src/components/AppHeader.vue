<template>

  <header class="p-5 flex flex-col gap-1">

    <!-- Main Navigation -->
    <div class="flex flex-wrap justify-between items-center gap-y-2">

      <!-- Logo & Status -->
      <div class="flex flex-wrap items-center gap-x-5">
        <AppLogo/>
        <HealthStatusComponent/>
      </div>

      <!-- Navigation & Time -->
      <nav class="flex flex-wrap items-center gap-2">
        <!-- Time -->
        <TimeSyncComponent class="me-5"/>

        <!-- Navigation -->
        <NavlinkComponent to="/">IO</NavlinkComponent>
        <NavlinkComponent to="/devices">Devices</NavlinkComponent>
        <NavlinkComponent to="/events">Events</NavlinkComponent>
        <NavlinkComponent to="/settings">Settings</NavlinkComponent>
        <NavlinkComponent to="/system">System</NavlinkComponent>
      </nav>

    </div>
  </header>

</template>

<script setup lang="ts">

import TimeSyncComponent from './TimeSyncComponent.vue';
import AppLogo from './AppLogo.vue';
import HealthStatusComponent from './HealthStatusComponent.vue';
import NavlinkComponent from './ui/NavlinkComponent.vue';

</script>
