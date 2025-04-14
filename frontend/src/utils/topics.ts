/**
 * Centralized MQTT topic builder for FAT-Simulator
 * Supports DO, DI, AI units
 */

// 🧩 Utility: returns the unit name as-is (placeholder for future logic if needed)
const moduleId = (unitName: string): string => unitName

// -------------------------
// ✅ Digital Outputs (DO)
// -------------------------

export const getDoSetTopic = (index: number, unit: string = 'do-unix-XXXX'): string =>
  `${moduleId(unit)}/set/do${index}`

export const getDoGroupTopic = (unit: string = 'do-unix-XXXX'): string =>
  `${moduleId(unit)}/set/do`

export const getDoStatusTopic = (index: number, unit: string = 'do-unix-XXXX'): string =>
  `${moduleId(unit)}/status/do${index}`

export const getDoStatusBase = (unit: string = 'do-unix-XXXX'): string =>
  `${moduleId(unit)}/status/`

export const getDoStatusSubscription = (unit: string = 'do-unix-XXXX'): string =>
  `${moduleId(unit)}/status/#`

export const getDoRequestStatusTopic = (unit: string = 'do-unix-XXXX'): string =>
  `${moduleId(unit)}/get/status`

// -------------------------
// ✅ Digital Inputs (DI)
// -------------------------

export const getDiStatusTopic = (index: number, unit: string = 'di-unit-XXXX'): string =>
  `${moduleId(unit)}/status/di${index}`

export const getDiStatusBase = (unit: string = 'di-unit-XXXX'): string =>
  `${moduleId(unit)}/status/`

export const getDiStatusSubscription = (unit: string = 'di-unit-XXXX'): string =>
  `${moduleId(unit)}/status/#`

export const getDiRequestStatusTopic = (unit: string = 'di-unit-XXXX'): string =>
  `${moduleId(unit)}/get/status`

// -------------------------
// 🔧 Analog Inputs (AI)
// -------------------------

export const getAiStatusTopic = (index: number, unit: string = 'ai-unit-XXXX'): string =>
  `${moduleId(unit)}/status/ai${index}`

export const getAiStatusSubscription = (unit: string = 'ai-unit-XXXX'): string =>
  `${moduleId(unit)}/status/#`

// -------------------------
// 🎛️ Common patterns
// -------------------------

export const getAnyUnitStatusSubscription = (unit: string = 'unit-id'): string =>
  `${moduleId(unit)}/status/#`

export const getAnyUnitSetTopic = (unit: string = 'unit-id'): string =>
  `${moduleId(unit)}/set/#`
