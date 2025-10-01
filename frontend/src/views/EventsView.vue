<template>
  <div class="p-3">
    <EventsToolbar/>
  </div>
  <div class="h-dvh overflow-hidden p-3 flex flex-col">
    <EventLog/>
  </div>

</template>

<script setup lang="ts">
import EventLog from '@/components/EventLog.vue';
import EventsToolbar from '@/components/toolbars/EventsToolbar.vue';



</script>
