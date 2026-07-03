import type { CoreNetNetworkInterfaceInfo } from "@/types/coreNetwork"

export interface ImportSubnetCandidate {
  cidr: string
  networkAddress: string
  prefixLength: number
  subnetMask: string
  deviceCount: number
  deviceIps: string[]
}

const IPV4_PATTERN = /(?:^|[^\d])((?:\d{1,3}\.){3}\d{1,3})(?:[^\d]|$)/

export function normalizeIpv4Address(value: unknown): string | null {
  if (value === null || value === undefined) return null
  const text = String(value).trim()
  if (!text) return null
  const match = text.match(IPV4_PATTERN)
  const candidate = match?.[1] ?? text
  const parts = candidate.split(".")
  if (parts.length !== 4) return null
  const octets = parts.map(part => {
    if (!/^\d{1,3}$/.test(part)) return Number.NaN
    return Number.parseInt(part, 10)
  })
  if (octets.some(octet => !Number.isInteger(octet) || octet < 0 || octet > 255)) return null
  return octets.join(".")
}

export function collectUniqueDeviceIps(rows: unknown[][], columnIndex: number | null): string[] {
  if (columnIndex === null) return []
  const ips = new Set<string>()
  for (let rowIndex = 1; rowIndex < rows.length; rowIndex += 1) {
    const row = Array.isArray(rows[rowIndex]) ? rows[rowIndex] : []
    const ip = normalizeIpv4Address(row[columnIndex])
    if (ip) ips.add(ip)
  }
  return Array.from(ips).sort(compareIpv4)
}

export function inferProjectSubnets(deviceIps: string[], prefixLength = 24): ImportSubnetCandidate[] {
  const prefix = clampPrefix(prefixLength)
  const groups = new Map<string, string[]>()
  for (const ip of deviceIps) {
    const ipInt = ipv4ToInt(ip)
    if (ipInt === null) continue
    const networkInt = networkIntForPrefix(ipInt, prefix)
    const networkAddress = intToIpv4(networkInt)
    const cidr = `${networkAddress}/${prefix}`
    const group = groups.get(cidr) ?? []
    group.push(ip)
    groups.set(cidr, group)
  }
  return Array.from(groups.entries())
    .map(([cidr, ips]) => {
      const [networkAddress, prefixRaw] = cidr.split("/")
      return {
        cidr,
        networkAddress,
        prefixLength: Number.parseInt(prefixRaw, 10),
        subnetMask: prefixToSubnetMask(Number.parseInt(prefixRaw, 10)),
        deviceCount: ips.length,
        deviceIps: Array.from(new Set(ips)).sort(compareIpv4),
      }
    })
    .sort((left, right) => right.deviceCount - left.deviceCount || compareIpv4(left.networkAddress, right.networkAddress))
}

export function suggestStaticPiAddress(
  subnetCidr: string | null,
  deviceIps: string[],
  occupiedIps: string[] = [],
): string | null {
  if (!subnetCidr) return null
  const parsed = parseCidr(subnetCidr)
  if (!parsed) return null
  const blocked = new Set([...deviceIps, ...occupiedIps].map(ip => normalizeIpv4Address(ip)).filter(Boolean) as string[])
  const hostOctetPreference = [
    ...range(10, 49),
    ...range(2, 9),
    ...range(50, 254),
  ]
  const networkStart = parsed.networkInt
  const networkEnd = parsed.broadcastInt
  for (const hostOctet of hostOctetPreference) {
    const candidateInt = ((networkStart & 0xffffff00) >>> 0) + hostOctet
    if (candidateInt <= networkStart || candidateInt >= networkEnd) continue
    const candidate = intToIpv4(candidateInt)
    if (!blocked.has(candidate)) return candidate
  }
  for (let candidateInt = networkStart + 1; candidateInt < networkEnd; candidateInt += 1) {
    const candidate = intToIpv4(candidateInt)
    if (!blocked.has(candidate)) return candidate
  }
  return null
}

