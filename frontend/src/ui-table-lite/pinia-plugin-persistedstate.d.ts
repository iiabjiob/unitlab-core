import "pinia"
import type { PiniaPluginContext, StateTree } from "pinia"

declare module "pinia" {
  interface PersistedStateSerializer {
    serialize: (value: any) => string
    deserialize: (value: string) => any
  }

  interface PersistedStateOptions {
    key?: string
    storage?: Pick<Storage, "getItem" | "setItem" | "removeItem"> | undefined
    paths?: string[]
    pick?: string[]
    omit?: string[]
    debug?: boolean
    serializer?: PersistedStateSerializer
    beforeRestore?: (context: PiniaPluginContext) => void
    afterRestore?: (context: PiniaPluginContext) => void
  }

  interface DefineStoreOptionsBase<S extends StateTree, Store> {
    persist?: boolean | PersistedStateOptions | PersistedStateOptions[]
  }
}

declare module "pinia-plugin-persistedstate" {
  export interface PersistedStateFactoryOptions extends Partial<PersistedStateOptions> {
    auto?: boolean
  }

  export function createPersistedState(options?: PersistedStateFactoryOptions): (context: PiniaPluginContext) => void
  const _default: (context: PiniaPluginContext) => void
  export default _default
}
