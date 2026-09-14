export function buildCoreDiagnosticsIssueSignature(mode: string, issues: string[]): string {
  const stableIssues = issues.map(issue => {
    const normalized = issue.trim().toLowerCase()
    if (normalized.startsWith("cpu temp")) return "cpu_temperature"
    if (normalized.startsWith("memory ")) return "memory_pressure"
    if (normalized.startsWith("disk ")) return "disk_pressure"
    if (normalized.startsWith("services inactive (")) {
      const services = normalized
        .slice("services inactive (".length)
        .replace(/\)$/, "")
        .split(",")
        .map(service => service.trim())
        .filter(Boolean)
        .filter(service => service !== "…")
        .sort()
      return `inactive_services:${services.join(",")}`
    }
    return normalized
  }).sort()
  return JSON.stringify({ mode, issues: stableIssues })
}
