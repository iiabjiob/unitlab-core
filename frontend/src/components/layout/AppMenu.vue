<template>
  <nav
    ref="navRef"
    class="flex-1 space-y-4 focus:outline-none"
    :class="compact ? 'px-1 py-3' : 'px-2 py-4'"
    tabindex="0"
    role="listbox"
    aria-label="Primary navigation"
    :aria-activedescendant="activeDescendantId ?? undefined"
    @keydown="handleKeydown"
    @focus="handleNavFocus"
  >
    <div v-for="section in sections" :key="section.title" class="space-y-1">
      <p v-if="!compact" class="px-2 text-[11px] font-semibold uppercase tracking-wide text-neutral-400">
        {{ section.title }}
      </p>
      <div v-for="item in section.items" :key="item.to">
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
                class="app-menu__entry block w-full rounded-xl py-2 text-sm font-medium transition-all focus:outline-none"
                :class="[
                  compact ? 'px-0 text-center' : 'pl-6 pr-3 text-left',
                  {
                    'is-active': isRouteHighlighted(item.to),
                    'is-focused': !isRouteHighlighted(item.to) && isEntryFocused(item.to),
                    'is-compact': compact,
                  },
                ]"
                @focus="setFocusByRoute(item.to)"
                @click="event => handleEntryClick(event, item.to, navigate)"
              >
                <span class="inline-flex items-center" :class="compact ? 'justify-center w-full' : 'gap-2'">
                  <component :is="resolveRouteIcon(item.to)" class="h-5 w-5 shrink-0" aria-hidden="true" />
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
                class="app-menu__entry is-child mt-1 block w-full rounded-xl py-2 text-sm font-medium transition-all focus:outline-none"
                :class="[
                  compact ? 'px-0 text-center' : 'pl-10 pr-3 text-left',
                  {
                    'is-active': isRouteHighlighted(child.to),
                    'is-focused': !isRouteHighlighted(child.to) && isEntryFocused(child.to),
                    'is-compact': compact,
                  },
                ]"
                @focus="setFocusByRoute(child.to)"
                @click="event => handleEntryClick(event, child.to, navigate)"
              >
                <span class="inline-flex items-center" :class="compact ? 'justify-center w-full' : 'gap-2'">
                  <component :is="resolveRouteIcon(child.to)" class="h-5 w-5 shrink-0" aria-hidden="true" />
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
import PlaceholderUiIcon from "@/components/icons/PlaceholderUiIcon.vue"
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
const navRef = ref<HTMLElement | null>(null)
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

function resolveRouteIcon(to: string) {
  const normalized = normalizePath(to)
  if (normalized.startsWith("/devices")) {
    return PlaceholderUiIcon
  }
  if (normalized.startsWith("/signals")) {
    return PlaceholderUiIcon
  }
  if (normalized.startsWith("/switchgears")) {
    return PlaceholderUiIcon
  }
  if (normalized.startsWith("/sequences")) {
    return PlaceholderUiIcon
  }
  if (normalized.startsWith("/settings")) {
    return PlaceholderUiIcon
  }
  return PlaceholderUiIcon
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
.app-menu__entry {
  color: rgb(64 64 64);
  user-select: none;
}

.app-menu__entry.is-compact {
  min-height: 2.5rem;
}

.app-menu__entry:hover {
  background: rgb(245 245 245);
  color: rgb(23 23 23);
}

.app-menu__entry.is-child {
  color: rgb(82 82 82);
}

.app-menu__entry.is-focused {
  background: rgb(229 229 229);
  color: rgb(23 23 23);
}

.app-menu__entry.is-active {
  background: rgb(229 231 235);
  color: rgb(15 23 42);
}

.app-menu__entry:focus-visible {
  box-shadow: 0 0 0 2px rgb(59 130 246 / 40%);
}

.dark .app-menu__entry {
  color: rgb(212 212 212);
}

.dark .app-menu__entry.is-child {
  color: rgb(163 163 163);
}

.dark .app-menu__entry:hover {
  background: rgb(38 38 38 / 0.8);
  color: rgb(255 255 255);
}

.dark .app-menu__entry.is-focused {
  background: rgb(64 64 64 / 0.7);
  color: rgb(245 245 245);
}

.dark .app-menu__entry.is-active {
  background: rgb(59 130 246 / 0.2);
  color: rgb(219 234 254);
}

.dark .app-menu__entry:focus-visible {
  box-shadow: 0 0 0 2px rgb(96 165 250 / 40%);
}
</style>
