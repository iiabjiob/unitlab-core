<template>
  <div>
    <PageHeader title="Settings" />

    <nav class="flex mb-4">
      <NavlinkComponent to="/settings/project">Project</NavlinkComponent>
    </nav>

    <!-- Здесь будут загружаться дочерние маршруты -->
    <router-view />
  </div>
</template>

<script setup lang="ts">
import PageHeader from "@/components/PageHeader.vue";
import NavlinkComponent from '@/components/ui/NavlinkComponent.vue';
</script>
