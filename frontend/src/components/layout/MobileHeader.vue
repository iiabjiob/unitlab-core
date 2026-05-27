<template>
  <header class="mobile-header">
    <div class="mobile-header__left">
      <button
        type="button"
        class="mobile-header__menu-button"
        aria-label="Open menu"
        @pointerup.stop.prevent="$emit('open-drawer')"
        @touchend.stop.prevent="$emit('open-drawer')"
        @click.stop.prevent="$emit('open-drawer')"
      >
        <svg
          xmlns="http://www.w3.org/2000/svg"
          class="mobile-header__menu-icon"
          fill="none"
          viewBox="0 0 24 24"
          stroke="currentColor"
          stroke-width="2"
        >
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h16M4 18h16" />
        </svg>
      </button>

      <slot name="left">
      </slot>
    </div>

    <slot name="right" />
  </header>
</template>

<script setup lang="ts">
defineEmits<{
  (e: "open-drawer"): void
}>()
</script>

<style scoped>
.mobile-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid var(--color-neutral-200);
}

.mobile-header__left {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.mobile-header__menu-button {
  display: inline-flex;
  width: 2.25rem;
  height: 2.25rem;
  align-items: center;
  justify-content: center;
  border: 0;
  border-radius: 0.75rem;
  background: transparent;
  color: inherit;
}

.mobile-header__menu-button:hover {
  background: var(--color-neutral-100);
}

.mobile-header__menu-icon {
  width: 1.25rem;
  height: 1.25rem;
}

@media (min-width: 768px) {
  .mobile-header {
    display: none;
  }
}

:global(.dark .mobile-header) {
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .mobile-header__menu-button:hover) {
  background: var(--color-neutral-800);
}
</style>
