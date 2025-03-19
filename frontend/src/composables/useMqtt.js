import { ref, onMounted, onUnmounted } from "vue";
import mqtt from "mqtt";
import { getMqttUrl } from "@/config"; // Import function to get MQTT URL

export function useMqtt(topic, port = 9001) {
  const brokerUrl = getMqttUrl(port); // Generate MQTT URL dynamically
  const client = mqtt.connect(brokerUrl);
  const data = ref(null);

  // Message handler
  const onMessage = (receivedTopic, message) => {
    if (receivedTopic !== topic) return; // Ignore messages from other topics
    try {
      data.value = JSON.parse(message.toString());
    } catch (error) {
      console.error(`Error processing MQTT message from topic '${topic}':`, error);
    }
  };

  onMounted(() => {
    client.on("connect", () => {
      console.log(`🔗 MQTT connected to ${brokerUrl} (subscribing to topic: ${topic})`);
      client.subscribe(topic, (err) => {
        if (err) console.error(`❌ Subscription error for topic '${topic}':`, err);
      });
    });

    client.on("message", onMessage);

    client.on("disconnect", () => {
      console.log(`🔴 MQTT disconnected (topic: ${topic})`);
    });

    client.on("error", (error) => {
      console.error(`❌ MQTT error on topic '${topic}':`, error);
    });
  });

  onUnmounted(() => {
    console.log(`🚪 Disconnecting from MQTT (topic: ${topic})`);
    client.end();
  });

  return { data };
}
