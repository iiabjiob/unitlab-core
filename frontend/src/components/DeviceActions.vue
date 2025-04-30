<template>
  <div class="flex flex-col md:flex-row justify-start md:justify-end gap-2">
    <ButtonComponent class="min-w-[100px]" size="sm" type="secondary" @click="$emit('toggle')">
      {{ isActive ? 'Disable' : 'Enable' }}
    </ButtonComponent>
    <ButtonComponent class="min-w-[100px] text-red-500" size="sm" type="secondary" @click="$emit('delete')">
      Delete
    </ButtonComponent>
  </div>
</template>

<script setup lang="ts">
import ButtonComponent from './ui/ButtonComponent.vue';

defineProps<{
  isActive: boolean
}>()

defineEmits(['toggle', 'delete'])
</script>
