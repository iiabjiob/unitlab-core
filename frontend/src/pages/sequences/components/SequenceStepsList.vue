<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import SequenceStepAddToolbar from "./SequenceStepAddToolbar.vue"
import SequenceStepItem from "./SequenceStepItem.vue"
import DraggableList from "@/components/ui/DraggableList.vue"
import type { SequenceDef, SequenceStep, SequenceStepCreate } from "@/types/sequences"

const props = defineProps<{ sequence: SequenceDef }>()

const stepStore = useSequenceStepStore()
const sequenceStore = useSequenceStore()
const toastStore = useToastStore()

type EnrichedStep = SequenceStep & { description: string }
type StepSelectionPayload = { stepId: number; shiftKey: boolean; ctrlKey: boolean; metaKey: boolean }
type StepsClipboardPayload = {
  kind: "affino.sequence-steps"
  version: 1
  steps: SequenceStepCreate[]
}

const CLIPBOARD_KIND = "affino.sequence-steps"

const steps = computed<EnrichedStep[]>(() =>
  stepStore.enrichedStepsBySequence(props.sequence.id).value
)

const draggableSteps = ref<EnrichedStep[]>([])
const selectedStepIds = ref<number[]>([])
const selectionAnchorId = ref<number | null>(null)
const localClipboardDrafts = ref<SequenceStepCreate[]>([])

const selectedStepIdSet = computed(() => new Set(selectedStepIds.value))

watch(
  steps,
  (next) => {
    draggableSteps.value = [...next]
    syncSelectionWithSteps(next)
  },
  { immediate: true }
)

watch(
  () => stepStore.activeStepId,
  (nextActiveId) => {
    if (nextActiveId === null) {
      if (!selectedStepIds.value.length) {
        selectionAnchorId.value = null
      }
      return
    }

    if (!selectedStepIdSet.value.has(nextActiveId)) {
      applySelection([nextActiveId], nextActiveId, nextActiveId)
    }
  }
)

function orderedStepIds() {
  return draggableSteps.value.map((step) => step.id)
}

function normalizeSelection(ids: number[]) {
  const orderedIds = orderedStepIds()
  const idSet = new Set(ids)
  return orderedIds.filter((id) => idSet.has(id))
}

function applySelection(ids: number[], activeId: number | null, anchorId: number | null) {
  const normalized = normalizeSelection(ids)
  selectedStepIds.value = normalized
  selectionAnchorId.value = anchorId
  stepStore.setActiveStep(activeId)
}

function syncSelectionWithSteps(nextSteps: EnrichedStep[]) {
  const existingIds = new Set(nextSteps.map((step) => step.id))
  const filteredSelection = selectedStepIds.value.filter((id) => existingIds.has(id))
  const activeId = stepStore.activeStepId
  const activeExists = activeId !== null && existingIds.has(activeId)

  if (!filteredSelection.length) {
    if (activeExists && activeId !== null) {
      selectedStepIds.value = [activeId]
      selectionAnchorId.value = activeId
      return
    }

    selectedStepIds.value = []
    selectionAnchorId.value = null
    if (activeId !== null) {
      stepStore.setActiveStep(null)
    }
    return
  }

  selectedStepIds.value = normalizeSelection(filteredSelection)
  if (selectionAnchorId.value === null || !existingIds.has(selectionAnchorId.value)) {
    selectionAnchorId.value = selectedStepIds.value[selectedStepIds.value.length - 1] ?? null
  }

  if (!activeExists) {
    stepStore.setActiveStep(selectedStepIds.value[selectedStepIds.value.length - 1] ?? null)
  }
}

function collectRange(fromId: number, toId: number) {
  const ids = orderedStepIds()
  const fromIndex = ids.indexOf(fromId)
  const toIndex = ids.indexOf(toId)
  if (fromIndex < 0 || toIndex < 0) {
    return [toId]
  }
  const start = Math.min(fromIndex, toIndex)
  const end = Math.max(fromIndex, toIndex)
  return ids.slice(start, end + 1)
}

