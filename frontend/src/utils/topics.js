/**
 * Centralized MQTT topic builder for FAT-Simulator
 * Supports DO, DI, AI boards
 */

// 🧩 Utility: allows overriding module name if needed
const moduleId = (boardName) => boardName

// -------------------------
// ✅ Digital Outputs (DO)
// -------------------------

export const getDoSetTopic = (index, board = 'do-board-1') =>
  `${moduleId(board)}/set/do${index}`

export const getDoGroupTopic = (board = 'do-board-1') =>
  `${moduleId(board)}/set/do` // 🆕 Group topic

export const getDoStatusTopic = (index, board = 'do-board-1') =>
  `${moduleId(board)}/status/do${index}`

export const getDoStatusBase = (board = 'do-board-1') =>
  `${moduleId(board)}/status/`

export const getDoStatusSubscription = (board = 'do-board-1') =>
  `${moduleId(board)}/status/#`

export const getDoRequestStatusTopic = (board = 'do-board-1') =>
  `${moduleId(board)}/get/status`

// -------------------------
// ✅ Digital Inputs (DI)
// -------------------------

export const getDiStatusTopic = (index, board = 'di-board-1') =>
  `${moduleId(board)}/status/di${index}`

export const getDiStatusBase = (board = 'di-board-1') =>
  `${moduleId(board)}/status/`

export const getDiStatusSubscription = (board = 'di-board-1') =>
  `${moduleId(board)}/status/#`

export const getDiRequestStatusTopic = (board = 'di-board-1') =>
  `${moduleId(board)}/get/status`

// -------------------------
// 🔧 Future: Analog Inputs (AI)
// -------------------------

export const getAiStatusTopic = (index, board = 'ai-board-1') =>
  `${moduleId(board)}/status/ai${index}`

export const getAiStatusSubscription = (board = 'ai-board-1') =>
  `${moduleId(board)}/status/#`

// -------------------------
// 🎛️ Common patterns
// -------------------------

export const getAnyBoardStatusSubscription = (board = 'BOARD-ID') =>
  `${moduleId(board)}/status/#`

export const getAnyBoardSetTopic = (board = 'BOARD-ID') =>
  `${moduleId(board)}/set/#`
