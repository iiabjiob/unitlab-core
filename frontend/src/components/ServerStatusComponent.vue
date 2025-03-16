<template>
  <AlertComponent
    v-if="!serverAvailable"
    message="Connection to server lost. Trying to reconnect..."
    type="error"
    icon="⚠️"
  />
</template>

<script setup>

import AlertComponent from "@/components/AlertComponent.vue";

</script>
