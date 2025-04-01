<template>
  <div>
    <PageHeader title="Dashboard" />

    <div class="grid grid-cols-1 gap-6 mt-6">
      <div>

        <h2 class="text-lg font-semibold mb-2">Digital Outputs</h2>
        <div class="space-x-2">
          <button class="btn btn-sm" @click="setAll(true)">Включить все</button>
          <button class="btn btn-sm" @click="setAll(false)">Выключить все</button>
        </div>
        <div class="grid grid-cols-5 gap-2">
          <div
            v-for="doItem in doStatuses"
            :key="doItem.name"
            class="p-2 rounded text-center border"
            :class="doItem.state ? 'bg-emerald-500 text-white' : 'bg-gray-200 text-gray-900'"
          >
            {{ doItem.name }}
            <div class="flex justify-center gap-1">
              <button class="btn" @click="toggleDO(parseInt(doItem.name.replace('do', '')), doItem.state)">toggle</button>
              <button class="btn" @click="pulseDO(parseInt(doItem.name.replace('do', '')))">pulse</button>
            </div>
          </div>
        </div>
      </div>

      <div>
        <h2 class="text-lg font-semibold mb-2">Digital Inputs</h2>
        <div class="grid grid-cols-5 gap-2">
          <div
            v-for="diItem in diStatuses"
            :key="diItem.name"
            class="p-2 rounded text-center border"
            :class="diItem.state ? 'bg-blue-500 text-white' : 'bg-gray-200 text-gray-900'"
          >
            {{ diItem.name }}

          </div>

        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted } from "vue";
import PageHeader from "@/components/PageHeader.vue";
import { useWebSocketStore } from "@/stores/websocket";

const wsStore = useWebSocketStore();

const doBoardId = "do-board-1"; // позже будет динамически

onMounted(() => {
  wsStore.subscribe(["mqtt_do_boards", "mqtt_di_boards"]);
});

onUnmounted(() => {
  wsStore.unsubscribe(["mqtt_do_boards", "mqtt_di_boards"]);
});

const doStatuses = computed(() => {
  return Object.entries(wsStore.receivedData)
    .filter(([key]) => key.startsWith(`${doBoardId}/status/`))
    .map(([key, value]) => {
      return {
        name: key.split("/").pop(), // "do1"
        state: value === "true"
      }
    });
});

const diStatuses = computed(() => {
  return Object.entries(wsStore.receivedData)
    .filter(([key]) => key.startsWith("di-board-1/status/"))
    .map(([key, value]) => {
      return {
        name: key.split("/").pop(), // "di1"
        state: value === "true"
      }
    });
});

function toggleDO(index, currentState) {
  wsStore.send({
    action: "publish",
    topic: `${doBoardId}/set/do${index}`,
    payload: { state: !currentState }
  });
}

function pulseDO(index) {
  wsStore.send({
    action: "publish",
    topic: `${doBoardId}/set/do${index}`,
    payload: { pulse_ms: 1000 } // или любую нужную длительность
  });
}

function setAll(state) {
  const actions = doStatuses.value.map((doItem) => ({
    index: parseInt(doItem.name.replace("do", "")),
    state
  }));

  wsStore.send({
    action: "publish",
    topic: `${doBoardId}/set/group`,
    payload: { actions }
  });
}

</script>
