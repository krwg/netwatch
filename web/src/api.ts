import type { HistoryResponse, Status } from './types'

let overrideBase: string | null = null

export function setApiBase(base: string) {
  overrideBase = base.trim().replace(/\/$/, '') || null
}

export function getApiBase(): string {
  if (overrideBase !== null) return overrideBase
  const fromEnv = import.meta.env.VITE_API_BASE as string | undefined
  if (fromEnv && fromEnv.length > 0) return fromEnv.replace(/\/$/, '')
  return ''
}

async function getJson<T>(path: string): Promise<T> {
  const res = await fetch(`${getApiBase()}${path}`, {
    headers: { Accept: 'application/json' },
  })
  if (!res.ok) throw new Error(`${path} → HTTP ${res.status}`)
  return res.json() as Promise<T>
}

export const fetchStatus = () => getJson<Status>('/api/status')
export const fetchHistory = () => getJson<HistoryResponse>('/api/history')
