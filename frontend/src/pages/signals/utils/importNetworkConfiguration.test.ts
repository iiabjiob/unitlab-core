import { describe, expect, it } from "vitest"

import {
  collectUniqueDeviceIps,
  inferProjectSubnets,
  isIpInSubnet,
  selectRj45Interface,
  suggestStaticPiAddress,
} from "./importNetworkConfiguration"

describe("importNetworkConfiguration", () => {
  it("collects unique valid device IPs from a selected column", () => {
    const rows = [
      ["Name", "IP"],
      ["IED1", "192.168.10.21"],
      ["IED2", "mms://192.168.10.22:102"],
      ["IED3", "999.168.10.23"],
      ["IED4", "192.168.10.21"],
    ]

    expect(collectUniqueDeviceIps(rows, 1)).toEqual(["192.168.10.21", "192.168.10.22"])
  })

  it("groups imported IPs by /24 subnet", () => {
    const subnets = inferProjectSubnets(["192.168.11.2", "192.168.10.5", "192.168.10.6"])

    expect(subnets.map(item => ({ cidr: item.cidr, count: item.deviceCount }))).toEqual([
      { cidr: "192.168.10.0/24", count: 2 },
      { cidr: "192.168.11.0/24", count: 1 },
    ])
  })

  it("suggests low engineering-tool addresses without colliding with imported devices", () => {
    expect(suggestStaticPiAddress("192.168.10.0/24", ["192.168.10.10", "192.168.10.11"])).toBe("192.168.10.12")
    expect(suggestStaticPiAddress("192.168.10.0/24", ["192.168.10.10"], ["192.168.10.11"])).toBe("192.168.10.12")
  })

  it("detects whether the current Pi IP is inside the project subnet", () => {
    expect(isIpInSubnet("192.168.10.12", "192.168.10.0/24")).toBe(true)
    expect(isIpInSubnet("192.168.11.12", "192.168.10.0/24")).toBe(false)
  })

  it("prefers wired RJ45 interfaces and skips Wi-Fi", () => {
    const selected = selectRj45Interface([
      {
        interface_name: "wlan0",
        device_type: "wifi",
        local_ip: "10.42.0.1",
        netmask: "24",
        network: "10.42.0.0/24",
      },
      {
        interface_name: "end0",
        device_type: "ethernet",
        local_ip: null,
        netmask: null,
        network: null,
        carrier: true,
      },
    ])

    expect(selected?.interface_name).toBe("end0")
  })
})
