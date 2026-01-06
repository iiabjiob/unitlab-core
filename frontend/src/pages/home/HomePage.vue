<template>
  <div class="h-full overflow-auto bg-[radial-gradient(circle_at_top,rgba(59,130,246,0.08),transparent_55%)] dark:bg-[radial-gradient(circle_at_top,rgba(59,130,246,0.15),transparent_60%)]">
    <section class="mx-auto flex max-w-5xl flex-col gap-8 px-4 py-10 sm:px-8 lg:px-0">

      <div class="rounded-[32px] border border-white/40 bg-white/70 p-8 text-neutral-900 shadow-[0_25px_60px_-30px_rgba(15,118,110,0.55)] backdrop-blur-2xl dark:border-neutral-800 dark:bg-neutral-900/80 dark:text-neutral-100">
        <p class="text-xs font-semibold uppercase tracking-[0.5em] text-neutral-500 dark:text-neutral-400">Workspace control</p>
        <h1 class="mt-4 text-3xl font-semibold leading-tight md:text-4xl">
          {{ heroTitle }}
        </h1>
        <p class="mt-4 max-w-3xl text-base text-neutral-600 dark:text-neutral-300">
          Active projects scope every sequence, switchgear, and log stream in the system. Pick a project below to hop into its dedicated sandbox or create a brand-new one for a clean slate.
        </p>

        <div class="mt-6">
          <ProjectSwitcher />
        </div>
      </div>

      <div class="grid gap-4 md:grid-cols-3">
        <article class="rounded-3xl border border-neutral-200/70 bg-white/80 p-5 text-sm text-neutral-700 shadow-sm backdrop-blur dark:border-neutral-800 dark:bg-neutral-900/60 dark:text-neutral-300">
          <p class="text-[11px] uppercase tracking-[0.5em] text-neutral-500 dark:text-neutral-400 mb-3">Sequences</p>
          <p class="text-base font-semibold text-neutral-900 dark:text-neutral-100">Versioned orchestration</p>
          <p class="mt-2 text-sm leading-relaxed">All step lists, runs, and telemetry pivot around the selected project, so you can iterate freely without touching other environments.</p>
        </article>

        <article class="rounded-3xl border border-neutral-200/70 bg-white/80 p-5 text-sm text-neutral-700 shadow-sm backdrop-blur dark:border-neutral-800 dark:bg-neutral-900/60 dark:text-neutral-300">
          <p class="text-[11px] uppercase tracking-[0.5em] text-neutral-500 dark:text-neutral-400 mb-3">Switchgears</p>
          <p class="text-base font-semibold text-neutral-900 dark:text-neutral-100">Scoped channel bindings</p>
          <p class="mt-2 text-sm leading-relaxed">Bindings, latencies, and hardware states are isolated per project so field tests never collide with production-ready wiring.</p>
        </article>

        <article class="rounded-3xl border border-neutral-200/70 bg-white/80 p-5 text-sm text-neutral-700 shadow-sm backdrop-blur dark:border-neutral-800 dark:bg-neutral-900/60 dark:text-neutral-300">
          <p class="text-[11px] uppercase tracking-[0.5em] text-neutral-500 dark:text-neutral-400 mb-3">Observability</p>
          <p class="text-base font-semibold text-neutral-900 dark:text-neutral-100">Clear audit trail</p>
          <p class="mt-2 text-sm leading-relaxed">Logs, WS events, and execution history are filtered by the active project to keep diagnostics razor-focused.</p>
        </article>
      </div>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import ProjectSwitcher from "@/components/projects/ProjectSwitcher.vue"
import { useProjectStore } from "@/stores/projectStore"

const projectStore = useProjectStore()
const heroTitle = computed(() =>
  projectStore.activeProject
    ? `Working inside "${projectStore.activeProject.name}"`
    : "Open a project to begin",
)
</script>
