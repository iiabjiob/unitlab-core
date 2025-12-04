export function measureDomElements(container: HTMLElement): number {
  try {
    return container.getElementsByTagName("*").length
  } catch {
    return 0
  }
}
