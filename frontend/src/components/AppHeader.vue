<template>

  <header class="px-5 py-2 flex flex-col gap-1 bg-white dark:bg-gray-950">

    <!-- Main Navigation -->
    <div class="flex flex-wrap justify-between items-center gap-y-2">

      <!-- Logo & Status -->
      <div class="flex flex-wrap items-center gap-x-5">
        <AppLogo />
        <HealthStatusComponent />
      </div>

      <div class="flex flex-wrap items-center gap-x-5">

        <!-- Time -->
        <TimeSyncComponent />
        <!-- Navigation -->
        <nav class="hidden md:flex flex-wrap items-center gap-2">
          <NavlinkComponent to="/devices">Devices</NavlinkComponent>
        </nav>

        <!-- Hamburger Button: visible only on mobile -->
        <button class="md:hidden flex flex-col justify-center items-center w-10"
          @click="isMobileMenuOpen = !isMobileMenuOpen" aria-label="Open menu">
          <span class="block w-6 h-0.5 bg-gray-700 mb-1 rounded transition-all"
            :class="isMobileMenuOpen ? 'rotate-45 translate-y-1.5' : ''"></span>
          <span class="block w-6 h-0.5 bg-gray-700 mb-1 rounded transition-all"
            :class="isMobileMenuOpen ? 'opacity-0' : ''"></span>
          <span class="block w-6 h-0.5 bg-gray-700 rounded transition-all"
            :class="isMobileMenuOpen ? '-rotate-45 -translate-y-1.5' : ''"></span>
        </button>
      </div>

    </div>

    <transition name="fade">
      <nav
        v-if="isMobileMenuOpen"
        class="md:hidden mt-2 flex flex-col gap-2 bg-white rounded-xl shadow-lg px-5 py-4 z-50"
      >

        <NavlinkComponent to="/devices" @click="isMobileMenuOpen = false">Devices</NavlinkComponent>


      </nav>
    </transition>
  </header>

</template>

<script setup lang="ts">

import { ref } from "vue";
const isMobileMenuOpen = ref(false);

import TimeSyncComponent from './TimeSyncComponent.vue';
import AppLogo from './AppLogo.vue';
import HealthStatusComponent from './HealthStatusComponent.vue';
import NavlinkComponent from './ui/NavlinkComponent.vue';

</script>
