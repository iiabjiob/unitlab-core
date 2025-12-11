import { MenuCore } from "./MenuCore"
import { MousePrediction } from "./MousePrediction"
import type {
  ItemProps,
  MenuCallbacks,
  MenuOptions,
  PanelProps,
  PointerEventLike,
  Rect,
  TriggerProps,
} from "./Types"

interface SubmenuOptions extends MenuOptions {
  parentItemId: string
}

export class SubmenuCore extends MenuCore {
  private readonly parent: MenuCore
  private readonly parentItemId: string
  private readonly predictor: MousePrediction
  private triggerRect: Rect | null = null
  private panelRect: Rect | null = null
  private parentSubscription: { unsubscribe: () => void } | null = null

  constructor(parent: MenuCore, options: SubmenuOptions, callbacks: MenuCallbacks = {}) {
    super(options, callbacks)
    this.parent = parent
    this.parentItemId = options.parentItemId
    this.predictor = new MousePrediction(options.mousePrediction)
    this.parentSubscription = parent.subscribe((state) => {
      if (!state.open || state.activeItemId !== this.parentItemId) {
        this.close("programmatic")
      }
    })
  }

  override destroy() {
    super.destroy()
    this.parentSubscription?.unsubscribe()
    this.parentSubscription = null
  }

  setTriggerRect(rect: Rect | null) {
    this.triggerRect = rect
  }

  setPanelRect(rect: Rect | null) {
    this.panelRect = rect
  }

  recordPointer(point: { x: number; y: number }) {
    this.predictor.push(point)
  }

  override getTriggerProps(): TriggerProps {
    const base = super.getItemProps(this.parentItemId)
    return {
      id: base.id,
      role: "button",
      tabIndex: base.tabIndex,
      "aria-haspopup": "menu",
      "aria-expanded": this.state.open,
      "aria-controls": `${this.id}-panel`,
      onPointerEnter: (event) => {
        this.parent.highlight(this.parentItemId)
        this.parent.cancelClose()
        this.cancelClose()
        this.scheduleOpen()
        this.capturePointer(event)
      },
      onPointerLeave: (event) => this.handlePointerLeave(event),
      onClick: (event: any) => {
        event.preventDefault?.()
        this.open("pointer")
      },
      onKeyDown: (event) => this.handleNestedTriggerKeydown(event),
    }
  }

  override getPanelProps(): PanelProps {
    const props = super.getPanelProps()
    return {
      ...props,
      onPointerEnter: (event) => {
        this.cancelClose()
        this.parent.cancelClose()
        this.capturePointer(event)
      },
      onPointerLeave: (event) => this.handlePointerLeave(event),
    }
  }

  override getItemProps(id: string): ItemProps {
    const props = super.getItemProps(id)
    return {
      ...props,
      onPointerEnter: (event) => {
        props.onPointerEnter?.(event)
        this.capturePointer(event)
      },
    }
  }

  private handlePointerLeave(event?: PointerEventLike) {
    this.capturePointer(event)

    if (event?.meta?.isInsidePanel) {
      this.cancelClose()
      return
    }

    if (event?.meta?.enteredChildPanel) {
      this.cancelClose()
      return
    }

    if (this.triggerRect && this.panelRect && this.predictor.isMovingToward(this.panelRect, this.triggerRect)) {
      this.cancelClose()
      return
    }

    this.scheduleClose()
  }

  private handleNestedTriggerKeydown(event: KeyboardEvent) {
    if (event.key === "ArrowRight" || event.key === "Enter" || event.key === " ") {
      event.preventDefault()
      this.open("keyboard")
      this.ensureInitialHighlight()
      return
    }

    if (event.key === "ArrowLeft") {
      event.preventDefault()
      this.close("keyboard")
      this.parent.highlight(this.parentItemId)
    }
  }

  private capturePointer(event?: PointerEventLike) {
    if (!event || event.clientX == null || event.clientY == null) return
    this.recordPointer({ x: event.clientX, y: event.clientY })
  }
}
