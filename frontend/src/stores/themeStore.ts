import { defineStore } from "pinia"
import { ref, watch, computed } from "vue"

export type ThemeMode = "light" | "dark" | "auto"

export const useThemeStore = defineStore("themeStore", () => {
  const mode = ref<ThemeMode>(
    (localStorage.getItem("themeMode") as ThemeMode) || "auto"
  )

  const mql = typeof window !== "undefined"
    ? window.matchMedia("(prefers-color-scheme: dark)")
    : null

  const currentTheme = computed<"light" | "dark">(() => {
    if (mode.value === "auto") {
      return mql?.matches ? "dark" : "light"
    }
    return mode.value
  })

  function applyTheme() {
    if (typeof document === "undefined") return
    document.documentElement.classList.toggle("dark", currentTheme.value === "dark")
  }

  function setMode(newMode: ThemeMode) {
    if (mode.value === newMode) return
    mode.value = newMode
    localStorage.setItem("themeMode", newMode)
  }

  // react to mode change
  watch(mode, applyTheme)

  // react to OS theme change
  if (mql) {
    mql.addEventListener?.("change", () => {
      if (mode.value === "auto") applyTheme()
    })
  }

  applyTheme()

  return { mode, setMode, currentTheme }
})
