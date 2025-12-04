export interface WritableSignal<T> {
  value: T
}

export type CreateWritableSignal<T> = (initial: T) => WritableSignal<T>

export function createWritableSignal<T>(initial: T): WritableSignal<T> {
  return { value: initial }
}
