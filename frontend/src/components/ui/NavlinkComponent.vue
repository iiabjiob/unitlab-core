<template>
  <router-link :to="to" class="nav-link" active-class="nav-link-active">
    <slot />
  </router-link>
</template>

<script setup>
defineProps({ to: String });
</script>

