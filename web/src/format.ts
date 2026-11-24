export function formatUptime(sec: number): string {
  const s = Math.max(0, Math.floor(sec))
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  const r = s % 60
  if (d > 0) return `${d}d ${h}h ${m}m`
  if (h > 0) return `${h}h ${m}m ${r}s`
  if (m > 0) return `${m}m ${r}s`
  return `${r}s`
}

export function formatEpoch(epoch: number): string {
  if (!epoch || epoch < 1_000_000_000) return '----/--/-- --:--:--'
  return new Date(epoch * 1000).toLocaleString(undefined, {
    hour12: false,
  })
}

export function pad(v: string | number, n: number): string {
  return String(v).padStart(n, ' ')
}
