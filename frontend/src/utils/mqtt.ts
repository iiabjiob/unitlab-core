const moduleId = (unit: string): string => unit

export const TopicBuilder = {
  set:      (index: number, unit: string) => `${moduleId(unit)}/set/${index}`,
  group:    (unit: string)                => `${moduleId(unit)}/set/group`,
  getStates:(unit: string)                => `${moduleId(unit)}/get/states`,

  deviceRegister:()                       => `device/register`,
  deviceRegisterAnnounce:(unit: string)   => `device/register/${moduleId(unit)}`, // Кто зарегистрировался
  deviceScan:()                           => `device/scan`,
}
