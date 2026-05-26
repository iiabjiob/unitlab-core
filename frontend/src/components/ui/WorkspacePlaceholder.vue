<script setup lang="ts">
const props = withDefaults(defineProps<{
  tag: string
  title: string
  description?: string
}>(), {
  description: "",
})
</script>

<template>
  <div class="workspace-placeholder">
    <p class="workspace-placeholder__tag">
      {{ props.tag }}
    </p>
    <h2 class="workspace-placeholder__title">
      {{ props.title }}
    </h2>
    <p v-if="props.description" class="workspace-placeholder__description">
      {{ props.description }}
    </p>
    <slot />
  </div>
</template>

<style scoped>
.workspace-placeholder {
  align-items: center;
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 80%, transparent);
  border-radius: 1rem;
  color: var(--color-neutral-600);
  display: flex;
  flex-direction: column;
  height: 100%;
  justify-content: center;
  padding: 4rem 1.5rem;
  text-align: center;
}

.workspace-placeholder__tag {
  color: var(--color-neutral-400);
  font-size: 0.6875rem;
  letter-spacing: 0.4em;
  line-height: 1rem;
  margin: 0;
  text-transform: uppercase;
}

.workspace-placeholder__title {
  color: var(--color-neutral-900);
  font-size: var(--text-2xl);
  font-weight: 600;
  line-height: 2rem;
  margin: 1rem 0 0;
}

.workspace-placeholder__description {
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
  line-height: 1.25rem;
  margin: 0.5rem 0 0;
  max-width: 28rem;
}

.dark .workspace-placeholder {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

.dark .workspace-placeholder__tag,
.dark .workspace-placeholder__description {
  color: var(--color-neutral-400);
}

.dark .workspace-placeholder__title {
  color: var(--color-neutral-100);
}
</style>
