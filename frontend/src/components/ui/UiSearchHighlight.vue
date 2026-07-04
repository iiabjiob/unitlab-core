<template>
  <span class="ui-search-highlight">
    <template v-for="(segment, index) in segments" :key="`${index}:${segment.matched ? '1' : '0'}:${segment.text}`">
      <span
        v-if="segment.matched"
        class="ui-search-highlight__segment ui-search-highlight__segment--matched"
      >
        {{ segment.text }}
      </span>
      <span v-else class="ui-search-highlight__segment">
        {{ segment.text }}
      </span>
    </template>
  </span>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { splitSearchHighlightText } from "@/utils/searchHighlight"

const props = withDefaults(defineProps<{
  text?: string | null
  query?: string | null
}>(), {
  text: "",
  query: "",
})

const segments = computed(() => splitSearchHighlightText(props.text ?? "", props.query ?? ""))
</script>

<style scoped>
.ui-search-highlight {
  display: inline;
}

.ui-search-highlight__segment {
  color: inherit;
}

.ui-search-highlight__segment--matched {
  background: color-mix(in srgb, var(--runtime-accent, var(--color-blue-500)) 18%, transparent);
  border-radius: 0.2rem;
  color: inherit;
  font-weight: 700;
  padding: 0 0.1rem;
  -webkit-box-decoration-break: clone;
  box-decoration-break: clone;
}

:global(.dark) .ui-search-highlight__segment--matched {
  background: color-mix(in srgb, var(--runtime-accent, var(--color-blue-400)) 24%, transparent);
}
</style>
