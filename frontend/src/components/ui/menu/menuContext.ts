// File: menuContext.ts
import type { InjectionKey, Ref } from "vue"

export interface UiMenuContext {
  /** v-model style flag shared between trigger/content. */
  open: Ref<boolean>
  /** Inline style object computed by the root popper logic. */
  menuStyle: Ref<Record<string, string>>
  /** References to DOM nodes so keyboard + focus management stay in sync. */
  triggerEl: Ref<HTMLElement | null>
  contentEl: Ref<HTMLElement | null>
  /** Recalculate floating coordinates (called after size changes, scroll, resize, etc.). */
  position: () => void
  /** Helper methods so triggers can decide *how* to open without duplicating logic. */
  openFromTrigger: () => void
  openAtCursor: (e: MouseEvent) => void
  toggleFromTrigger: () => void
  close: () => void
}

// Provide a unique key so nested menus (and even portals) never accidentally read the wrong context.
export const UI_MENU_KEY: InjectionKey<UiMenuContext> = Symbol("uiMenu")
