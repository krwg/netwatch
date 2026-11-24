# netwatch

[Русский](README.ru.md)

ESP32 firmware + small React UI that monitors LAN/internet reachability and whether your PC answers on Ethernet and/or Wi‑Fi.

Typical layout: ESP32 on 2.4 GHz Wi‑Fi (USB powered), PC on Ethernet to the same router, UI in a browser on the PC talking to the board over LAN.

```
ESP32 (Wi-Fi)
  ├─ ICMP/TCP → gateway
  ├─ ICMP     → 1.1.1.1 / 8.8.8.8
  ├─ probe    → PC eth IP [, PC wifi IP]
  └─ HTTP     ← React UI (localhost)
```

Wi‑Fi credentials and PC addresses are **not** in the repo. You enter them once in the captive portal; they stay in NVS on that device.

## Hardware

`platformio.ini` uses `board = esp32dev`.

**Supported without changes** (classic ESP32, Xtensa dual-core):

| Board / module | Notes |
|----------------|--------|
| ESP32-WROOM-32 / 32D / 32U | Main target |
| ESP32-WROVER / B / E | Same family; PSRAM unused |
| DevKit v1, DOIT, NodeMCU-32S, etc. | CH340 / CP2102 / similar |
| ESP32-D0WD / D0WD-V3 | Same silicon |

Needs 2.4 GHz Wi‑Fi (no 5 GHz), USB-UART for flashing, ~4 MB flash is enough.

**Not configured here:** ESP32-S2/S3, C3/C6/H2, ESP8266. Those need another `board` value and re-testing.

## Requirements

- Classic ESP32 board (above)
- USB data cable
- 2.4 GHz Wi‑Fi
- PC on the same LAN
- [PlatformIO Core](https://platformio.org/install/cli)
- Node.js 18+

```
firmware/   PlatformIO (Arduino)
web/        Vite + React
```

## Install tools

```bash
python -m pip install -U platformio
pio --version
node -v && npm -v
```

Windows: if `pio` is missing, add `%USERPROFILE%\AppData\Roaming\Python\Python3xx\Scripts` to User PATH and open a new terminal. Or use `firmware/upload.cmd`.

## PC IP (for the portal)

You will paste this into the setup form.

Windows:

```powershell
Get-NetIPConfiguration | Where-Object { $_.IPv4Address -and $_.NetAdapter.Status -eq 'Up' }
```

Or Settings → Network → adapter → IPv4 address. Ethernet and Wi‑Fi usually have different addresses.

Linux / macOS: `ip addr` / `ifconfig`.

## Flash

1. Connect USB, note the port (`COM3`, `/dev/ttyUSB0`, …).
2. Set `upload_port` / `monitor_port` in `firmware/platformio.ini`.
3. Upload:

```bash
cd firmware
pio run -t upload
```

If you get `Wrong boot mode`: hold **BOOT**, start upload, release when it connects.

```bash
pio device monitor   # 115200
```

## First setup (soft-AP)

After flash / credential wipe the board opens open AP **`NETWATCH-SETUP`**.

1. Join it (“No internet” is expected).
2. Open http://192.168.4.1 → **Configure WiFi**.
3. Enter home SSID/password, **PC Ethernet IP**, optional **PC WiFi IP**.
4. Save → ESP joins home Wi‑Fi. Put your client back on the normal network / keep PC on Ethernet.

Check serial for `Wi-Fi OK ip=…` and `PC eth=…`. API: `http://<esp-ip>/api/status` (or `http://netwatch.local` if mDNS works on your OS). ESP IP is also in the router DHCP list (hostname `netwatch`).

## Web UI

```bash
cd web
npm install
npm run dev
```

Open http://localhost:5173, set **api** to `http://<esp-ip>`, Apply. Expect gateway/internet probes, `pc.path` (`ethernet` / `wifi` / `both` / `none`), and an event log.

Optional `.env` from `.env.example`: `VITE_API_BASE`, `VITE_ESP_PROXY`.

```bash
npm run build && npm run preview
```

## API

| Method | Path | |
|--------|------|--|
| GET | `/api/status` | probes, RSSI, uptime, PC block |
| GET | `/api/history` | ring buffer of up/down events |

CORS `*` so the UI on another origin can call the board.

```json
"pc": {
  "ok": true,
  "path": "ethernet",
  "ethernet": { "ok": true, "ms": 2, "host": "192.168.1.50" },
  "wifi": { "ok": false, "ms": -1, "host": "" }
}
```

PC check: ICMP first; if blocked, TCP to `445` / `135` / `3389` / `80`.

Windows (admin), if ICMP is denied:

```powershell
netsh advfirewall firewall add rule name="NETWATCH ICMP Echo" protocol=icmpv4:8,any dir=in action=allow
```

## Config

`firmware/include/config.h` — interval, public hosts, AP name, history size. PC IP defaults are empty on purpose.

To reopen the portal: bump schema / `WiFiManager::resetSettings()` / erase NVS, then flash or reboot and repeat soft-AP setup.

## Troubleshooting

| Symptom | Check |
|---------|--------|
| `pio` not found | PATH / `upload.cmd` |
| Wrong boot mode | Hold BOOT |
| No `NETWATCH-SETUP` | Wait after boot; serial log |
| Portal blank | http://192.168.4.1 |
| No home Wi‑Fi | 2.4 GHz only; password; redo portal |
| UI LINK DOWN | Same LAN; correct ESP IP |
| `pc` FAIL | Wrong IP; firewall; TCP fallback |
| `netwatch.local` dead | Use numeric IP |

## License

MIT
