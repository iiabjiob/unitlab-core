export function splitKey(key: string) {
  const parts = key.split(".")
  if (parts.length > 1) {
    return { root: parts[0], sub: parts.slice(1).join(".") }
  }
  return { root: null, sub: key }
}
