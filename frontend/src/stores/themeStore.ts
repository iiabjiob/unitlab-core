// src/stores/themeStore.ts
import { defineStore } from "pinia"
import { ref, computed } from "vue"

import {
  LOCAL_SETTINGS_STORAGE_KEY,
  localSettingsKeys,
  readLocalSetting,
  writeLocalSetting,
} from "@/services/localSettingsStorage"

export type ThemeMode = "light" | "dark" | "auto"

const LEGACY_THEME_MODE_KEY = "themeMode"

export const useThemeStore = defineStore("themeStore", () => {
  const supported = typeof window !== "undefined" && typeof document !== "undefined"

  const mode = ref<ThemeMode>(supported ? readThemeMode() : "auto")

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
    writeLocalSetting(localSettingsKeys.themeMode, newMode, {
      legacyKeys: [LEGACY_THEME_MODE_KEY],
    })
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
      if (e.key === LOCAL_SETTINGS_STORAGE_KEY || e.key === LEGACY_THEME_MODE_KEY) {
        mode.value = readThemeMode()
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

function readThemeMode(): ThemeMode {
  return readLocalSetting<ThemeMode>(localSettingsKeys.themeMode, "auto", {
    legacyKeys: [LEGACY_THEME_MODE_KEY],
    parseLegacy: raw => raw,
    validate: value => isThemeMode(value) ? value : null,
  })
}

function isThemeMode(value: unknown): value is ThemeMode {
  return value === "light" || value === "dark" || value === "auto"
}
