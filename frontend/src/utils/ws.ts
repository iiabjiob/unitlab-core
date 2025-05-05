export const WsTopicBuilder = {
  timeStatus: () => 'time_status',
  systemInfo: () => 'system_info',
  deviceRegister: () => 'devices/register',
  unitStates: (unitId: string) => `devices/${unitId}/states`,
  getStates: (unitId: string) => `${unitId}/get/states`,
}
