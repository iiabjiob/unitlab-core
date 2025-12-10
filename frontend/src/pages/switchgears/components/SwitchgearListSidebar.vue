<script setup lang="ts">
import { ref, computed } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useRouter, useRoute } from "vue-router"
import SwitchgearListItem from "./SwitchgearListItem.vue"
import UiButton from "@/components/ui/UiButton.vue"

const store = useSwitchgearStore()
const router = useRouter()
const route = useRoute()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSwitchgear(id: number) {
  router.push({ name: "switchgears.detail", params: { id } })
}

async function addSwitchgear() {
  const created = await store.createAuto()
  openSwitchgear(created.id)
}

// SEARCH
const query = ref("")

const filteredSwitchgears = computed(() => {
  if (!query.value.trim()) return store.switchgears

  const q = query.value.toLowerCase()

  return store.switchgears.filter(s =>
    s.name.toLowerCase().includes(q) ||
    s.switchgear_type.toLowerCase().includes(q)
  )
})
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <div class="mb-3">
      <UiButton
        variant="primary"
        size="sm"
        full
        @click="addSwitchgear"
      >
        + New Switchgear
      </UiButton>
    </div>

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        name="switchgear-search"
        placeholder="Search switchgears…"
        class="w-full px-3 py-1 text-sm rounded border border-gray-700
               text-gray-800 dark:text-gray-200 placeholder-gray-500"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-for="switchgear in filteredSwitchgears"
        :key="switchgear.id"
      >
        <SwitchgearListItem
          :switchgear="switchgear"
          :active="isActive(switchgear.id)"
          @click="openSwitchgear(switchgear.id)"
        />
      </div>

      <div
        v-if="filteredSwitchgears.length === 0"
        class="text-gray-500 text-xs italic px-2 py-2"
      >
        No switchgears found
      </div>
    </div>

  </div>
</template>
