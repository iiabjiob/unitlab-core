<template>
  <UiModal :open="open" :title="modalTitle" @close="handleCancel">
    <template #header>
      <span class="text-sm font-semibold">Filter by condition</span>
      <p class="mt-1 text-[11px] text-neutral-500 dark:text-neutral-400">
        Column: <span class="font-medium text-neutral-700 dark:text-neutral-200">{{ columnLabel || 'this column' }}</span>
      </p>
    </template>
    <div class="mt-4 space-y-3 max-h-[60vh] overflow-y-auto pr-1">
      <div
        v-for="(clause, index) in clauses"
        :key="clause.id"
        class="space-y-3 rounded border border-neutral-200 bg-white p-3 shadow-sm dark:border-neutral-700 dark:bg-neutral-800"
      >
        <div class="flex items-center justify-between text-[11px] font-medium text-neutral-600 dark:text-neutral-300">
          <span>Condition {{ index + 1 }}</span>
          <button
            v-if="clauses.length > 1"
            type="button"
            class="text-neutral-500 transition hover:text-neutral-700 dark:text-neutral-400 dark:hover:text-neutral-200"
            @click="removeClause(clause.id)"
          >
            Remove
          </button>
        </div>
        <div class="flex flex-wrap items-center gap-2">
          <select
            v-if="index > 0"
            v-model="clause.join"
            :name="fieldName(clause.id, 'join')"
            class="ui-table__control w-20"
          >
            <option value="and">AND</option>
            <option value="or">OR</option>
          </select>
          <select
            v-model="clause.operator"
            :name="fieldName(clause.id, 'operator')"
            class="ui-table__control"
          >
            <option
              v-for="option in operatorOptions"
              :key="`${clause.id}-${option}`"
              :value="option"
            >
              {{ operatorLabels[option] ?? option }}
            </option>
          </select>
          <input
            v-model="clause.value"
            :type="inputType"
            :inputmode="inputMode"
            :name="fieldName(clause.id, 'value')"
            class="ui-table__control"
          />
          <input
            v-if="requiresSecondValue(clause.operator)"
            v-model="clause.value2"
            :type="inputType"
            :inputmode="inputMode"
            :name="fieldName(clause.id, 'value2')"
            class="ui-table__control"
          />
        </div>
      </div>
    </div>
    <button
      type="button"
      class="mt-3 text-[11px] font-medium text-blue-600 transition hover:text-blue-700 dark:text-blue-400 dark:hover:text-blue-300"
      @click="addClause"
    >
      + Add condition
    </button>
    <template #footer>
      <button
        type="button"
        class="ui-table__button ui-table__button--neutral"
        @click="handleClear"
      >
        Clear
      </button>
      <button
        type="button"
        class="ui-table__button ui-table__button--neutral"
        @click="handleCancel"
      >
        Cancel
      </button>
      <button
        type="button"
        class="ui-table__button ui-table__button--primary"
        @click="handleApply"
      >
        Apply
      </button>
    </template>
  </UiModal>
<!-- End of template -->
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"
import UiModal from "./UiModal.vue"
const modalTitle = "Filter by condition"
import type { FilterCondition } from "../composables/useTableFilters"

const operatorLabels: Record<string, string> = {
  contains: "Contains",
  startsWith: "Starts with",
  endsWith: "Ends with",
  equals: "Equals",
  ">": ">",
  "<": "<",
  ">=": ">=",
  "<=": "<=",
  between: "Between",
}

const operatorMap: Record<FilterCondition["type"], string[]> = {
  text: ["contains", "startsWith", "endsWith", "equals"],
  number: ["equals", ">", "<", ">=", "<=", "between"],
  date: ["equals", ">", "<", ">=", "<=", "between"],
}

const inputTypeMap: Record<FilterCondition["type"], string> = {
  text: "text",
  number: "number",
  date: "date",
}

interface LocalClause {
  id: number
  join: "and" | "or"
  operator: string
  value: any
  value2?: any
}

const props = defineProps<{
  open: boolean
  columnLabel: string
  type: FilterCondition["type"]
  condition: FilterCondition | null
}>()

const emit = defineEmits<{
  (e: "apply", condition: FilterCondition | null): void
  (e: "cancel"): void
  (e: "clear"): void
}>()

let clauseCounter = 0

const operatorOptions = computed(() => operatorMap[props.type])
const inputType = computed(() => inputTypeMap[props.type])
const clauses = ref<LocalClause[]>([])

const inputMode = computed(() => {
  if (props.type === "number") return "decimal"
  return undefined
})

function nextClauseId() {
  clauseCounter += 1
  return clauseCounter
}

function createClause(join: "and" | "or" = "and"): LocalClause {
  return {
    id: nextClauseId(),
    join,
    operator: operatorMap[props.type][0],
    value: "",
    value2: undefined,
  }
}

function ensureValidOperator(operator: string) {
  const options = operatorOptions.value
  return options.includes(operator) ? operator : options[0]
}

function normalizeClauseJoins(list: LocalClause[]) {
  return list.map((clause, index) => ({
    ...clause,
    join: index === 0 ? "and" : clause.join,
  }))
}

function resetClauses() {
  clauses.value = [createClause()]
}

function hydrateFromCondition(condition: FilterCondition | null) {
  if (!condition || !condition.clauses?.length) {
    resetClauses()
    return
  }
  clauses.value = normalizeClauseJoins(
    condition.clauses.map(clause => ({
      id: nextClauseId(),
      join: clause.join ?? "and",
      operator: ensureValidOperator(clause.operator),
      value: clause.value,
      value2: clause.value2,
    }))
  )
  if (!clauses.value.length) {
    resetClauses()
  }
}

watch(
  () => props.open,
  open => {
    if (open) {
      hydrateFromCondition(props.condition)
    }
  },
  { immediate: true }
)

watch(
  () => props.condition,
  value => {
    if (props.open) {
      hydrateFromCondition(value)
    }
  }
)

watch(
  () => props.type,
  () => {
    if (props.open) {
      hydrateFromCondition(props.condition)
    } else {
      resetClauses()
    }
  }
)

function requiresSecondValue(operator: string) {
  return operator === "between"
}

function clauseHasValue(type: FilterCondition["type"], clause: LocalClause): boolean {
  if (type === "text") {
    return Boolean(String(clause.value ?? "").trim())
  }
  if (clause.operator === "between") {
    return clause.value !== undefined && clause.value !== "" && clause.value2 !== undefined && clause.value2 !== ""
  }
  return clause.value !== undefined && clause.value !== ""
}

function fieldName(id: number, field: "join" | "operator" | "value" | "value2") {
  return `filter-clause-${id}-${field}`
}

function handleApply() {
  const valid = clauses.value.filter(clause => clauseHasValue(props.type, clause))
  if (!valid.length) {
    emit("apply", null)
    return
  }
  emit("apply", {
    type: props.type,
    clauses: valid.map((clause, index) => ({
      id: clause.id.toString(),
      operator: clause.operator,
      value: clause.value,
      value2: clause.value2,
      join: index === 0 ? undefined : clause.join ?? "and",
    })),
  })
}

function handleCancel() {
  emit("cancel")
}

function handleClear() {
  emit("clear")
  resetClauses()
}

function addClause() {
  clauses.value = normalizeClauseJoins([...clauses.value, createClause("and")])
}

function removeClause(id: number) {
  if (clauses.value.length === 1) {
    resetClauses()
    return
  }
  clauses.value = normalizeClauseJoins(clauses.value.filter(clause => clause.id !== id))
}

resetClauses()
</script>
