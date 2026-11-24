export type Probe = {
  ok: boolean
  ms: number
  host?: string
}

export type PcStatus = {
  ok: boolean
  path: 'ethernet' | 'wifi' | 'both' | 'none' | 'unset' | string
  ethernet: Probe
  wifi: Probe
}

export type Status = {
  name: string
  version: string
  uptime_s: number
  wifi_ssid: string
  wifi_rssi: number
  ip: string
  gateway_ip: string
  mdns: string
  time_epoch: number
  last_change_uptime_s: number
  checks_ok: number
  checks_fail: number
  interval_ms: number
  gateway: Probe
  internet: Probe
  extra: Probe
  pc?: PcStatus
  online: boolean
}

export type HistoryEvent = {
  uptime_s: number
  epoch: number
  kind: 'up' | 'down' | string
  target: string
  detail: string
}

export type HistoryResponse = {
  events: HistoryEvent[]
}
