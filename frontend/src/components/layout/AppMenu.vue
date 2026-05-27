<template>
  <nav
    ref="navRef"
    class="app-menu"
    :class="{ 'app-menu--compact': compact }"
    tabindex="0"
    role="listbox"
    aria-label="Primary navigation"
    :aria-activedescendant="activeDescendantId ?? undefined"
    @keydown="handleKeydown"
    @focus="handleNavFocus"
  >
    <div v-for="section in sections" :key="section.title" class="app-menu__section">
      <p v-if="!compact" class="app-menu__section-title">
        {{ section.title }}
      </p>
      <div v-for="item in section.items" :key="item.to" class="app-menu__item">
        <UiHoverTooltip :text="item.label" :disabled="!compact" placement="right" align="center">
          <template #default="{ setTriggerRef, getTriggerProps }">
            <RouterLink :to="item.to" custom v-slot="{ href, navigate }">
              <a
                :id="entryDomId(item.to)"
                :ref="setTriggerRef"
                v-bind="getTriggerProps()"
                role="option"
                :aria-selected="isRouteActive(item.to)"
                :tabindex="isEntryFocused(item.to) ? 0 : -1"
                :href="href"
                :title="item.label"
                :aria-label="item.label"
                class="app-menu__entry"
                :class="[
                  compact ? 'app-menu__entry--compact' : 'app-menu__entry--full',
                  {
                    'is-active': isRouteHighlighted(item.to),
                    'is-focused': !isRouteHighlighted(item.to) && isEntryFocused(item.to),
                    'is-compact': compact,
                  },
                ]"
                @focus="setFocusByRoute(item.to)"
                @click="event => handleEntryClick(event, item.to, navigate)"
              >
                <span
                  class="app-menu__entry-content"
                  :class="compact ? 'app-menu__entry-content--compact' : 'app-menu__entry-content--full'"
                >
                  <span v-if="!compact">{{ item.label }}</span>
                </span>
              </a>
            </RouterLink>
          </template>
        </UiHoverTooltip>
        <RouterLink
          v-for="child in item.children ?? []"
          :key="child.to"
          :to="child.to"
          custom
          v-slot="{ href, navigate }"
        >
          <UiHoverTooltip :text="child.label" :disabled="!compact" placement="right" align="center">
            <template #default="{ setTriggerRef, getTriggerProps }">
              <a
                :id="entryDomId(child.to)"
                :ref="setTriggerRef"
                v-bind="getTriggerProps()"
                role="option"
                :aria-selected="isRouteActive(child.to)"
                :tabindex="isEntryFocused(child.to) ? 0 : -1"
                :href="href"
                :title="child.label"
                :aria-label="child.label"
                class="app-menu__entry is-child"
                :class="[
                  compact ? 'app-menu__entry--compact' : 'app-menu__entry--child-full',
                  {
                    'is-active': isRouteHighlighted(child.to),
                    'is-focused': !isRouteHighlighted(child.to) && isEntryFocused(child.to),
                    'is-compact': compact,
                  },
                ]"
                @focus="setFocusByRoute(child.to)"
                @click="event => handleEntryClick(event, child.to, navigate)"
              >
                <span
                  class="app-menu__entry-content"
                  :class="compact ? 'app-menu__entry-content--compact' : 'app-menu__entry-content--full'"
                >
                  <span v-if="!compact">{{ child.label }}</span>
                </span>
              </a>
            </template>
          </UiHoverTooltip>
        </RouterLink>
      </div>
    </div>
  </nav>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"
import { RouterLink, useRoute, useRouter } from "vue-router"
import UiHoverTooltip from "@/components/ui/UiHoverTooltip.vue"

const props = withDefaults(defineProps<{
  compact?: boolean
  includeSettings?: boolean
}>(), {
  compact: false,
  includeSettings: true,
})

type MenuItem = {
  to: string
  label: string
  children?: MenuItem[]
}

type MenuSection = {
  title: string
  items: MenuItem[]
}

const baseSections: MenuSection[] = [
  {
    title: "HARDWARE",
    items: [{ to: "/devices", label: "Devices" }],
  },
  {
    title: "DESIGN",
    items: [
      { to: "/signals", label: "Signals" },
      { to: "/switchgears", label: "Switchgears" },
    ],
  },
  {
    title: "RUN",
    items: [
      { to: "/sequences", label: "Sequences" },
    ],
  },
]

