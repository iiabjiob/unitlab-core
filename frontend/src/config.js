export const config = {
  app_name: null,
  app_version: null,
  app_env: null,
  description: null,
  debug: null,
  host: "localhost",
  health_check_interval: 5000,
};

// Функция загрузки конфигурации с бэкенда
export async function loadConfig() {
  try {
    const response = await fetch("/api/config");
    const data = await response.json();

    config.app_name = data.app_name;
    config.app_version = data.app_version;
    config.app_env = data.app_env;
    config.description = data.description;
    config.debug = data.debug;
    config.host = data.host || config.host;
    config.health_check_interval = (data.health_check_interval || 5) * 1000;

    console.log("✅ Config loaded:", config);
  } catch (error) {
    console.error("⚠️ Failed to load config:", error);
  }
}

// Function to get the full MQTT WebSocket URL dynamically
export function getMqttUrl(port = 9001) {
  return `ws://${config.host}:${port}`;
}
