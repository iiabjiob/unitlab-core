<template>
  <div class="w-full">
    <label v-if="label" class="block text-sm font-medium text-gray-700 mb-1">
      {{ label }}
    </label>
    <input
      :type="type"
      :placeholder="placeholder"
      class="w-full border px-1.5 py-0.5 rounded-xs focus:outline-none focus:ring focus:ring-blue-300"
      :autocomplete="autocomplete"
      v-model="inputValue"
    />
  </div>
</template>

<script setup>
import { ref, defineProps, defineEmits, watch } from "vue";

const props = defineProps({
  modelValue: String,  // Поддержка v-model
  type: {
    type: String,
    default: "text",  // Может быть "text" или "password"
  },
  label: String,
  placeholder: String,
  autocomplete: {
    type: String,
    default: "off",
  }
});

const emits = defineEmits(["update:modelValue"]);
const inputValue = ref(props.modelValue);

// Следим за изменением и передаем наверх
watch(inputValue, (newValue) => {
  emits("update:modelValue", newValue);
});
</script>