function selectStep(payload: StepSelectionPayload) {
  const { stepId, shiftKey, ctrlKey, metaKey } = payload
  const additive = ctrlKey || metaKey

  if (shiftKey) {
    const anchor = selectionAnchorId.value ?? stepStore.activeStepId ?? stepId
    const range = collectRange(anchor, stepId)
    if (additive) {
      applySelection([...selectedStepIds.value, ...range], stepId, anchor)
      return
    }
    applySelection(range, stepId, anchor)
    return
  }

  if (additive) {
    if (selectedStepIdSet.value.has(stepId)) {
      const nextSelection = selectedStepIds.value.filter((id) => id !== stepId)
      const nextActive = nextSelection[nextSelection.length - 1] ?? null
      applySelection(nextSelection, nextActive, stepId)
      return
    }
    applySelection([...selectedStepIds.value, stepId], stepId, stepId)
    return
  }

  applySelection([stepId], stepId, stepId)
}

function itemKey(step: EnrichedStep) {
  return String(step.id)
}

async function handleReorder(nextItems: EnrichedStep[]) {
  draggableSteps.value = nextItems
  const newOrder = nextItems.map((item) => item.id)

  try {
    await stepStore.reorderSteps(props.sequence.id, newOrder)
    sequenceStore.resetState(props.sequence.id)
    selectedStepIds.value = normalizeSelection(selectedStepIds.value)
  } catch (error) {
    console.error("Failed to reorder steps", error)
    draggableSteps.value = [...steps.value]
  }
}

async function handleDuplicate(stepId: number) {
  try {
    const duplicateIds = selectedStepIdSet.value.has(stepId)
      ? [...selectedStepIds.value]
      : [stepId]

    const inserted = duplicateIds.length > 1
      ? await stepStore.duplicateSteps(
          props.sequence.id,
          duplicateIds,
          duplicateIds[duplicateIds.length - 1] ?? stepId,
        )
      : [await stepStore.duplicateStep(props.sequence.id, stepId)]

    sequenceStore.resetState(props.sequence.id)
    const insertedIds = inserted.map((step) => step.id)
    const nextActiveId = insertedIds[insertedIds.length - 1] ?? null
    applySelection(insertedIds, nextActiveId, nextActiveId)
  } catch (error) {
    console.error("Failed to duplicate step", error)
  }
}

async function handleAdd(payload: SequenceStepCreate) {
  try {
    const created = await stepStore.addStep(props.sequence.id, payload)
    sequenceStore.resetState(props.sequence.id)
    stepStore.setActiveStep(created.id)
  } catch (error) {
    console.error("Failed to add step", error)
  }
}

function selectedStepsInOrder() {
  return steps.value.filter((step) => selectedStepIdSet.value.has(step.id))
}

function getInsertAfterStepId() {
  const orderedIds = orderedStepIds()
  const selectedInOrder = orderedIds.filter((id) => selectedStepIdSet.value.has(id))
  if (selectedInOrder.length) {
    return selectedInOrder[selectedInOrder.length - 1]
  }
  if (stepStore.activeStepId !== null) {
    return stepStore.activeStepId
  }
  return orderedIds[orderedIds.length - 1] ?? null
}

function parseClipboardPayload(raw: string): SequenceStepCreate[] | null {
  try {
    const parsed = JSON.parse(raw) as StepsClipboardPayload
    if (parsed?.kind !== CLIPBOARD_KIND || parsed?.version !== 1 || !Array.isArray(parsed.steps)) {
      return null
    }
    return parsed.steps.filter((step): step is SequenceStepCreate =>
      Boolean(step && typeof step.sequence_step_type === "string")
    )
  } catch {
    return null
  }
}

function buildClipboardPayload(stepsToCopy: SequenceStepCreate[]) {
  return JSON.stringify(
    {
      kind: CLIPBOARD_KIND,
      version: 1,
      steps: stepsToCopy,
    } as StepsClipboardPayload,
    null,
    2
  )
}

