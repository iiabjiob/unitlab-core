import { defaultThemeTokens } from "./defaultThemeTokens"
import type { UiTableResolvedStyleConfig } from "./types"

export const defaultStyleConfig: UiTableResolvedStyleConfig = {
  table: {
    wrapper: "",
    container: "",
  },
  header: {
    row: "",
    cell: "",
    selectionCell: "",
    indexCell: "",
  },
  body: {
    row: "",
    cell: "",
    selectionCell: "",
    indexCell: "",
  },
  group: {
    row: "",
    cell: "",
    caret: "",
  },
  summary: {
    row: "",
    cell: "",
    labelCell: "",
  },
  state: {
    selectedRow: "",
  },
  tokens: defaultThemeTokens,
}
