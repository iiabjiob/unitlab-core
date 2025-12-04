export interface ClipboardAdapter {
  writeText(value: string): Promise<void>
  readText(): Promise<string>
}

export interface MeasurementScheduler {
  measure<T>(callback: () => T): Promise<T>
}

export interface FrameScheduler {
  schedule(callback: () => void): number
  cancel(handle: number): void
}

export interface HoverDelegationEnvironment {
  measurement: MeasurementScheduler
  frame: FrameScheduler
}
