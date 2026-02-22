declare module "@affino/menu-vue" {
  import type { DefineComponent } from "vue"
  import type {
    Alignment,
    MenuCallbacks,
    MenuOptions,
    Placement,
  } from "@affino/menu-core"

  export type { Alignment, MenuCallbacks, MenuOptions, Placement }

  export interface MenuController {
    readonly id: string
    readonly state: {
      readonly open: boolean
    }
    readonly open: (reason?: "pointer" | "keyboard" | "programmatic") => void
    readonly close: (reason?: "pointer" | "keyboard" | "programmatic") => void
    readonly toggle: () => void
    readonly highlight: (id: string | null) => void
    readonly select: (id: string) => void
    readonly setAnchor: (rect: unknown) => void
    readonly dispose: () => void
  }

  export const UiMenu: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiMenuTrigger: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiMenuContent: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiMenuItem: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiMenuLabel: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiMenuSeparator: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiSubMenu: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiSubMenuTrigger: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const UiSubMenuContent: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
}