async function handleCopySelectedSteps() {
  const sourceSteps = selectedStepsInOrder()
  if (!sourceSteps.length) {
    toastStore.info("Select at least one step to copy")
    return
  }

  const drafts = sourceSteps.map((step) => stepStore.buildStepCreateFromStep(step))
  localClipboardDrafts.value = drafts

  try {
    if (typeof navigator !== "undefined" && navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(buildClipboardPayload(drafts))
    }
    toastStore.success(`Copied ${drafts.length} step${drafts.length > 1 ? "s" : ""}`)
  } catch {
    toastStore.success(`Copied ${drafts.length} step${drafts.length > 1 ? "s" : ""}`)
  }
}

async function handleCopyFromStep(stepId: number) {
  if (!selectedStepIdSet.value.has(stepId)) {
    applySelection([stepId], stepId, stepId)
  }
  await handleCopySelectedSteps()
}

async function resolveClipboardDrafts() {
  try {
    if (typeof navigator !== "undefined" && navigator.clipboard?.readText) {
      const rawText = await navigator.clipboard.readText()
      const parsed = parseClipboardPayload(rawText)
      if (parsed?.length) {
        localClipboardDrafts.value = parsed
        return parsed
      }
    }
  } catch {
  }

  if (localClipboardDrafts.value.length) {
    return localClipboardDrafts.value
  }

  return null
}

async function handlePasteSteps(insertAfterStepId?: number | null) {
  const drafts = await resolveClipboardDrafts()
  if (!drafts?.length) {
    toastStore.info("Nothing to paste")
    return
  }

  try {
    const insertAfter = typeof insertAfterStepId === "number"
      ? insertAfterStepId
      : getInsertAfterStepId()
    const inserted = await stepStore.insertSteps(props.sequence.id, drafts, insertAfter)
    sequenceStore.resetState(props.sequence.id)
    const insertedIds = inserted.map((step) => step.id)
    const nextActiveId = insertedIds[insertedIds.length - 1] ?? null
    applySelection(insertedIds, nextActiveId, nextActiveId)
    toastStore.success(`Pasted ${inserted.length} step${inserted.length > 1 ? "s" : ""}`)
  } catch (error) {
    console.error("Failed to paste steps", error)
    toastStore.error("Failed to paste steps")
  }
}

async function handlePasteAfterStep(stepId: number) {
  await handlePasteSteps(stepId)
}

async function deleteSelectedSteps() {
  const idsToDelete = selectedStepIds.value.length
    ? [...selectedStepIds.value]
    : stepStore.activeStepId !== null
      ? [stepStore.activeStepId]
      : []

  if (!idsToDelete.length) {
    toastStore.info("Select at least one step to delete")
    return
  }

  const beforeOrder = orderedStepIds()
  const firstRemovedIndex = beforeOrder.findIndex((id) => idsToDelete.includes(id))

  const toDeleteIds = steps.value
    .filter((step) => idsToDelete.includes(step.id))
    .map((step) => step.id)

  try {
    await Promise.all(
      toDeleteIds.map((stepId) => stepStore.deleteStep(props.sequence.id, stepId)),
    )

    sequenceStore.resetState(props.sequence.id)

    const remainingIds = orderedStepIds()
    if (!remainingIds.length) {
      applySelection([], null, null)
      return
    }

    const fallbackIndex = Math.min(
      Math.max(firstRemovedIndex, 0),
      remainingIds.length - 1,
    )
    const nextActiveId = remainingIds[fallbackIndex] ?? remainingIds[remainingIds.length - 1]
    applySelection([nextActiveId], nextActiveId, nextActiveId)
  } catch (error) {
    console.error("Failed to delete selected steps", error)
    toastStore.error("Failed to delete selected steps")
  }
}

async function deleteFromStep(stepId: number) {
  if (!selectedStepIdSet.value.has(stepId)) {
    applySelection([stepId], stepId, stepId)
  }
  await deleteSelectedSteps()
}

