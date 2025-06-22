export interface Device {
  unit_id: string             // Уникальный ID устройства (обычно MAC)
  type: string                // Тип (например: 'core', 'do', 'di', 'ao')
  channels: number            // Количество каналов (выходов/входов)
  is_active: boolean          // Добавлено ли в систему, управляется ли
  is_online: boolean          // Сейчас ли устройство онлайн (heartbeat)
  name?: string               // Человеко-понятное имя (CoreUnit 1)
  last_seen?: string          // Время последнего ping/heartbeat (ISO)
  firmware?: string           // Версия прошивки (если актуально)
}
