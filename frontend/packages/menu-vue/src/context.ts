import { inject, provide, shallowRef } from "vue"
import type { InjectionKey, Ref, ShallowRef } from "vue"
import type { MenuCore, MenuState, Rect, SubmenuCore } from "@workspace/menu-core"

export interface MenuContextValue {
  core: MenuCore
  state: ShallowRef<MenuState>
  triggerRef: Ref<HTMLElement | null>
  panelRef: Ref<HTMLElement | null>
  anchorOverride: ShallowRef<Rect | null>
  parentMenuId: string | null
  rootMenuId: string
}

export interface SubmenuContextValue {
  submenuItemId: string
  submenu: {
    core: SubmenuCore
    state: ShallowRef<MenuState>
  }
  parent: MenuContextValue
  triggerRef: Ref<HTMLElement | null>
  panelRef: Ref<HTMLElement | null>
  anchorOverride: ShallowRef<Rect | null>
  parentSubmenu: SubmenuContextValue | null
}

const MENU_CTX_KEY: InjectionKey<MenuContextValue> = Symbol("menu-context")
const SUBMENU_CTX_KEY: InjectionKey<SubmenuContextValue> = Symbol("submenu-context")

export function provideMenuContext(value: MenuContextValue) {
  provide(MENU_CTX_KEY, value)
}

export function useMenuContext(): MenuContextValue {
  const ctx = inject(MENU_CTX_KEY)
  if (!ctx) {
    throw new Error("Menu components must be used inside <UiMenu>")
  }
  return ctx
}

export function provideSubmenuContext(value: SubmenuContextValue) {
  provide(SUBMENU_CTX_KEY, value)
}

export function useSubmenuContext(): SubmenuContextValue {
  const ctx = inject(SUBMENU_CTX_KEY)
  if (!ctx) {
    throw new Error("Submenu components must be nested inside <UiSubMenu>")
  }
  return ctx
}

export function useOptionalSubmenuContext(): SubmenuContextValue | null {
  return inject(SUBMENU_CTX_KEY, null)
}
