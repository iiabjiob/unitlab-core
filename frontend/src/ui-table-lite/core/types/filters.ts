export interface FilterConditionClause {
	id: string
	operator: string
	value: unknown
	value2?: unknown
	join?: "and" | "or"
}

export interface FilterCondition {
	type: "text" | "number" | "date"
	clauses: FilterConditionClause[]
}

export interface FilterMenuState {
	columnKey: string | null
	search: string
	selectedKeys: string[]
}

export interface FilterStateSnapshot {
	columnFilters: Record<string, string[]>
	advancedFilters: Record<string, FilterCondition>
}
