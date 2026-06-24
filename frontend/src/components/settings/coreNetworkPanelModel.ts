import type { CoreNetHostNetworkSettings, CoreNetNetworkInterfaceInfo, CoreNetIpv4Mode } from "@/types/coreNetwork"

export interface CoreNetworkDraft {
  interface: string
  profile: string
  ipv4Mode: CoreNetIpv4Mode
  ipAddress: string
  prefixLength: string
  gateway: string
  dnsServers: string
  proxyUrl: string
  proxyNoProxy: string
}

export interface CoreNetworkInterfaceChoice {
  value: string
  label: string
  selected: boolean
}

export function splitList(value: string): string[] {
  return value
    .split(",")
    .map(part => part.trim())
    .filter(Boolean)
}

export function buildInterfaceDisplayLabel(entry: CoreNetNetworkInterfaceInfo): string {
  const parts = [entry.interface_name]
  if (entry.device_type) {
    parts.push(entry.device_type)
  }
  if (entry.is_default_route) {
    parts.push("default route")
  }
  if (entry.local_ip) {
    parts.push(entry.netmask ? `${entry.local_ip}/${entry.netmask}` : entry.local_ip)
  }
  return parts.join(" · ")
}

export function buildInterfaceDetailText(entry: CoreNetNetworkInterfaceInfo | null | undefined): string {
  if (!entry) return ""
  const parts: string[] = []
  if (entry.state) parts.push(entry.state)
  if (entry.connection) parts.push(entry.connection)
  if (entry.network) parts.push(entry.network)
  if (entry.is_default_route) parts.push("default route")
  return parts.join(" · ")
}

export function buildCoreNetworkInterfaceWarnings(entry: CoreNetNetworkInterfaceInfo | null | undefined): string[] {
  if (!entry) return []
  const warnings: string[] = []
  if (entry.carrier === false) {
    warnings.push("No carrier detected on this interface")
  }
  if (!entry.local_ip) {
    warnings.push("No IPv4 address detected")
  }
  if (!isCoreNetworkInterfaceHealthy(entry)) {
    warnings.push(`Interface state: ${entry.state || entry.oper_state}`)
  }
  return warnings
}

export function buildRecommendedCoreNetworkInterface(
  interfaces: CoreNetNetworkInterfaceInfo[],
  fallbackInterface: string,
): CoreNetNetworkInterfaceInfo | null {
  if (!interfaces.length) return null
  const normalizedFallback = fallbackInterface.trim()
  const ranked = [...interfaces].sort((left, right) => {
    return scoreCoreNetworkInterface(right, normalizedFallback) - scoreCoreNetworkInterface(left, normalizedFallback)
  })
  return ranked[0] ?? null
}

function scoreCoreNetworkInterface(entry: CoreNetNetworkInterfaceInfo, fallbackInterface: string): number {
  let score = 0
  if (entry.is_default_route) score += 1000
  if ((entry.device_type || "").toLowerCase() === "ethernet") score += 200
  if (entry.carrier === true) score += 80
  if (isCoreNetworkInterfaceHealthy(entry)) score += 60
  if (entry.local_ip) score += 40
  if (entry.interface_name === fallbackInterface) score += 20
  return score
}

function isCoreNetworkInterfaceHealthy(entry: CoreNetNetworkInterfaceInfo): boolean {
  const state = (entry.state || "").toLowerCase()
  const operState = (entry.oper_state || "").toLowerCase()
  return state.startsWith("connected") || state.startsWith("activated") || state === "up" || operState === "up"
}

export function buildCoreNetworkInterfaceChoices(
  interfaces: CoreNetNetworkInterfaceInfo[],
  selectedInterface: string,
): CoreNetworkInterfaceChoice[] {
  const choices = interfaces.map((entry) => ({
    value: entry.interface_name,
    label: buildInterfaceDisplayLabel(entry),
    selected: entry.interface_name === selectedInterface,
  }))
  if (selectedInterface && !choices.some(choice => choice.value === selectedInterface)) {
    choices.unshift({
      value: selectedInterface,
      label: `${selectedInterface} · configured`,
      selected: true,
    })
  }
  return choices
}

export function getSelectedCoreNetworkInterface(
  interfaces: CoreNetNetworkInterfaceInfo[],
  selectedInterface: string,
): CoreNetNetworkInterfaceInfo | null {
  return interfaces.find(entry => entry.interface_name === selectedInterface) ?? null
}

export function createCoreNetworkDraft(
  snapshot: CoreNetHostNetworkSettings | null | undefined,
  fallbackInterface: string,
): CoreNetworkDraft {
  const addressParts = snapshot?.address_cidr?.split("/", 2) ?? []
  return {
    interface: snapshot?.interface || fallbackInterface,
    profile: snapshot?.profile || "unitlab-lan",
    ipv4Mode: snapshot?.ipv4_mode || "auto",
    ipAddress: addressParts[0] || "",
    prefixLength: addressParts[1] || "24",
    gateway: snapshot?.gateway || "",
    dnsServers: snapshot?.dns_servers?.join(", ") || "",
    proxyUrl: snapshot?.proxy_url || "",
    proxyNoProxy: snapshot?.proxy_no_proxy?.join(", ") || "",
  }
}

export function buildCoreNetworkSettingsPayload(draft: CoreNetworkDraft) {
  const addressCidr = draft.ipv4Mode === "manual" && draft.ipAddress.trim()
    ? `${draft.ipAddress.trim()}/${Number.parseInt(draft.prefixLength, 10) || 24}`
    : null
  return {
    interface: draft.interface.trim() || null,
    profile: draft.profile.trim() || null,
    ipv4_mode: draft.ipv4Mode,
    address_cidr: addressCidr,
    gateway: draft.gateway.trim() || null,
    dns_servers: splitList(draft.dnsServers),
    proxy_url: draft.proxyUrl.trim() || null,
    proxy_no_proxy: splitList(draft.proxyNoProxy),
  }
}
