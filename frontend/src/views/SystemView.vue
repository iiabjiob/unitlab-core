<script setup>
import { ref } from "vue";
import PageHeader from "@/components/PageHeader.vue";
import LoadingSpinner from "@/components/LoadingSpinner.vue";
import AlertComponent from "@/components/AlertComponent.vue";
const message = ref("");

</script>

<template>
  <div>
    <PageHeader title="System Information" />
    <AlertComponent v-if="message" :message="message" type="error" icon="⚠️" />
    <LoadingSpinner size="small"/>
  </div>
</template>
