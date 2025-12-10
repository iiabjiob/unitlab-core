// submenuContext.ts
import type { InjectionKey, Ref } from "vue"

export interface UiSubMenuContext {
  open: Ref<boolean>
  parentItemEl: Ref<HTMLElement | null>
  contentEl: Ref<HTMLElement | null>

  openMenu: () => void
  closeMenu: () => void
  position: () => void

  scheduleClose: () => void
  cancelClose: () => void
}

export const UI_SUBMENU_KEY: InjectionKey<UiSubMenuContext> =
  Symbol("ui-submenu")
