<!-- src/components/switchgear/SwitchgearMenu.vue -->
<template>
  <MenuComponent>

    <MenuItemComponent danger @click.stop="$emit('delete')">
      Delete
    </MenuItemComponent>

  </MenuComponent>
</template>

<script setup lang="ts">
import MenuComponent from "../ui/MenuComponent.vue";
import MenuItemComponent from "../ui/MenuItemComponent.vue";
defineEmits<{ (e: "delete"): void }>()
</script>
