<script setup lang="ts">
import { useRouter } from "vue-router"
import UiButton from "@/components/ui/UiButton.vue"
import type { SequenceDef } from "@/types/sequences"

defineProps<{
  sequence: SequenceDef
}>()

const router = useRouter()

function openTestRuns() {
  router.push({ name: "signals.testRuns" })
}

function openSignals() {
  router.push({ name: "signals.home" })
}
</script>

<template>
  <section class="mt-4 rounded-2xl border border-neutral-200 bg-white/80 p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/80">
    <p class="text-xs font-semibold uppercase tracking-[0.3em] text-neutral-400">
      Execution flow
    </p>
    <h3 class="mt-2 text-lg font-semibold text-neutral-900 dark:text-neutral-50">
      Эта последовательность запускается только внутри Test Run.
    </h3>
    <p class="mt-2 text-sm text-neutral-600 dark:text-neutral-300">
      Sequence описывает шаги. Чтобы она заработала с реальными каналами или сигналами,
      соберите Test Run с нужным набором последовательностей, аллокацией и (при необходимости) сигнал-листом.
    </p>
    <ul class="mt-3 list-disc space-y-1 pl-5 text-sm text-neutral-600 dark:text-neutral-300">
      <li>Добавьте sequence в Test Run и выберите workspace allocation.</li>
      <li>Назначьте физические каналы или сигналы в разделе Signals → Test Runs.</li>
      <li>Стартуйте/останавливайте выполнение только из Test Run history.</li>
    </ul>
    <div class="mt-4 flex flex-wrap gap-2">
      <UiButton size="sm" @click="openTestRuns">Открыть Test Runs</UiButton>
      <UiButton variant="ghost" size="sm" @click="openSignals">К сигналам</UiButton>
    </div>
  </section>
</template>
