<template>
  <div class="p-5 flex flex-col gap-5">

    <SwitchgearComponent
      title="Feeder QF"
      :do-unit-id="'do-unit-9458EC'"
      :do-open-ch="0"
      :do-close-ch="1"
      :di-unit-id="'di-unit-942C14'"
      :di-open-pulse-ch="0"
      :di-close-pulse-ch="1"
    />

    <DoPulseTester
      title="SCADA Pulse"
      :do-unit-id="'do-unit-9458EC'"
      :ch-a="3"
      :ch-b="4"
      :pulse-ms="200"
      label-a="SCADA OPEN"
      label-b="SCADA CLOSE"
    />

  </div>
</template>

<script setup lang="ts">
import DoPulseTester from '@/components/DoPulseTester.vue';
import SwitchgearComponent from '@/components/SwitchgearComponent.vue';

</script>
