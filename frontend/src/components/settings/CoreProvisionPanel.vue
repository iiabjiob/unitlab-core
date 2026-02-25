<template>
  <section class="rounded-2xl border border-neutral-200/70 bg-white/90 p-4 dark:border-neutral-800 dark:bg-neutral-950/40">
    <div class="flex flex-wrap items-start justify-between gap-3">
      <div class="min-w-0">
        <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Provisioning</p>
        <p class="mt-1 text-sm text-neutral-700 dark:text-neutral-200">
          Mode: <span class="font-semibold uppercase">{{ modeText }}</span>
          <template v-if="snapshot?.last_event"> · {{ snapshot.last_event }}</template>
        </p>
        <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          Project root: <code class="rounded bg-neutral-100 px-1 py-0.5 dark:bg-neutral-800">{{ snapshot?.project_root || "—" }}</code>
        </p>
        <p v-if="errorText" class="mt-1 text-xs text-rose-500">{{ errorText }}</p>
      </div>
      <div class="flex flex-wrap items-center gap-2">
        <button
          type="button"
          class="rounded-lg border border-neutral-300 px-3 py-1.5 text-xs font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
          :disabled="busy"
          @click="runSmokeCheck"
        >
          Run smoke check
        </button>
      </div>
    </div>

    <div class="mt-3 grid gap-3 xl:grid-cols-[minmax(0,1.15fr)_minmax(0,0.85fr)]">
      <div class="space-y-3">
        <section class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
          <div class="mb-2 flex items-center justify-between gap-2">
            <p class="text-xs font-semibold text-neutral-700 dark:text-neutral-200">Provision checks</p>
            <span class="text-[11px] text-neutral-500 dark:text-neutral-400">{{ checks.length }} checks</span>
          </div>
          <ChecksList :items="checks" empty-text="No checks reported yet." />
        </section>

        <section class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
          <div class="mb-2 flex items-center justify-between gap-2">
            <p class="text-xs font-semibold text-neutral-700 dark:text-neutral-200">Smoke checks</p>
            <span class="text-[11px] text-neutral-500 dark:text-neutral-400">{{ smokeChecks.length }} checks</span>
          </div>
          <ChecksList :items="smokeChecks" empty-text="No smoke-check results yet. Run smoke check." />
        </section>
      </div>

      <div class="space-y-3">
        <section class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
          <p class="mb-2 text-xs font-semibold text-neutral-700 dark:text-neutral-200">Host Agent Install / Repair</p>
          <div class="grid gap-2">
            <button
              type="button"
              class="rounded-lg bg-neutral-900 px-3 py-2 text-xs font-semibold text-white hover:bg-neutral-700 disabled:cursor-not-allowed disabled:opacity-60 dark:bg-neutral-100 dark:text-neutral-900 dark:hover:bg-neutral-300"
              :disabled="busy"
              @click="installNetAgent"
            >
              Install / Repair Core Network Agent
            </button>
            <button
              type="button"
              class="rounded-lg bg-neutral-900 px-3 py-2 text-xs font-semibold text-white hover:bg-neutral-700 disabled:cursor-not-allowed disabled:opacity-60 dark:bg-neutral-100 dark:text-neutral-900 dark:hover:bg-neutral-300"
              :disabled="busy"
              @click="installNtpAgent"
            >
              Install / Repair NTP Agent
            </button>
            <button
              type="button"
              class="rounded-lg bg-neutral-900 px-3 py-2 text-xs font-semibold text-white hover:bg-neutral-700 disabled:cursor-not-allowed disabled:opacity-60 dark:bg-neutral-100 dark:text-neutral-900 dark:hover:bg-neutral-300"
              :disabled="busy"
              @click="installDiagAgent"
            >
              Install / Repair Diagnostics Agent
            </button>
          </div>

          <div v-if="snapshot?.request_in_flight" class="mt-3 rounded-lg border border-amber-200 bg-amber-50 px-2 py-1 text-[11px] text-amber-800 dark:border-amber-700/50 dark:bg-amber-950/40 dark:text-amber-200">
            In progress: {{ snapshot.request_in_flight.action }} ({{ snapshot.request_in_flight.request_id }})
          </div>
        </section>

        <section class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
          <p class="mb-2 text-xs font-semibold text-neutral-700 dark:text-neutral-200">Last action result</p>
          <div v-if="lastAction" class="space-y-1 text-xs">
            <p class="text-neutral-700 dark:text-neutral-200">
              <span class="font-semibold">{{ lastAction.action }}</span>
              ·
              <span :class="lastAction.success ? 'text-emerald-600 dark:text-emerald-300' : 'text-rose-600 dark:text-rose-300'">
                {{ lastAction.success ? "SUCCESS" : "FAILED" }}
              </span>
              <template v-if="lastAction.duration_ms != null"> · {{ lastAction.duration_ms }} ms</template>
              <template v-if="lastAction.exit_code != null"> · exit {{ lastAction.exit_code }}</template>
            </p>
            <p class="whitespace-pre-wrap break-words text-neutral-500 dark:text-neutral-400">{{ lastAction.message }}</p>
          </div>
          <p v-else class="text-xs text-neutral-500 dark:text-neutral-400">
            No provisioning action has been executed yet.
          </p>
        </section>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, defineComponent, h, onMounted, onUnmounted } from "vue"
