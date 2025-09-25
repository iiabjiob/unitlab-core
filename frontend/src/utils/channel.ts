// utils/channel.ts

/** Ограничивает значение в диапазоне 4..20 мА (или 0..24, если хочешь расширенный режим) */
export function clampAoValue(value: number, min = 4, max = 20): number {
  if (isNaN(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

/** Приводит значение к строке с двумя знаками после запятой */
export function formatAoValue(value: number): string {
  return clampAoValue(value).toFixed(2)
}

/** Парсинг строки из input → нормализованное число */
export function parseAoInput(raw: string, min = 4, max = 20): number {
  const num = parseFloat(raw)
  return clampAoValue(isNaN(num) ? min : num, min, max)
}
