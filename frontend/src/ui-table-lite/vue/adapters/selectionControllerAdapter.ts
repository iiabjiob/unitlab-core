import { onScopeDispose, shallowRef } from "vue"
import type { ShallowRef } from "vue"
import {
	createSelectionController,
	type SelectionController,
	type SelectionControllerListener,
} from "@/ui-table/core/selection/headlessSelectionController"
import type {
	SelectionEnvironment,
	SelectionOverlaySnapshot,
	SelectionRange,
} from "@/ui-table/core/selection/selectionEnvironment"
import type { GridSelectionContext } from "@/ui-table/core/selection/selectionState"
import type { HeadlessSelectionState } from "@/ui-table/core/selection/update"
import type { PointerCoordinates } from "@/ui-table/core/selection/autoScroll"

export interface SelectionControllerAdapterOptions<RowKey> {
	environment: SelectionEnvironment<RowKey>
	context: GridSelectionContext<RowKey>
	initialState?: HeadlessSelectionState<RowKey>
}

export interface SelectionControllerAdapter<RowKey> {
	controller: SelectionController<RowKey>
	state: ShallowRef<HeadlessSelectionState<RowKey>>
	overlaySnapshot: ShallowRef<SelectionOverlaySnapshot | null>
	fillHandleRange: ShallowRef<SelectionRange<RowKey> | null>
	autoscrollState: ShallowRef<{ active: boolean; pointer: PointerCoordinates | null }>
	subscribe(listener: SelectionControllerListener<RowKey>): () => void
	dispose(): void
}

export function createSelectionControllerAdapter<RowKey>(
	options: SelectionControllerAdapterOptions<RowKey>,
): SelectionControllerAdapter<RowKey> {
	const controller = createSelectionController<RowKey>(options)
	const state = shallowRef(controller.getState())
	const overlaySnapshot = shallowRef<SelectionOverlaySnapshot | null>(null)
	const fillHandleRange = shallowRef<SelectionRange<RowKey> | null>(null)
	const autoscrollState = shallowRef<{ active: boolean; pointer: PointerCoordinates | null }>({
		active: false,
		pointer: null,
	})

	const listeners = new Set<SelectionControllerListener<RowKey>>()
	let disposed = false

	const controllerUnsubscribe = controller.subscribe(event => {
		if (disposed) {
			return
		}

		if (event.type === "selection-change") {
			state.value = event.state
		} else if (event.type === "overlay-update") {
			overlaySnapshot.value = event.snapshot
		} else if (event.type === "fill-handle-change") {
			fillHandleRange.value = event.range
		} else if (event.type === "autoscroll") {
			autoscrollState.value = event.active
				? { active: true, pointer: event.pointer }
				: { active: false, pointer: null }
		}

		if (listeners.size) {
			for (const listener of listeners) {
				listener(event)
			}
		}
	})

	const subscribe = (listener: SelectionControllerListener<RowKey>) => {
		if (disposed) {
			return () => {}
		}
		listeners.add(listener)
		return () => {
			listeners.delete(listener)
		}
	}

	const dispose = () => {
		if (disposed) {
			return
		}
		disposed = true
		controllerUnsubscribe()
		listeners.clear()
		controller.dispose()
	}

	onScopeDispose(dispose)

	return {
		controller,
		state,
		overlaySnapshot,
		fillHandleRange,
		autoscrollState,
		subscribe,
		dispose,
	}
}
