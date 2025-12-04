export interface TableViewportResizeObserver {
	observe(target: Element): void
	unobserve?(target: Element): void
	disconnect(): void
}

export interface TableViewportContainerMetrics {
	clientHeight: number
	clientWidth: number
	scrollHeight: number
	scrollWidth: number
	scrollTop: number
	scrollLeft: number
}

export interface TableViewportHeaderMetrics {
	height: number
}

export interface TableViewportDomStats {
	rowLayers: number
	cells: number
	fillers: number
}

export interface TableViewportHostEnvironment {
	addScrollListener(target: EventTarget, listener: (event: Event) => void, options?: AddEventListenerOptions): void
	removeScrollListener(
		target: EventTarget,
		listener: (event: Event) => void,
		options?: boolean | EventListenerOptions,
	): void
	createResizeObserver?(callback: () => void): TableViewportResizeObserver | null
	removeResizeObserverTarget?(observer: TableViewportResizeObserver, target: Element): void
	readContainerMetrics?(target: HTMLDivElement): TableViewportContainerMetrics | null
	readHeaderMetrics?(target: HTMLElement | null): TableViewportHeaderMetrics | null
	getBoundingClientRect?(target: HTMLElement): DOMRect | null
	isEventFromContainer?(event: Event, container: HTMLElement): boolean
	normalizeScrollLeft?(target: HTMLElement): number
	queryDebugDomStats?(container: HTMLElement): TableViewportDomStats | null
}