async function duplicateFromStep(stepId: number) {
  if (!selectedStepIdSet.value.has(stepId)) {
    applySelection([stepId], stepId, stepId)
  }
  await handleDuplicate(stepId)
}

function selectAllSteps() {
  const allIds = orderedStepIds()
  if (!allIds.length) return
  const lastId = allIds[allIds.length - 1] ?? null
  applySelection(allIds, lastId, allIds[0] ?? null)
}

function handleListKeydown(event: KeyboardEvent) {
  const orderedIds = orderedStepIds()
  const currentId = stepStore.activeStepId

  if (event.key === "ArrowDown" || event.key === "ArrowUp") {
    if (!orderedIds.length) {
      return
    }

    event.preventDefault()

    const direction = event.key === "ArrowDown" ? 1 : -1

    if (currentId === null) {
      const fallbackId = direction > 0 ? orderedIds[0] : orderedIds[orderedIds.length - 1]
      if (fallbackId !== undefined) {
        applySelection([fallbackId], fallbackId, fallbackId)
      }
      return
    }

    const currentIndex = orderedIds.indexOf(currentId)
    if (currentIndex < 0) {
      const fallbackId = direction > 0 ? orderedIds[0] : orderedIds[orderedIds.length - 1]
      if (fallbackId !== undefined) {
        applySelection([fallbackId], fallbackId, fallbackId)
      }
      return
    }

    const nextIndex = Math.min(Math.max(currentIndex + direction, 0), orderedIds.length - 1)
    const nextId = orderedIds[nextIndex]
    if (nextId === undefined) {
      return
    }

    if (event.shiftKey) {
      const anchor = selectionAnchorId.value ?? currentId
      const range = collectRange(anchor, nextId)
      applySelection(range, nextId, anchor)
      return
    }

    applySelection([nextId], nextId, nextId)
    return
  }

  const isModifier = event.metaKey || event.ctrlKey

  if (isModifier && event.key.toLowerCase() === "c") {
    event.preventDefault()
    void handleCopySelectedSteps()
    return
  }

  if (isModifier && event.key.toLowerCase() === "v") {
    event.preventDefault()
    void handlePasteSteps()
    return
  }

  if (event.key === "Delete" || event.key === "Backspace") {
    event.preventDefault()
    void deleteSelectedSteps()
    return
  }

  if (event.key === "Escape" && stepStore.activeStepId !== null) {
    event.preventDefault()
    applySelection([stepStore.activeStepId], stepStore.activeStepId, stepStore.activeStepId)
  }
}
</script>

<template>
  <div class="flex flex-col h-full overflow-hidden p-4">

    <!-- HEADER -->
    <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
      Steps ({{ steps.length }})
    </div>

    <!-- TOOLBAR -->
    <div class="py-2">
      <SequenceStepAddToolbar
        @add="handleAdd"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto mt-5">
      <div
        tabindex="0"
        role="listbox"
        aria-label="Sequence steps"
        aria-multiselectable="true"
        class="h-full rounded-md focus:outline-none focus-visible:ring-2 focus-visible:ring-blue-500/60"
        @keydown="handleListKeydown"
      >
        <DraggableList
          :items="draggableSteps"
          :item-key="itemKey"
          wrapper-tag="ul"
          item-tag="li"
          handle-only
          class="divide-y divide-neutral-300 dark:divide-neutral-700"
          :style="{ gap: '0' }"
          @update:items="handleReorder"
        >
          <template #default="{ item }">
            <SequenceStepItem
              :step="item"
              :active="stepStore.activeStepId === item.id"
              :selected="selectedStepIdSet.has(item.id)"
              @select="selectStep"
              @copy="handleCopyFromStep"
              @paste-after="handlePasteAfterStep"
              @delete="deleteFromStep"
              @duplicate="duplicateFromStep"
              @select-all="selectAllSteps"
            />
          </template>
        </DraggableList>
      </div>
    </div>

  </div>
</template>
