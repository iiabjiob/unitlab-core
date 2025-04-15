const moduleId = (unit: string): string => unit

export const TopicBuilder = {
  set:      (index: number, unit: string) => `${moduleId(unit)}/set/${index}`,
  group:    (unit: string)                => `${moduleId(unit)}/set/group`,
  status:   (index: number, unit: string) => `${moduleId(unit)}/status/${index}`,
  statusAll:(unit: string)                => `${moduleId(unit)}/status/`,
  subscribe:(unit: string)                => `${moduleId(unit)}/status/#`,
  getStatus:(unit: string)                => `${moduleId(unit)}/get/status`,
}
