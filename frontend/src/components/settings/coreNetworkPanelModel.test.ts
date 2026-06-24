import { describe, expect, it } from "vitest"
import { buildCoreNetworkSettingsPayload, createCoreNetworkDraft, splitList } from "./coreNetworkPanelModel"

describe("coreNetworkPanelModel", () => {
  it("splits comma-separated lists", () => {
    expect(splitList(" localhost, 127.0.0.1, 10.0.0.0/8 ")).toEqual([
      "localhost",
      "127.0.0.1",
      "10.0.0.0/8",
    ])
  })

  it("creates a draft from the host network snapshot", () => {
    const draft = createCoreNetworkDraft(
      {
        interface: "eth0",
        profile: "unitlab-lan",
        ipv4_mode: "manual",
        address_cidr: "192.168.10.21/24",
        gateway: "192.168.10.1",
        dns_servers: ["192.168.10.1", "1.1.1.1"],
        proxy_url: "http://proxy:3128",
        proxy_no_proxy: ["localhost", "127.0.0.1"],
      },
      "eth1",
    )

    expect(draft).toEqual({
      interface: "eth0",
      profile: "unitlab-lan",
      ipv4Mode: "manual",
      ipAddress: "192.168.10.21",
      prefixLength: "24",
      gateway: "192.168.10.1",
      dnsServers: "192.168.10.1, 1.1.1.1",
      proxyUrl: "http://proxy:3128",
      proxyNoProxy: "localhost, 127.0.0.1",
    })
  })

  it("builds a network settings payload from the draft", () => {
    const payload = buildCoreNetworkSettingsPayload({
      interface: " eth0 ",
      profile: " unitlab-lan ",
      ipv4Mode: "manual",
      ipAddress: "192.168.10.21",
      prefixLength: "24",
      gateway: "192.168.10.1",
      dnsServers: "192.168.10.1, 1.1.1.1",
      proxyUrl: "http://proxy:3128",
      proxyNoProxy: "localhost, 127.0.0.1",
    })

    expect(payload).toEqual({
      interface: "eth0",
      profile: "unitlab-lan",
      ipv4_mode: "manual",
      address_cidr: "192.168.10.21/24",
      gateway: "192.168.10.1",
      dns_servers: ["192.168.10.1", "1.1.1.1"],
      proxy_url: "http://proxy:3128",
      proxy_no_proxy: ["localhost", "127.0.0.1"],
    })
  })
})
