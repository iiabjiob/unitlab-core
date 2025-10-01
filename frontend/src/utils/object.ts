// src/utils/object.ts
export function getValueByPath(obj: any, path: string): any {
  return path.split(".").reduce((acc, key) => acc?.[key], obj)
}
