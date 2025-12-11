import { computePosition } from "./Positioning"
import type {
  EventHandler,
  ItemProps,
  MenuCallbacks,
  MenuOptions,
  MenuState,
  PanelProps,
  Rect,
  Subscription,
  TriggerProps,
} from "./Types"

let idCounter = 0

interface ItemRecord {
  id: string
  disabled: boolean
}

export class MenuCore {
  readonly id: string
  protected readonly options: Required<MenuOptions>
  protected readonly callbacks: MenuCallbacks
  protected readonly items: ItemRecord[] = []
  protected readonly subscribers = new Set<(state: MenuState) => void>()

  protected openTimer: ReturnType<typeof setTimeout> | null = null
  protected closeTimer: ReturnType<typeof setTimeout> | null = null

  protected state: MenuState = {
    open: false,
    activeItemId: null,
  }
  protected pendingInitialHighlight = false

  constructor(options: MenuOptions = {}, callbacks: MenuCallbacks = {}) {
    this.id = options.id ?? `menu-${++idCounter}`
    this.options = {
      openDelay: options.openDelay ?? 80,
      closeDelay: options.closeDelay ?? 150,
      closeOnSelect: options.closeOnSelect ?? true,
      loopFocus: options.loopFocus ?? true,
      mousePrediction: options.mousePrediction ?? {},
      id: this.id,
    }
    this.callbacks = callbacks
  }

  destroy() {
    this.clearTimers()
    this.subscribers.clear()
    this.items.length = 0
  }

  getSnapshot(): MenuState {
    return { ...this.state }
  }

  subscribe(listener: (state: MenuState) => void): Subscription {
    this.subscribers.add(listener)
    listener(this.getSnapshot())
    return {
      unsubscribe: () => {
        this.subscribers.delete(listener)
      },
    }
  }

  protected emit() {
    const snapshot = this.getSnapshot()
    this.subscribers.forEach((cb) => cb(snapshot))
  }

  open(reason: "pointer" | "keyboard" | "programmatic" = "programmatic") {
    if (this.state.open) return
    this.clearTimers()
    this.state.open = true
    this.callbacks.onOpen?.(this.id)
    this.emit()
  }

  close(reason: "pointer" | "keyboard" | "programmatic" = "programmatic") {
    if (!this.state.open) return
    this.clearTimers()
    this.state.open = false
    this.state.activeItemId = null
    this.pendingInitialHighlight = false
    this.callbacks.onClose?.(this.id)
    this.emit()
  }

  toggle() {
    this.state.open ? this.close("programmatic") : this.open("programmatic")
  }

  scheduleOpen() {
    if (this.openTimer) clearTimeout(this.openTimer)
    this.openTimer = setTimeout(() => this.open("pointer"), this.options.openDelay)
  }

  scheduleClose() {
    if (this.closeTimer) clearTimeout(this.closeTimer)
    this.closeTimer = setTimeout(() => this.close("pointer"), this.options.closeDelay)
  }

  cancelClose() {
    if (this.closeTimer) {
      clearTimeout(this.closeTimer)
      this.closeTimer = null
    }
  }

  cancelOpen() {
    if (this.openTimer) {
      clearTimeout(this.openTimer)
      this.openTimer = null
    }
  }

  protected clearTimers() {
    if (this.openTimer) {
      clearTimeout(this.openTimer)
      this.openTimer = null
    }
    if (this.closeTimer) {
      clearTimeout(this.closeTimer)
      this.closeTimer = null
    }
  }

  registerItem(id: string, options: { disabled?: boolean } = {}) {
    const existing = this.items.find((item) => item.id === id)
    if (existing) {
      existing.disabled = Boolean(options.disabled)
      this.maybeHighlightOnRegister(existing)
    } else {
      const record = { id, disabled: Boolean(options.disabled) }
      this.items.push(record)
      this.maybeHighlightOnRegister(record)
    }

    return () => {
      const index = this.items.findIndex((item) => item.id === id)
      if (index >= 0) this.items.splice(index, 1)
      if (this.state.activeItemId === id) {
        this.state.activeItemId = null
        this.emit()
      }
    }
  }

  highlight(id: string | null) {
    if (!id) {
      if (this.state.activeItemId !== null) {
        this.state.activeItemId = null
        this.emit()
      }
      return
    }

    const item = this.items.find((entry) => entry.id === id && !entry.disabled)
    if (!item) return
    if (this.state.activeItemId === id) return
    this.state.activeItemId = id
    this.pendingInitialHighlight = false
    this.emit()
  }

  protected ensureInitialHighlight() {
    if (!this.state.open) return
    if (this.state.activeItemId) return
    const first = this.items.find((item) => !item.disabled)
    if (first) {
      this.highlight(first.id)
      return
    }
    this.pendingInitialHighlight = true
  }

