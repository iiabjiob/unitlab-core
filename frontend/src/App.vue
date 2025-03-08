<script setup>
import AppFooter from './components/AppFooter.vue';
import AppHeader from './components/AppHeader.vue';
</script>

<template>
  <div class="min-h-screen flex flex-col">
    <AppHeader/>

    <main class="flex-grow">
      <RouterView />
    </main>

    <AppFooter/>
  </div>
</template>
