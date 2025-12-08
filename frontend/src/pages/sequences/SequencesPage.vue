<template>
  <div class="flex h-full">

    <ResizablePanel
        placement="left"
        storageKey="sequence-list-width"
        :defaultSize="240"
        :minSize="200"
        :maxSize="400"
        >
        <aside class="p-4 h-full flex flex-col">
            <SequenceListSidebar />
        </aside>
    </ResizablePanel>
    
    <main class="flex-1 p-4">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import ResizablePanel from '@/components/ui/ResizablePanel.vue';
import SequenceListSidebar from './components/SequenceListSidebar.vue';
</script>
