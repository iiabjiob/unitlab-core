<template>
  <div class="ui-toolbar">
    <div class="ui-toolbar__section ui-toolbar__section--left">
      <h2 v-if="title" class="ui-toolbar__title">
        {{ title }}
      </h2>
      <slot name="left" />
    </div>

    <div class="ui-toolbar__section ui-toolbar__section--center">
      <slot name="center" />
    </div>

    <div class="ui-toolbar__section ui-toolbar__section--right">
      <slot name="right" />
    </div>
  </div>
</template>

<script setup lang="ts">
const props = defineProps<{
  title?: string
}>()
</script>

<style scoped>
.ui-toolbar {
  align-items: center;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-sm);
  box-shadow: 0 1px 3px 0 rgb(0 0 0 / 0.1), 0 1px 2px -1px rgb(0 0 0 / 0.1);
  display: flex;
  justify-content: space-between;
  padding: 0.5rem 0.75rem;
}

.ui-toolbar__section {
  align-items: center;
  display: flex;
}

.ui-toolbar__section--left {
  gap: 0.125rem;
}

.ui-toolbar__section--center {
  flex: 1 1 auto;
  justify-content: center;
}

.ui-toolbar__section--right {
  gap: 0.5rem;
}

.ui-toolbar__title {
  font-size: var(--text-sm);
  font-weight: 600;
  line-height: 1.25rem;
  margin: 0 1rem 0 0;
}

.dark .ui-toolbar {
  border-color: var(--color-neutral-700);
}
</style>
