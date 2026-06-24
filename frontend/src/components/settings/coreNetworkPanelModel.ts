import type { CoreNetHostNetworkSettings, CoreNetIpv4Mode } from "@/types/coreNetwork"

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

export function splitList(value: string): string[] {
  return value
    .split(",")
    .map(part => part.trim())
    .filter(Boolean)
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
