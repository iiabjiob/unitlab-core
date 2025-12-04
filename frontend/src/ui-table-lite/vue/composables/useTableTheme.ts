import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { ComputedRef, CSSProperties, Ref } from "vue"

import { applyTableTheme, defaultStyleConfig, THEME_TOKEN_VARIABLE_MAP } from "@/ui-table/theme"
import type { UiTableResolvedStyleConfig as ThemeResolvedStyleConfig } from "@/ui-table/theme"
import type { UiTableStyleConfig, UiTableThemeTokens } from "../../core/types"
import type { NormalizedTableProps } from "../../core/config/tableConfig"

type UiTableResolvedStyleConfig = ThemeResolvedStyleConfig

type ClassValue = string | string[] | Record<string, boolean> | undefined

interface UseTableThemeOptions {
  normalizedProps: ComputedRef<NormalizedTableProps>
}

export interface UseTableThemeResult {
  tableRootRef: Ref<HTMLElement | null>
  tableThemeVars: ComputedRef<CSSProperties>
  tableWrapperClass: ComputedRef<string>
  tableContainerBaseClass: ComputedRef<ClassValue>
  headerRowClass: ComputedRef<string>
  headerCellBaseClass: ComputedRef<string>
  headerSelectionCellClass: ComputedRef<string>
  headerIndexCellClass: ComputedRef<string>
  bodyRowClass: ComputedRef<string>
  bodyCellBaseClass: ComputedRef<string>
  bodySelectionCellClass: ComputedRef<string>
  bodyIndexCellClass: ComputedRef<string>
  groupRowClass: ComputedRef<string>
  groupCellClass: ComputedRef<string>
  groupCaretClass: ComputedRef<string>
  summaryRowClass: ComputedRef<string>
  summaryCellClass: ComputedRef<string>
  summaryLabelCellClass: ComputedRef<string>
  selectedRowClass: ComputedRef<string>
}

const TABLE_BASE_CLASS = "ui-table-theme__table"
const HEADER_ROW_BASE_CLASS = "ui-table-theme__header-row"
const HEADER_CELL_BASE_CLASS = "ui-table-theme__header-cell"
const HEADER_SELECTION_CELL_BASE_CLASS = "ui-table-theme__header-selection-cell"
const HEADER_INDEX_CELL_BASE_CLASS = "ui-table-theme__header-index-cell"
const BODY_ROW_BASE_CLASS = "ui-table-theme__body-row"
const BODY_CELL_BASE_CLASS = "ui-table-theme__body-cell"
const BODY_SELECTION_CELL_BASE_CLASS = "ui-table-theme__body-selection-cell"
const BODY_INDEX_CELL_BASE_CLASS = "ui-table-theme__body-index-cell"
const GROUP_ROW_BASE_CLASS = "ui-table-theme__group-row"
const GROUP_CELL_BASE_CLASS = "ui-table-theme__group-cell"
const GROUP_CARET_BASE_CLASS = "ui-table-theme__group-caret"
const SUMMARY_ROW_BASE_CLASS = "ui-table-theme__summary-row"
const SUMMARY_CELL_BASE_CLASS = "ui-table-theme__summary-cell"
const SUMMARY_LABEL_CELL_BASE_CLASS = "ui-table-theme__summary-label-cell"
const SELECTED_ROW_BASE_CLASS = "ui-table-theme__body-row--selected"

function mergeClassValues(...values: ClassValue[]): string {
  const parts: string[] = []
  for (const value of values) {
    if (!value) continue
    if (typeof value === "string") {
      const trimmed = value.trim()
      if (trimmed) parts.push(trimmed)
      continue
    }
    if (Array.isArray(value)) {
      const merged = mergeClassValues(...value)
      if (merged) parts.push(merged)
      continue
    }
    for (const [key, active] of Object.entries(value)) {
      if (active) parts.push(key)
    }
  }
  return parts.join(" ").trim()
}

