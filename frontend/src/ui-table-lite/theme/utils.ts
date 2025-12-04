// src/ui-table/theme/utils.ts

import { THEME_TOKEN_VARIABLE_MAP } from "./tokens"
import type { UiTableThemeTokens } from "./types"

/** Apply CSS custom properties for the given theme to a DOM element */
export function applyTableTheme(el: HTMLElement, tokens: UiTableThemeTokens) {
  for (const [key, cssVar] of Object.entries(THEME_TOKEN_VARIABLE_MAP)) {
    const value = tokens[key as keyof UiTableThemeTokens]
    if (value != null) el.style.setProperty(cssVar, value)
  }
}

/** Merge two token sets (useful for partial overrides) */
export function mergeThemeTokens(
  base: UiTableThemeTokens,
  override?: Partial<UiTableThemeTokens>
): UiTableThemeTokens {
  return { ...base, ...override }
}