import { useCoreProvisionStore } from "@/stores/coreProvisionStore"
import type { CoreProvisionCheck } from "@/types/coreProvision"

const store = useCoreProvisionStore()

const snapshot = computed(() => store.snapshot)
const checks = computed(() => store.checks)
const smokeChecks = computed(() => store.smokeChecks)
const lastAction = computed(() => store.lastAction)
const busy = computed(() => store.loading || store.commandPending)
const modeText = computed(() => String(store.mode).toUpperCase())
const errorText = computed(() => store.lastError || snapshot.value?.last_error || null)

async function runSmokeCheck() {
  await store.runSmokeCheck().catch(() => undefined)
}
async function installNetAgent() {
  await store.installNetAgent().catch(() => undefined)
}
async function installNtpAgent() {
  await store.installNtpAgent().catch(() => undefined)
}
async function installDiagAgent() {
  await store.installDiagAgent().catch(() => undefined)
}

onMounted(() => {
  store.startMonitoring()
})
onUnmounted(() => {
  store.stopMonitoring()
})

function statusText(ok: boolean | null): string {
  if (ok === true) return "OK"
  if (ok === false) return "FAIL"
  return "UNKNOWN"
}
function statusPillClass(ok: boolean | null): string {
  if (ok === true) return "bg-emerald-100 text-emerald-800 dark:bg-emerald-900/40 dark:text-emerald-200"
  if (ok === false) return "bg-rose-100 text-rose-800 dark:bg-rose-900/40 dark:text-rose-200"
  return "bg-neutral-200 text-neutral-700 dark:bg-neutral-800 dark:text-neutral-200"
}

const ChecksList = defineComponent({
  name: "CoreProvisionChecksList",
  props: {
    items: { type: Array as () => CoreProvisionCheck[], required: true },
    emptyText: { type: String, required: true },
  },
  setup(props) {
    return () => h("div", { class: "max-h-56 overflow-y-auto rounded-lg border border-neutral-200 bg-white/80 p-1 dark:border-neutral-800 dark:bg-neutral-950/50" }, [
      ...(props.items.length
        ? props.items.map((item) =>
            h("div", { class: "flex items-start justify-between gap-2 px-2 py-1 text-xs", key: item.key }, [
              h("div", { class: "min-w-0" }, [
                h("div", { class: "truncate text-neutral-800 dark:text-neutral-100" }, item.label),
                item.detail ? h("div", { class: "truncate text-[11px] text-neutral-500 dark:text-neutral-400" }, item.detail) : null,
              ]),
              h("span", { class: `shrink-0 rounded px-1.5 py-0.5 text-[10px] font-semibold ${statusPillClass(item.ok)}` }, statusText(item.ok)),
            ]),
          )
        : [h("p", { class: "px-2 py-2 text-xs text-neutral-500 dark:text-neutral-400" }, props.emptyText)]),
    ])
  },
})
</script>

