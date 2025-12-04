// src/ui-table/theme/types.ts
// Contains base interfaces for table theme tokens and style config.

export interface UiTableThemeTokens {
  tableFontFamily: string
  tableFontSize: string
  tableTextColor: string
  tableBackgroundColor: string
  headerBackgroundColor: string
  headerTextColor: string
  headerBorderColor: string
  headerPaddingX: string
  headerPaddingY: string
  headerHighlightBorderColor: string
  headerHighlightTextColor: string
  headerSelectedTextColor: string
  headerSystemTextColor: string
  bodyRowBackgroundColor: string
  bodyRowTextColor: string
  bodyRowHoverBackgroundColor: string
  bodyRowSelectedBackgroundColor: string
  bodyRowSelectedTextColor: string
  bodyCellPaddingX: string
  bodyCellPaddingY: string
  bodyCellBorderColor: string
  selectionCellBackgroundColor: string
  selectionCellTextColor: string
  indexCellBackgroundColor: string
  indexCellTextColor: string
  rowDividerSize: string
  rowDividerColor: string
  columnDividerSize: string
  columnDividerColor: string
  headerDividerSize: string
  headerDividerColor: string
  summaryDividerSize: string
  summaryDividerColor: string
  groupRowBackgroundColor: string
  groupRowTextColor: string
  summaryRowBackgroundColor: string
  summaryRowTextColor: string
  summaryLabelTextColor: string
  pinnedBackgroundColor: string
  pinnedLeftBackgroundColor: string
  pinnedRightBackgroundColor: string
  pinnedShadow: string
  pinnedLeftShadow: string
  pinnedRightShadow: string
  pinnedLeftBorderColor: string
  pinnedLeftBorderWidth: string
  pinnedRightBorderColor: string
  pinnedRightBorderWidth: string
}

export interface UiTableResolvedStyleConfig {
  table: Record<string, string>
  header: Record<string, string>
  body: Record<string, string>
  group: Record<string, string>
  summary: Record<string, string>
  state: Record<string, string>
  tokens: UiTableThemeTokens
}

export interface UiTableStyleSection {
  wrapper?: string
  container?: string
}

export interface UiTableHeaderStyle {
  row?: string
  cell?: string
  selectionCell?: string
  indexCell?: string
}

export interface UiTableBodyStyle {
  row?: string
  cell?: string
  selectionCell?: string
  indexCell?: string
}

export interface UiTableGroupStyle {
  row?: string
  cell?: string
  caret?: string
}

export interface UiTableSummaryStyle {
  row?: string
  cell?: string
  labelCell?: string
}

export interface UiTableStateStyle {
  selectedRow?: string
}

export type UiTableThemeTokenVariants = Record<string, Partial<UiTableThemeTokens>>

export interface UiTableStyleConfig {
  table?: UiTableStyleSection
  header?: UiTableHeaderStyle
  body?: UiTableBodyStyle
  group?: UiTableGroupStyle
  summary?: UiTableSummaryStyle
  state?: UiTableStateStyle
  tokens?: Partial<UiTableThemeTokens>
  tokenVariants?: UiTableThemeTokenVariants
  activeTokenVariant?: string
  defaultTokenVariant?: string
  inheritThemeFromDocument?: boolean
  documentDarkClass?: string
}
