import type { SelectionOverlayRect } from "./selectionOverlay"

export type MutableSelectionOverlayRect = SelectionOverlayRect & {
	id: string
	left: number
	top: number
	width: number
	height: number
	active?: boolean
	pin?: "left" | "none" | "right"
}

function createRect(): MutableSelectionOverlayRect {
	return {
		id: "",
		left: 0,
		top: 0,
		width: 0,
		height: 0,
		active: undefined,
		pin: undefined,
	}
}

function resetRect(rect: MutableSelectionOverlayRect): void {
	rect.id = ""
	rect.left = 0
	rect.top = 0
	rect.width = 0
	rect.height = 0
	rect.active = undefined
	rect.pin = undefined
}

export class SelectionOverlayRectPool {
	private readonly pool: MutableSelectionOverlayRect[] = []

	acquire(size = 1): MutableSelectionOverlayRect[] {
		if (size <= 0) {
			return []
		}
		const result: MutableSelectionOverlayRect[] = []
		for (let index = 0; index < size; index += 1) {
			result.push(this.acquireOne())
		}
		return result
	}

	single(): MutableSelectionOverlayRect {
		return this.acquireOne()
	}

	release(rect: SelectionOverlayRect | null | undefined): void {
		if (!rect) {
			return
		}
		const mutable = rect as MutableSelectionOverlayRect
		resetRect(mutable)
		this.pool.push(mutable)
	}

	releaseAll(rects: readonly SelectionOverlayRect[] | null | undefined): void {
		if (!rects || rects.length === 0) {
			return
		}
		for (let index = 0; index < rects.length; index += 1) {
			const rect = rects[index]
			if (rect) {
				this.release(rect)
			}
		}
	}

	clear(): void {
		this.pool.length = 0
	}

	private acquireOne(): MutableSelectionOverlayRect {
		const rect = this.pool.pop()
		return rect ? rect : createRect()
	}
}

export const selectionOverlayRectPool = new SelectionOverlayRectPool()

class SelectionOverlayRectArrayPool {
	private readonly pool: MutableSelectionOverlayRect[][] = []

	acquire(): MutableSelectionOverlayRect[] {
		const array = this.pool.pop()
		if (array) {
			array.length = 0
			return array
		}
		return []
	}

	release(array: MutableSelectionOverlayRect[]): void {
		array.length = 0
		this.pool.push(array)
	}

	clear(): void {
		this.pool.length = 0
	}
}

const selectionOverlayRectArrayPool = new SelectionOverlayRectArrayPool()

export function acquireOverlayRect(): MutableSelectionOverlayRect {
	return selectionOverlayRectPool.single()
}

export function acquireOverlayRectBatch(size: number): MutableSelectionOverlayRect[] {
	return selectionOverlayRectPool.acquire(size)
}

export function acquireOverlayRectArray(): MutableSelectionOverlayRect[] {
	return selectionOverlayRectArrayPool.acquire()
}

export function releaseOverlayRect(rect: SelectionOverlayRect | null | undefined): void {
	selectionOverlayRectPool.release(rect)
}

export function releaseOverlayRectList(rects: readonly SelectionOverlayRect[] | null | undefined): void {
	releaseOverlayRectArray(rects as SelectionOverlayRect[] | null | undefined)
}

export function resetOverlayRectPool(): void {
	selectionOverlayRectPool.clear()
	selectionOverlayRectArrayPool.clear()
}

export function releaseOverlayRectArray(
	rects: SelectionOverlayRect[] | null | undefined,
): void {
	if (!rects) {
		return
	}
	const mutable = rects as MutableSelectionOverlayRect[]
	if (mutable.length) {
		for (let index = 0; index < mutable.length; index += 1) {
			selectionOverlayRectPool.release(mutable[index])
		}
	}
	selectionOverlayRectArrayPool.release(mutable)
}
