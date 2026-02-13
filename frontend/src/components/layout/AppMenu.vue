<template>
  <nav
    ref="navRef"
    class="flex-1 px-2 py-4 space-y-4 focus:outline-none"
    tabindex="0"
    role="listbox"
    aria-label="Primary navigation"
    :aria-activedescendant="activeDescendantId ?? undefined"
    @keydown="handleKeydown"
    @focus="handleNavFocus"
  >
    <div v-for="section in sections" :key="section.title" class="space-y-1">
      <p class="px-2 text-[11px] font-semibold uppercase tracking-wide text-neutral-400">
        {{ section.title }}
      </p>
      <div v-for="item in section.items" :key="item.to">
        <button
          :id="entryDomId(item.to)"
          type="button"
          role="option"
          :aria-selected="isRouteActive(item.to)"
          :tabindex="isEntryFocused(item.to) ? 0 : -1"
          class="block w-full rounded-xl pl-6 pr-3 py-2 text-left text-sm font-medium transition-all focus:outline-none"
          :class="entryClasses(item.to, false)"
          @focus="setFocusByRoute(item.to)"
          @click="activateRoute(item.to)"
        >
          {{ item.label }}
        </button>
        <button
          v-for="child in item.children ?? []"
          :id="entryDomId(child.to)"
          :key="child.to"
          type="button"
          role="option"
          :aria-selected="isRouteActive(child.to)"
          :tabindex="isEntryFocused(child.to) ? 0 : -1"
          class="mt-1 block w-full rounded-xl pl-10 pr-3 py-2 text-left text-sm font-medium transition-all focus:outline-none"
          :class="entryClasses(child.to, true)"
          @focus="setFocusByRoute(child.to)"
          @click="activateRoute(child.to)"
        >
          {{ child.label }}
        </button>
      </div>
    </div>
  </nav>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

type MenuItem = {
  to: string
  label: string
  children?: MenuItem[]
}

type MenuSection = {
  title: string
  items: MenuItem[]
}

const sections: MenuSection[] = [
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
      {
        to: "/test-runs",
        label: "Test Runs",
        children: [{ to: "/test-runs/instructions", label: "Instructions" }],
      },
    ],
  },
]

const route = useRoute()
const router = useRouter()
const navRef = ref<HTMLElement | null>(null)
const focusedRoute = ref<string | null>(null)

const menuEntries = computed(() =>
  sections.flatMap(section =>
    section.items.flatMap(item => [item, ...(item.children ?? [])]),
  ),
)

const activeRoute = computed(() => {
  const currentPath = route.path
  const exact = menuEntries.value.find(entry => currentPath === entry.to)
  if (exact) {
    return exact.to
  }
  const prefixMatch = menuEntries.value.find(entry => currentPath.startsWith(entry.to + "/"))
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
  },
  { immediate: true },
)

function entryDomId(to: string): string {
  return `app-menu-option-${to.replace(/[^a-zA-Z0-9_-]/g, "-")}`
}

function isRouteActive(to: string): boolean {
  const currentPath = route.path
  return currentPath === to || currentPath.startsWith(to + "/")
}

function isEntryFocused(to: string): boolean {
  return focusedRoute.value === to
}

function setFocusByRoute(to: string) {
  focusedRoute.value = to
}

function entryClasses(to: string, isChild: boolean): string {
  const active = isRouteActive(to)
  if (active) {
    return "bg-neutral-100 text-neutral-900 dark:bg-neutral-800 dark:text-white"
  }
  return isChild
    ? "text-neutral-600 hover:bg-neutral-100 hover:text-neutral-900 dark:text-neutral-300 dark:hover:bg-neutral-800/70 dark:hover:text-white"
    : "text-neutral-700 hover:bg-neutral-100 hover:text-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800/70 dark:hover:text-white"
}

function moveFocus(delta: number) {
  const total = menuEntries.value.length
  if (!total) return
  const base = focusedIndex.value < 0 ? 0 : focusedIndex.value
  const next = Math.max(0, Math.min(total - 1, base + delta))
  focusedRoute.value = menuEntries.value[next].to
  nextTick(() => {
    const el = document.getElementById(entryDomId(menuEntries.value[next].to)) as HTMLButtonElement | null
    el?.focus({ preventScroll: true })
  })
}

function activateRoute(to: string) {
  if (route.path === to) return
  void router.push(to)
}

function handleNavFocus() {
  if (!menuEntries.value.length) return
  if (!focusedRoute.value) {
    focusedRoute.value = activeRoute.value ?? menuEntries.value[0].to
  }
  nextTick(() => {
    const target = focusedRoute.value
    if (!target) return
    const el = document.getElementById(entryDomId(target)) as HTMLButtonElement | null
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
