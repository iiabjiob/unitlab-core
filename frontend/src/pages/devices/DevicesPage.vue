<template>
  <div class="flex h-full">

    <ResizablePanel
        placement="left"
        storageKey="page-sidebar-width"
        :defaultSize="240"
        :minSize="200"
        :maxSize="400"
        >
        <aside class="p-4 h-full flex flex-col">
            <DeviceListSidebar />
        </aside>
    </ResizablePanel>
    
    <section class="flex-1 p-4">
      <router-view />
    </section>
  </div>
</template>

<script setup lang="ts">
import ResizablePanel from '@/components/ui/ResizablePanel.vue';
import DeviceListSidebar from './components/DeviceListSidebar.vue';
</script>
