export const config = {
  app_name: null,
  app_version: null,
  app_env: null,
  description: null,
  debug: null,
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

    console.log("✅ Config loaded:", config);
  } catch (error) {
    console.error("⚠️ Failed to load config:", error);
  }
}
