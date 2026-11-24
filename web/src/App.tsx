import { useEffect, useMemo, useState } from 'react'
import { fetchHistory, fetchStatus, getApiBase, setApiBase } from './api'
import { formatEpoch, formatUptime, pad } from './format'
import type { HistoryEvent, Status } from './types'

type ConnState = 'booting' | 'live' | 'error'

const POLL_MS = 2000

export default function App() {
  const [status, setStatus] = useState<Status | null>(null)
  const [events, setEvents] = useState<HistoryEvent[]>([])
  const [conn, setConn] = useState<ConnState>('booting')
  const [error, setError] = useState<string | null>(null)
  const [ticks, setTicks] = useState(0)
  const [espBase, setEspBase] = useState(() => getApiBase())
  const [appliedBase, setAppliedBase] = useState(() => getApiBase())

  useEffect(() => {
    setApiBase(appliedBase)
  }, [appliedBase])

  useEffect(() => {
    let cancelled = false
    let timer: number | undefined

    const tick = async () => {
      try {
        const [s, h] = await Promise.all([fetchStatus(), fetchHistory()])
        if (cancelled) return
        setStatus(s)
        setEvents([...h.events].reverse())
        setConn('live')
        setError(null)
        setTicks((t) => t + 1)
      } catch (e) {
        if (cancelled) return
        setConn('error')
        setError(e instanceof Error ? e.message : String(e))
      } finally {
        if (!cancelled) timer = window.setTimeout(tick, POLL_MS)
      }
    }

    void tick()
    return () => {
      cancelled = true
      if (timer) window.clearTimeout(timer)
    }
  }, [appliedBase])

  const lines = useMemo(() => {
    if (!status) return [] as string[]
    const online = status.online ? 'ONLINE' : 'OFFLINE'
    const pc = status.pc
    const out = [
      `host      ${status.mdns}   ip ${status.ip}`,
      `wifi      ${status.wifi_ssid || '—'}   rssi ${status.wifi_rssi} dBm`,
      `uptime    ${formatUptime(status.uptime_s)}`,
      `checks    ok=${status.checks_ok}  fail=${status.checks_fail}  every ${status.interval_ms}ms`,
      ``,
      `gateway   ${flag(status.gateway.ok)}  ${ms(status.gateway.ms)}   ${status.gateway_ip}`,
      `internet  ${flag(status.internet.ok)}  ${ms(status.internet.ms)}   ${status.internet.host}`,
      `extra     ${flag(status.extra.ok)}  ${ms(status.extra.ms)}   ${status.extra.host}`,
    ]
    if (pc) {
      out.push(
        ``,
        `pc       ${flag(pc.ok)}  path=${pc.path}`,
        `  eth    ${flag(pc.ethernet.ok)}  ${ms(pc.ethernet.ms)}   ${pc.ethernet.host || '—'}`,
        `  wifi   ${flag(pc.wifi.ok)}  ${ms(pc.wifi.ms)}   ${pc.wifi.host || '—'}`,
      )
    }
    out.push(
      ``,
      `link      ${online}   last_change_uptime ${formatUptime(status.last_change_uptime_s)}`,
    )
    return out
  }, [status])

  return (
    <div className="crt">
      <div className="scan" aria-hidden />
      <header className="top">
        <div className="brand">
          <span className="prompt">root@netwatch</span>
          <span className="dim">:</span>
          <span className="path">~</span>
          <span className="dim">#</span>
          <span className="cmd"> ./monitor</span>
        </div>
        <div className={`badge badge-${conn}`}>
          {conn === 'live' && `LIVE · poll #${ticks}`}
          {conn === 'booting' && 'CONNECTING…'}
          {conn === 'error' && 'LINK DOWN'}
        </div>
      </header>

      <h1 className="title">NETWATCH</h1>
      <p className="tag">
        always-on link sentry · esp32 · no modules · local only
      </p>

      <section className="panel">
        <div className="panel-h">status</div>
        <pre className="term">
          {conn === 'error' && (
            <span className="err">
              {`! cannot reach ESP API\n! ${error}\n! enter board IP below (http://192.168.x.x) and press Apply\n`}
            </span>
          )}
          {lines.map((l, i) => (
            <div key={`l-${i}`}>{l || ' '}</div>
          ))}
          {conn === 'live' && <span className="cursor">█</span>}
        </pre>
      </section>

      <section className="panel">
        <div className="panel-h">event log</div>
        <pre className="term log">
          {events.length === 0 && <span className="dim">— waiting for events —</span>}
          {events.map((e, i) => (
            <div
              key={`${e.uptime_s}-${e.kind}-${e.detail}-${i}`}
              className={e.kind === 'down' ? 'down' : 'up'}
            >
              {`${formatEpoch(e.epoch)}  [${pad(e.kind.toUpperCase(), 4)}]  ${e.target.padEnd(8)}  ${e.detail}`}
            </div>
          ))}
        </pre>
      </section>

      <footer className="foot">
        <form
          className="api"
          onSubmit={(ev) => {
            ev.preventDefault()
            let next = espBase.trim()
            if (next && !/^https?:\/\//i.test(next)) next = `http://${next}`
            setEspBase(next)
            setAppliedBase(next)
            setConn('booting')
          }}
        >
          <span className="dim">api</span>
          <input
            value={espBase}
            placeholder="http://netwatch.local or http://192.168.x.x"
            onChange={(ev) => setEspBase(ev.target.value)}
            spellCheck={false}
          />
          <button type="submit">Apply</button>
        </form>
        <span className="dim">
          first boot: join Wi-Fi <code>NETWATCH-SETUP</code> · then open this UI
        </span>
      </footer>
    </div>
  )
}

function flag(ok: boolean): string {
  return ok ? '[ OK ]' : '[FAIL]'
}

function ms(v: number): string {
  if (v < 0) return '  — ms'
  return `${pad(v, 3)} ms`
}
