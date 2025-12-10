// submenuContext.ts
import type { InjectionKey, Ref } from "vue"

export interface UiSubMenuContext {
  /** Submenu-specific open state (decoupled from root popover). */
  open: Ref<boolean>
  /** Owning trigger element (used for focus handoffs when collapsing). */
  parentItemEl: Ref<HTMLElement | null>
  /** Floating panel element used for hit testing + positioning. */
  contentEl: Ref<HTMLElement | null>

  openMenu: () => void
  closeMenu: () => void
  position: () => void

  /** Debounce helpers so pointer-leave closures feel natural. */
  scheduleClose: () => void
  cancelClose: () => void
}

// Symbol ensures each submenu instance provides its own context, even when nested deeply.
export const UI_SUBMENU_KEY: InjectionKey<UiSubMenuContext> =
  Symbol("ui-submenu")
