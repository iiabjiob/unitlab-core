<template>
  <header class="p-5 flex flex-col gap-1">
    <div class="flex flex-wrap justify-between items-center">
      <!-- Logo -->
      <AppLogo/>

      <!-- Navigation -->
      <nav class="flex flex-wrap items-center gap-x-5">

        <TimeSyncComponent/>

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
</script>