  protected maybeHighlightOnRegister(record: ItemRecord) {
    if (
      !this.pendingInitialHighlight ||
      !this.state.open ||
      this.state.activeItemId !== null ||
      record.disabled
    ) {
      return
    }
    this.pendingInitialHighlight = false
    this.highlight(record.id)
  }

  select(id: string) {
    const item = this.items.find((entry) => entry.id === id && !entry.disabled)
    if (!item) return
    this.callbacks.onSelect?.(id, this.id)
    if (this.options.closeOnSelect) {
      this.close("programmatic")
    }
  }

  moveFocus(delta: 1 | -1) {
    if (!this.items.length) return
    const activeIndex = this.state.activeItemId
      ? this.items.findIndex((item) => item.id === this.state.activeItemId)
      : -1

    let nextIndex = activeIndex
    for (let i = 0; i < this.items.length; i += 1) {
      nextIndex = nextIndex + delta
      if (nextIndex >= this.items.length) {
        if (!this.options.loopFocus) return
        nextIndex = 0
      }
      if (nextIndex < 0) {
        if (!this.options.loopFocus) return
        nextIndex = this.items.length - 1
      }
      const candidate = this.items[nextIndex]
      if (!candidate.disabled) {
        this.highlight(candidate.id)
        return
      }
    }
  }

  getTriggerProps(): TriggerProps {
    return {
      id: `${this.id}-trigger`,
      role: "button",
      tabIndex: 0,
      "aria-haspopup": "menu",
      "aria-expanded": this.state.open,
      "aria-controls": `${this.id}-panel`,
      onPointerEnter: () => this.scheduleOpen(),
      onPointerLeave: () => this.scheduleClose(),
      onClick: () => this.toggle(),
      onKeyDown: this.handleTriggerKeydown,
    }
  }

  getPanelProps(): PanelProps {
    return {
      id: `${this.id}-panel`,
      role: "menu",
      tabIndex: -1,
      "aria-labelledby": `${this.id}-trigger`,
      onKeyDown: this.handlePanelKeydown,
      onPointerEnter: () => this.cancelClose(),
      onPointerLeave: () => this.scheduleClose(),
    }
  }

  getItemProps(id: string): ItemProps {
    const highlighted = this.state.activeItemId === id
    const disabled = this.items.find((item) => item.id === id)?.disabled ?? false
    return {
      id,
      role: "menuitem",
      tabIndex: highlighted ? 0 : -1,
      "aria-disabled": disabled ? true : undefined,
      "data-state": highlighted ? "highlighted" : "idle",
      onPointerEnter: () => this.highlight(disabled ? null : id),
      onClick: (event) => {
        if (disabled) {
          event.preventDefault?.()
          return
        }
        this.select(id)
      },
      onKeyDown: (event) => this.handleItemKeydown(event, id, disabled),
    }
  }

  computePosition(anchor: Rect, panel: Rect, options = {}) {
    const position = computePosition(anchor, panel, options)
    this.callbacks.onPositionChange?.(this.id, position)
    return position
  }

  protected handleTriggerKeydown: EventHandler<KeyboardEvent> = (event) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault()
      this.toggle()
      return
    }

    if (event.key === "ArrowDown") {
      event.preventDefault()
      this.open("keyboard")
      const first = this.items.find((item) => !item.disabled)
      this.highlight(first ? first.id : null)
      return
    }

    if (event.key === "ArrowUp") {
      event.preventDefault()
      this.open("keyboard")
      const reversed = [...this.items].reverse().find((item) => !item.disabled)
      this.highlight(reversed ? reversed.id : null)
    }
  }

  protected handlePanelKeydown: EventHandler<KeyboardEvent> = (event) => {
    if (event.key === "Escape") {
      event.preventDefault()
      this.close("keyboard")
      return
    }

    if (event.key === "ArrowDown") {
      event.preventDefault()
      this.moveFocus(1)
      return
    }

    if (event.key === "ArrowUp") {
      event.preventDefault()
      this.moveFocus(-1)
      return
    }

    if (event.key === "Home") {
      event.preventDefault()
      const first = this.items.find((item) => !item.disabled)
      this.highlight(first ? first.id : null)
      return
    }

    if (event.key === "End") {
      event.preventDefault()
      const last = [...this.items].reverse().find((item) => !item.disabled)
      this.highlight(last ? last.id : null)
      return
    }

    if (event.key === "Tab") {
      event.preventDefault()
      return
    }

    if (event.key === "Enter" || event.key === " ") {
      const current = this.state.activeItemId
      if (current) {
        event.preventDefault()
        this.select(current)
      }
    }
  }

  protected handleItemKeydown(event: KeyboardEvent, id: string, disabled: boolean) {
    if (disabled) {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault()
      }
      return
    }

    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault()
      this.select(id)
      return
    }

    if (event.key === "ArrowUp") {
      event.preventDefault()
      this.moveFocus(-1)
      return
    }

    if (event.key === "ArrowDown") {
      event.preventDefault()
      this.moveFocus(1)
    }
  }
}
