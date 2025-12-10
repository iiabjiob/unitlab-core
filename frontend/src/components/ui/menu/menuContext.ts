// File: menuContext.ts
import type { InjectionKey, Ref } from "vue"

export interface UiMenuContext {
  open: Ref<boolean>
  menuStyle: Ref<Record<string, string>>
  triggerEl: Ref<HTMLElement | null>
  contentEl: Ref<HTMLElement | null>
  position: () => void
  openFromTrigger: () => void
  openAtCursor: (e: MouseEvent) => void
  toggleFromTrigger: () => void
  close: () => void
}

export const UI_MENU_KEY: InjectionKey<UiMenuContext> = Symbol("uiMenu")