const sections = computed<MenuSection[]>(() => {
  if (props.includeSettings) {
    return [
      ...baseSections,
      {
        title: "SETTINGS",
        items: [{ to: "/settings", label: "Settings" }],
      },
    ]
  }
  return baseSections
})

const route = useRoute()
const router = useRouter()
const focusedRoute = ref<string | null>(null)
const pendingRoute = ref<string | null>(null)

const menuEntries = computed(() =>
  sections.value.flatMap(section =>
    section.items.flatMap(item => [item, ...(item.children ?? [])]),
  ),
)

const activeRoute = computed(() => {
  const currentPath = normalizePath(route.path)
  const exact = menuEntries.value.find(entry => currentPath === normalizePath(entry.to))
  if (exact) {
    return exact.to
  }
  const prefixMatch = menuEntries.value.find(entry => currentPath.startsWith(normalizePath(entry.to) + "/"))
  return prefixMatch?.to ?? menuEntries.value[0]?.to ?? null
})

const focusedIndex = computed(() => {
  if (!menuEntries.value.length) return -1
  if (focusedRoute.value) {
    const idx = menuEntries.value.findIndex(entry => entry.to === focusedRoute.value)
    if (idx >= 0) return idx
  }
  const activeIdx = menuEntries.value.findIndex(entry => entry.to === activeRoute.value)
  return activeIdx >= 0 ? activeIdx : 0
})

const activeDescendantId = computed(() => {
  const idx = focusedIndex.value
  if (idx < 0) return null
  return entryDomId(menuEntries.value[idx].to)
})

watch(
  () => route.fullPath,
  () => {
    focusedRoute.value = activeRoute.value
    pendingRoute.value = null
  },
  { immediate: true },
)

function normalizePath(path: string): string {
  if (path.length > 1 && path.endsWith("/")) {
    return path.replace(/\/+$/, "")
  }
  return path
}

function entryDomId(to: string): string {
  return `app-menu-option-${to.replace(/[^a-zA-Z0-9_-]/g, "-")}`
}

function isRouteActive(to: string): boolean {
  const currentPath = normalizePath(route.path)
  const targetPath = normalizePath(to)
  return currentPath === targetPath || currentPath.startsWith(targetPath + "/")
}

function isRouteHighlighted(to: string): boolean {
  return isRouteActive(to) || normalizePath(pendingRoute.value ?? "") === normalizePath(to)
}

function isEntryFocused(to: string): boolean {
  return focusedRoute.value === to
}

function setFocusByRoute(to: string) {
  focusedRoute.value = to
}

function moveFocus(delta: number) {
  const total = menuEntries.value.length
  if (!total) return
  const base = focusedIndex.value < 0 ? 0 : focusedIndex.value
  const next = Math.max(0, Math.min(total - 1, base + delta))
  focusedRoute.value = menuEntries.value[next].to
  nextTick(() => {
    const el = document.getElementById(entryDomId(menuEntries.value[next].to)) as HTMLAnchorElement | null
    el?.focus({ preventScroll: true })
  })
}

function activateRoute(to: string) {
  if (normalizePath(route.path) === normalizePath(to)) return
  pendingRoute.value = to
  void router.push(to).catch(() => {
    pendingRoute.value = null
  })
}

function shouldHandleInPlaceNavigation(event: MouseEvent): boolean {
  return (
    event.button === 0
    && !event.metaKey
    && !event.altKey
    && !event.ctrlKey
    && !event.shiftKey
  )
}

function handleEntryClick(event: MouseEvent, to: string, navigate: (event?: MouseEvent) => void) {
  if (normalizePath(route.path) === normalizePath(to)) {
    event.preventDefault()
    return
  }
  if (!shouldHandleInPlaceNavigation(event)) {
    return
  }
  pendingRoute.value = to
  navigate(event)
}

