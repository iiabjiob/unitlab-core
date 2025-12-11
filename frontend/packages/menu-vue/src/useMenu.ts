import { onBeforeUnmount, shallowRef } from "vue"
import type { ShallowRef } from "vue"
import {
  MenuCore,
  SubmenuCore,
  type MenuCallbacks,
  type MenuOptions,
  type MenuState,
  type Subscription,
} from "@workspace/menu-core"

export interface MenuController {
  core: MenuCore
  state: ShallowRef<MenuState>
}

export function useMenu(options?: MenuOptions, callbacks?: MenuCallbacks): MenuController {
  const core = new MenuCore(options, callbacks)
  const state = shallowRef<MenuState>(core.getSnapshot())
  const subscription = core.subscribe((next) => {
    state.value = next
  })

  onBeforeUnmount(() => {
    subscription.unsubscribe()
    core.destroy()
  })

  return { core, state }
}

export interface SubmenuController {
  core: SubmenuCore
  state: ShallowRef<MenuState>
  dispose: () => void
}

export function createSubmenuController(
  parent: MenuCore,
  options: { parentItemId: string } & MenuOptions,
  callbacks?: MenuCallbacks
): SubmenuController {
  const core = new SubmenuCore(parent, options, callbacks)
  const state = shallowRef<MenuState>(core.getSnapshot())
  const subscription: Subscription = core.subscribe((next) => {
    state.value = next
  })

  const dispose = () => {
    subscription.unsubscribe()
    core.destroy()
  }

  return { core, state, dispose }
}
