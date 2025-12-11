// src/stores/themeStore.ts
import { defineStore } from "pinia"
import { ref, computed } from "vue"

export type ThemeMode = "light" | "dark" | "auto"

export const useThemeStore = defineStore("themeStore", () => {
  const supported = typeof window !== "undefined" && typeof document !== "undefined"

  const mode = ref<ThemeMode>(
    (supported ? (localStorage.getItem("themeMode") as ThemeMode) : null) || "auto"
  )

  const systemDark = ref(
    supported ? window.matchMedia("(prefers-color-scheme: dark)").matches : false
  )

  const currentTheme = computed<"light" | "dark">(() => {
    return mode.value === "auto"
      ? (systemDark.value ? "dark" : "light")
      : mode.value
  })

  function applyTheme() {
    if (!supported) return
    document.documentElement.classList.toggle(
      "dark",
      currentTheme.value === "dark"
    )
  }

  function setMode(newMode: ThemeMode) {
    mode.value = newMode
    if (supported) {
      localStorage.setItem("themeMode", newMode)
    }
    applyTheme()
  }

  function toggle() {
    setMode(currentTheme.value === "dark" ? "light" : "dark")
  }

  function init() {
    if (!supported) return

    // Initial apply
    applyTheme()

    // OS theme changes
    const mql = window.matchMedia("(prefers-color-scheme: dark)")
    mql.addEventListener("change", (e) => {
      systemDark.value = e.matches
      if (mode.value === "auto") applyTheme()
    })

    // Sync between tabs
    window.addEventListener("storage", (e) => {
      if (e.key === "themeMode" && e.newValue) {
        mode.value = e.newValue as ThemeMode
        applyTheme()
      }
    })
  }

  return {
    mode,
    currentTheme,
    setMode,
    toggle,
    init,
  }
})
