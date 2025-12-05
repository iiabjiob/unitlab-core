<template>
  <MenuComponent>
    <MenuItemComponent danger @click="$emit('delete')">
      Delete
    </MenuItemComponent>
  </MenuComponent>
</template>

<script setup lang="ts">
import MenuComponent from '../ui/MenuComponent.vue';
import MenuItemComponent from '../ui/MenuItemComponent.vue';

defineEmits<{
  (e: 'toggle'): void;
  (e: 'delete'): void
  }>()
</script>
