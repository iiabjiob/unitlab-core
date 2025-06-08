export const WsTopicBuilder = {
  timeStatus: () => 'time_status',
  systemInfo: () => 'system_info',
  deviceRegister: () => 'devices/register',
  unitStates: (unitId: string, deviceType: string) => `devices/${unitId}/${deviceType}/states`,
  getStates: (unitId: string) => `${unitId}/get/states`,
}
