<template>
  <div>
    <PageHeader title="Settings" />

    <nav class="flex mb-4">
      <router-link
        to="/settings/wifi"
        class="px-4 py-2 text-lg font-semibold border-b-2"
        active-class="border-emerald-500 text-emerald-500"
        :class="{ 'border-emerald-500 text-emerald-500': $route.path === '/settings/wifi' }"
      >
        Wi-Fi
      </router-link>
      <router-link
        to="/settings/other"
        class="px-4 py-2 text-lg font-semibold border-b-2"
        active-class="border-emerald-500 text-emerald-500"
        :class="{ 'border-emerald-500 text-emerald-500': $route.path === '/settings/other' }"
      >
        Other
      </router-link>
    </nav>

    <!-- Здесь будут загружаться дочерние маршруты -->
    <router-view />
  </div>
</template>

<script setup>
import PageHeader from "@/components/PageHeader.vue";
</script>
