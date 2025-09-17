import { defineStore } from "pinia"
import { ref, watch } from "vue"

export type ThemeMode = "light" | "dark" | "auto"

export const useThemeStore = defineStore("themeStore", () => {
  const mode = ref<ThemeMode>(
    (localStorage.getItem("themeMode") as ThemeMode) || "auto"
  )

  function applyTheme() {
    const root = document.documentElement
    let theme: "light" | "dark"

    if (mode.value === "auto") {
      const prefersDark = window.matchMedia("(prefers-color-scheme: dark)").matches
      theme = prefersDark ? "dark" : "light"
    } else {
      theme = mode.value
    }

    if (theme === "dark") {
      root.classList.add("dark")
    } else {
      root.classList.remove("dark")
    }
  }

  function setMode(newMode: ThemeMode) {
    mode.value = newMode
    localStorage.setItem("themeMode", newMode)
    applyTheme() // 👈 применяем сразу
  }

  // применить при старте
  applyTheme()

  // если mode меняется где-то реактивно → обновить
  watch(mode, () => applyTheme())

  return { mode, setMode }
})
