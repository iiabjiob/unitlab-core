<template>
  <router-link :to="to" class="nav-link" active-class="nav-link-active">
    <slot />
  </router-link>
</template>

<script setup lang="ts">
import type { RouteLocationRaw } from 'vue-router'

// Типизация пропса `to`
const props = defineProps<{
  to: RouteLocationRaw
}>()
</script>