function mergeStyleConfig(
  base: UiTableResolvedStyleConfig,
  override?: UiTableStyleConfig | null,
): UiTableResolvedStyleConfig {
  const tokens: UiTableThemeTokens = { ...base.tokens }

  const applyTokenPatch = (patch?: Partial<UiTableThemeTokens>) => {
    if (!patch) return
    for (const [key, value] of Object.entries(patch)) {
      if (value == null) continue
      tokens[key as keyof UiTableThemeTokens] = value
    }
  }

  if (!override) {
    return {
      table: { ...base.table },
      header: { ...base.header },
      body: { ...base.body },
      group: { ...base.group },
      summary: { ...base.summary },
      state: { ...base.state },
      tokens,
    }
  }

  applyTokenPatch(override.tokenVariants?.default)

  const activeVariant = override.activeTokenVariant
  if (activeVariant) {
    applyTokenPatch(override.tokenVariants?.[activeVariant])
  }

  applyTokenPatch(override.tokens)

  return {
    table: { ...base.table, ...(override.table ?? {}) },
    header: { ...base.header, ...(override.header ?? {}) },
    body: { ...base.body, ...(override.body ?? {}) },
    group: { ...base.group, ...(override.group ?? {}) },
    summary: { ...base.summary, ...(override.summary ?? {}) },
    state: { ...base.state, ...(override.state ?? {}) },
    tokens,
  }
}