export function selectRj45Interface(
  interfaces: CoreNetNetworkInterfaceInfo[],
  configuredInterface?: string | null,
): CoreNetNetworkInterfaceInfo | null {
  const candidates = interfaces.filter(entry => {
    const type = (entry.device_type || "").toLowerCase()
    const name = entry.interface_name.toLowerCase()
    return type !== "wifi" && !name.startsWith("wl")
  })
  if (!candidates.length) return null
  return [...candidates].sort((left, right) => {
    return scoreRj45Interface(right, configuredInterface) - scoreRj45Interface(left, configuredInterface)
  })[0] ?? null
}

export function isIpInSubnet(ip: string | null | undefined, subnetCidr: string | null): boolean {
  if (!ip || !subnetCidr) return false
  const parsed = parseCidr(subnetCidr)
  const ipInt = ipv4ToInt(ip)
  if (!parsed || ipInt === null) return false
  return ipInt > parsed.networkInt && ipInt < parsed.broadcastInt
}

export function subnetMaskForCidr(subnetCidr: string | null): string {
  const parsed = parseCidr(subnetCidr)
  return parsed ? prefixToSubnetMask(parsed.prefixLength) : ""
}

function scoreRj45Interface(entry: CoreNetNetworkInterfaceInfo, configuredInterface?: string | null): number {
  const type = (entry.device_type || "").toLowerCase()
  const name = entry.interface_name.toLowerCase()
  let score = 0
  if (type === "ethernet") score += 300
  if (name === "eth0" || name === "end0") score += 220
  if (configuredInterface && entry.interface_name === configuredInterface) score += 100
  if (entry.carrier === true) score += 80
  if (entry.local_ip) score += 20
  if (entry.is_default_route) score += 10
  return score
}

function parseCidr(cidr: string | null): { networkInt: number; broadcastInt: number; prefixLength: number } | null {
  if (!cidr) return null
  const [networkAddress, prefixRaw] = cidr.split("/")
  const networkIpInt = ipv4ToInt(networkAddress)
  const prefixLength = Number.parseInt(prefixRaw, 10)
  if (networkIpInt === null || !Number.isInteger(prefixLength) || prefixLength < 1 || prefixLength > 30) return null
  const networkInt = networkIntForPrefix(networkIpInt, prefixLength)
  const hostCount = 2 ** (32 - prefixLength)
  return { networkInt, broadcastInt: networkInt + hostCount - 1, prefixLength }
}

function ipv4ToInt(ip: string): number | null {
  const normalized = normalizeIpv4Address(ip)
  if (!normalized) return null
  return normalized.split(".").reduce((acc, part) => (acc * 256) + Number.parseInt(part, 10), 0)
}

function intToIpv4(value: number): string {
  return [
    (value >>> 24) & 255,
    (value >>> 16) & 255,
    (value >>> 8) & 255,
    value & 255,
  ].join(".")
}

function networkIntForPrefix(ipInt: number, prefixLength: number): number {
  const mask = prefixLength === 0 ? 0 : (0xffffffff << (32 - prefixLength)) >>> 0
  return (ipInt & mask) >>> 0
}

function prefixToSubnetMask(prefixLength: number): string {
  const prefix = clampPrefix(prefixLength)
  return intToIpv4((0xffffffff << (32 - prefix)) >>> 0)
}

function clampPrefix(prefixLength: number): number {
  if (!Number.isInteger(prefixLength)) return 24
  return Math.min(30, Math.max(1, prefixLength))
}

function range(start: number, end: number): number[] {
  const values: number[] = []
  for (let value = start; value <= end; value += 1) values.push(value)
  return values
}

function compareIpv4(left: string, right: string): number {
  return (ipv4ToInt(left) ?? 0) - (ipv4ToInt(right) ?? 0)
}