function handleNavFocus() {
  if (!menuEntries.value.length) return
  if (!focusedRoute.value) {
    focusedRoute.value = activeRoute.value ?? menuEntries.value[0].to
  }
  nextTick(() => {
    const target = focusedRoute.value
    if (!target) return
    const el = document.getElementById(entryDomId(target)) as HTMLAnchorElement | null
    el?.focus({ preventScroll: true })
  })
}

function handleKeydown(event: KeyboardEvent) {
  if (!menuEntries.value.length) return
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      moveFocus(1)
      return
    case "ArrowUp":
      event.preventDefault()
      moveFocus(-1)
      return
    case "Home":
      event.preventDefault()
      focusedRoute.value = menuEntries.value[0].to
      handleNavFocus()
      return
    case "End":
      event.preventDefault()
      focusedRoute.value = menuEntries.value[menuEntries.value.length - 1].to
      handleNavFocus()
      return
    case "Enter":
    case " ":
      event.preventDefault()
      if (focusedIndex.value < 0) return
      activateRoute(menuEntries.value[focusedIndex.value].to)
      return
    default:
      return
  }
}
</script>

<style scoped>
.app-menu {
  flex: 1;
  padding: 1rem 0.5rem;
  outline: none;
}

.app-menu--compact {
  padding: 0.75rem 0.25rem;
}

.app-menu__section + .app-menu__section {
  margin-top: 1rem;
}

.app-menu__section-title {
  margin: 0 0 0.25rem;
  padding: 0 0.5rem;
  color: var(--color-neutral-400);
  font-size: 0.6875rem;
  font-weight: 600;
  letter-spacing: 0;
  text-transform: uppercase;
}

.app-menu__item + .app-menu__item {
  margin-top: 0.25rem;
}

.app-menu__entry {
  display: block;
  width: 100%;
  padding-top: 0.5rem;
  padding-bottom: 0.5rem;
  border-radius: 0.75rem;
  cursor: pointer;
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
  font-weight: 500;
  outline: none;
  text-decoration: none;
  transition: background-color 0.15s ease, color 0.15s ease, box-shadow 0.15s ease;
  user-select: none;
}

.app-menu__entry--compact {
  padding-right: 0;
  padding-left: 0;
  text-align: center;
}

.app-menu__entry--full {
  padding-right: 0.75rem;
  padding-left: 1.5rem;
  text-align: left;
}

.app-menu__entry--child-full {
  padding-right: 0.75rem;
  padding-left: 2.5rem;
  text-align: left;
}

.app-menu__entry-content {
  display: inline-flex;
  align-items: center;
}

.app-menu__entry-content--compact {
  width: 100%;
  justify-content: center;
}

.app-menu__entry-content--full {
  gap: 0.5rem;
}

.app-menu__entry-icon {
  flex-shrink: 0;
  font-size: 1.5rem;
  line-height: 1;
}

.app-menu__entry.is-compact {
  min-height: 2.5rem;
}

.app-menu__entry:hover {
  background: var(--color-neutral-100);
  color: var(--color-neutral-900);
}

.app-menu__entry.is-child {
  margin-top: 0.25rem;
  color: var(--color-neutral-600);
}

.app-menu__entry.is-focused {
  background: var(--color-neutral-200);
  color: var(--color-neutral-900);
}

.app-menu__entry.is-active {
  background: #e5e7eb;
  color: rgb(15 23 42);
}

.app-menu__entry:focus-visible {
  box-shadow: 0 0 0 2px rgb(59 130 246 / 40%);
}

:global(.dark .app-menu__entry) {
  color: var(--color-neutral-300);
}

:global(.dark .app-menu__entry.is-child) {
  color: var(--color-neutral-400);
}

:global(.dark .app-menu__entry:hover) {
  background: color-mix(in srgb, var(--color-neutral-800) 80%, transparent);
  color: var(--color-white);
}

:global(.dark .app-menu__entry.is-focused) {
  background: color-mix(in srgb, var(--color-neutral-700) 70%, transparent);
  color: var(--color-neutral-100);
}

:global(.dark .app-menu__entry.is-active) {
  background: rgb(59 130 246 / 0.2);
  color: rgb(219 234 254);
}

:global(.dark .app-menu__entry:focus-visible) {
  box-shadow: 0 0 0 2px rgb(96 165 250 / 40%);
}
</style>