export function useTableTheme({ normalizedProps }: UseTableThemeOptions): UseTableThemeResult {
  const documentThemeVariant = ref<string | undefined>(undefined)
  let documentClassObserver: MutationObserver | null = null
  const tableRootRef = ref<HTMLElement | null>(null)

  function evaluateDocumentThemeVariant(config: UiTableStyleConfig | null | undefined): string | undefined {
    if (!config?.inheritThemeFromDocument) {
      return undefined
    }
    if (typeof document === "undefined") {
      return config?.defaultTokenVariant
    }
    const darkClass = config.documentDarkClass ?? "dark"
    const isDark = document.documentElement.classList.contains(darkClass)
    return isDark ? "dark" : config.defaultTokenVariant ?? "light"
  }

  watch(
    () => normalizedProps.value.styleConfig,
    (config, _oldConfig, onCleanup) => {
      if (!config?.inheritThemeFromDocument || typeof document === "undefined" || typeof MutationObserver === "undefined") {
        documentThemeVariant.value = evaluateDocumentThemeVariant(config)
        if (documentClassObserver) {
          documentClassObserver.disconnect()
          documentClassObserver = null
        }
        return
      }

      const updateFromDocument = () => {
        documentThemeVariant.value = evaluateDocumentThemeVariant(config)
      }

      updateFromDocument()

      if (documentClassObserver) {
        documentClassObserver.disconnect()
      }

      documentClassObserver = new MutationObserver(updateFromDocument)
      documentClassObserver.observe(document.documentElement, {
        attributes: true,
        attributeFilter: ["class"],
      })

      onCleanup(() => {
        if (documentClassObserver) {
          documentClassObserver.disconnect()
          documentClassObserver = null
        }
      })
    },
    { immediate: true },
  )

  const resolvedStyleOverride = computed<UiTableStyleConfig | null>(() => {
    const config = normalizedProps.value.styleConfig
    if (!config) {
      return null
    }
    const activeVariant =
      config.activeTokenVariant ?? (config.inheritThemeFromDocument ? documentThemeVariant.value : undefined) ?? config.defaultTokenVariant
    if (!activeVariant) {
      return config
    }
    return {
      ...config,
      activeTokenVariant: activeVariant,
    }
  })

  const styleClasses = computed(() => mergeStyleConfig(defaultStyleConfig, resolvedStyleOverride.value))
  const themeTokens = computed(() => styleClasses.value.tokens)
  const tableThemeVars = computed<CSSProperties>(() => {
    const tokens = themeTokens.value
    const vars: Record<string, string> = {}
    for (const [tokenKey, cssVar] of Object.entries(THEME_TOKEN_VARIABLE_MAP)) {
      const value = tokens[tokenKey as keyof UiTableThemeTokens]
      if (value != null) {
        vars[cssVar] = value
      }
    }
    return vars as CSSProperties
  })

  function applyThemeToRoot(tokens: UiTableThemeTokens) {
    const root = tableRootRef.value?.closest?.(".ui-table-theme-root") ?? tableRootRef.value
    if (root instanceof HTMLElement) {
      applyTableTheme(root, tokens)
    }
  }

  watch(
    themeTokens,
    tokens => {
      applyThemeToRoot(tokens)
    },
    { immediate: true },
  )

  onMounted(() => {
    applyThemeToRoot(themeTokens.value)
  })

  onBeforeUnmount(() => {
    if (documentClassObserver) {
      documentClassObserver.disconnect()
      documentClassObserver = null
    }
  })

  const tableWrapperClass = computed(() => mergeClassValues(TABLE_BASE_CLASS, styleClasses.value.table.wrapper))
  const tableContainerBaseClass = computed<ClassValue>(() => styleClasses.value.table.container ?? "")
  const headerRowClass = computed(() => mergeClassValues(HEADER_ROW_BASE_CLASS, styleClasses.value.header.row))
  const headerCellBaseClass = computed(() => mergeClassValues(HEADER_CELL_BASE_CLASS, styleClasses.value.header.cell))
  const headerSelectionCellClass = computed(() =>
    mergeClassValues(
      HEADER_SELECTION_CELL_BASE_CLASS,
      styleClasses.value.header.selectionCell ?? styleClasses.value.header.cell,
    ),
  )
  const headerIndexCellClass = computed(() =>
    mergeClassValues(
      HEADER_INDEX_CELL_BASE_CLASS,
      styleClasses.value.header.indexCell ?? styleClasses.value.header.cell,
    ),
  )
  const bodyRowClass = computed(() => mergeClassValues(BODY_ROW_BASE_CLASS, styleClasses.value.body.row))
  const bodyCellBaseClass = computed(() => mergeClassValues(BODY_CELL_BASE_CLASS, styleClasses.value.body.cell))
  const bodySelectionCellClass = computed(() =>
    mergeClassValues(
      BODY_SELECTION_CELL_BASE_CLASS,
      styleClasses.value.body.selectionCell ?? styleClasses.value.body.cell,
    ),
  )
  const bodyIndexCellClass = computed(() =>
    mergeClassValues(
      BODY_INDEX_CELL_BASE_CLASS,
      styleClasses.value.body.indexCell ?? styleClasses.value.body.cell,
    ),
  )
  const groupRowClass = computed(() => mergeClassValues(GROUP_ROW_BASE_CLASS, styleClasses.value.group.row))
  const groupCellClass = computed(() => mergeClassValues(GROUP_CELL_BASE_CLASS, styleClasses.value.group.cell))
  const groupCaretClass = computed(() => mergeClassValues(GROUP_CARET_BASE_CLASS, styleClasses.value.group.caret))
  const summaryRowClass = computed(() => mergeClassValues(SUMMARY_ROW_BASE_CLASS, styleClasses.value.summary.row))
  const summaryCellClass = computed(() =>
    mergeClassValues(
      SUMMARY_CELL_BASE_CLASS,
      styleClasses.value.summary.cell ?? styleClasses.value.body.cell,
    ),
  )
  const summaryLabelCellClass = computed(() =>
    mergeClassValues(
      SUMMARY_LABEL_CELL_BASE_CLASS,
      styleClasses.value.summary.labelCell ?? styleClasses.value.summary.cell ?? styleClasses.value.body.cell,
    ),
  )
  const selectedRowClass = computed(() =>
    mergeClassValues(SELECTED_ROW_BASE_CLASS, styleClasses.value.state.selectedRow),
  )

  return {
    tableRootRef,
    tableThemeVars,
    tableWrapperClass,
    tableContainerBaseClass,
    headerRowClass,
    headerCellBaseClass,
    headerSelectionCellClass,
    headerIndexCellClass,
    bodyRowClass,
    bodyCellBaseClass,
    bodySelectionCellClass,
    bodyIndexCellClass,
    groupRowClass,
    groupCellClass,
    groupCaretClass,
    summaryRowClass,
    summaryCellClass,
    summaryLabelCellClass,
    selectedRowClass,
  }
}
