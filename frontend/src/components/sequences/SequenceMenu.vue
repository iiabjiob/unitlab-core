<template>

  <MenuComponent>

    <MenuItemComponent @click="$emit('export')">
      Export
    </MenuItemComponent>
    <MenuItemComponent danger @click="$emit('delete')">
      Delete
    </MenuItemComponent>

  </MenuComponent>

</template>

<script setup lang="ts">

import MenuComponent from "../ui/MenuComponent.vue";
import MenuItemComponent from "../ui/MenuItemComponent.vue";

const emit = defineEmits<{
  (e: "export"): void
  (e: "delete"): void
}>()
</script>
