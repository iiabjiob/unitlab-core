/**
 * Centralized MQTT topic builder for FAT-Simulator
 * Supports DO, DI, AI units
 */

// 🧩 Utility: returns the unit name as-is (placeholder for future logic if needed)
const moduleId = (unitName: string): string => unitName

// -------------------------
// ✅ Topics
// -------------------------

export const getSetTopic = (index: number, unit: string = 'do-unit-XXXX'): string =>
  `${moduleId(unit)}/set/${index}`

export const getGroupTopic = (unit: string = 'do-unit-XXXX'): string =>
  `${moduleId(unit)}/set/`

export const getStatusTopic = (index: number, unit: string = 'do-unit-XXXX'): string =>
  `${moduleId(unit)}/status/${index}`

export const getStatusBase = (unit: string = 'do-unit-XXXX'): string =>
  `${moduleId(unit)}/status/`

export const getStatusSubscription = (unit: string = 'do-unit-XXXX'): string =>
  `${moduleId(unit)}/status/#`

export const getRequestStatusTopic = (unit: string = 'do-unit-XXXX'): string =>
  `${moduleId(unit)}/get/status`
