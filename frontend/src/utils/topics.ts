/**
 * Centralized MQTT topic builder for FAT-Simulator
 * Supports DO, DI, AI boards
 */

// 🧩 Utility: returns the board name as-is (placeholder for future logic if needed)
const moduleId = (boardName: string): string => boardName

// -------------------------
// ✅ Digital Outputs (DO)
// -------------------------

export const getDoSetTopic = (index: number, board: string = 'do-board-1'): string =>
  `${moduleId(board)}/set/do${index}`

export const getDoGroupTopic = (board: string = 'do-board-1'): string =>
  `${moduleId(board)}/set/do`

export const getDoStatusTopic = (index: number, board: string = 'do-board-1'): string =>
  `${moduleId(board)}/status/do${index}`

export const getDoStatusBase = (board: string = 'do-board-1'): string =>
  `${moduleId(board)}/status/`

export const getDoStatusSubscription = (board: string = 'do-board-1'): string =>
  `${moduleId(board)}/status/#`

export const getDoRequestStatusTopic = (board: string = 'do-board-1'): string =>
  `${moduleId(board)}/get/status`

// -------------------------
// ✅ Digital Inputs (DI)
// -------------------------

export const getDiStatusTopic = (index: number, board: string = 'di-board-1'): string =>
  `${moduleId(board)}/status/di${index}`

export const getDiStatusBase = (board: string = 'di-board-1'): string =>
  `${moduleId(board)}/status/`

export const getDiStatusSubscription = (board: string = 'di-board-1'): string =>
  `${moduleId(board)}/status/#`

export const getDiRequestStatusTopic = (board: string = 'di-board-1'): string =>
  `${moduleId(board)}/get/status`

// -------------------------
// 🔧 Analog Inputs (AI)
// -------------------------

export const getAiStatusTopic = (index: number, board: string = 'ai-board-1'): string =>
  `${moduleId(board)}/status/ai${index}`

export const getAiStatusSubscription = (board: string = 'ai-board-1'): string =>
  `${moduleId(board)}/status/#`

// -------------------------
// 🎛️ Common patterns
// -------------------------

export const getAnyBoardStatusSubscription = (board: string = 'BOARD-ID'): string =>
  `${moduleId(board)}/status/#`

export const getAnyBoardSetTopic = (board: string = 'BOARD-ID'): string =>
  `${moduleId(board)}/set/#`
