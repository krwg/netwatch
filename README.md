# NETWATCH

Always-on **link sentry** for a bare **classic ESP32** board (USB power, **no extra modules**).

The board lives on your desk for days. It watches:

- your **router / gateway**
- the **internet** (public DNS)
- **your PC** — Ethernet IP and/or Wi‑Fi IP (whichever is online)

…keeps a small event log, and exposes a local HTTP API. A React terminal UI on the PC shows live status.

Works when the PC is on **Ethernet cable** or **Wi‑Fi** (same LAN as the ESP).

```
ESP32 (Wi-Fi) ──probe──► gateway / 1.1.1.1 / 8.8.8.8
       │            └──► PC Ethernet IP  and/or  PC Wi‑Fi IP
       └── HTTP /api/*  ◄──  React UI on PC (Ethernet or Wi‑Fi)
```

No personal IPs are hard-coded in the repo. Each person enters **their own** Wi‑Fi and PC addresses in the setup portal; values stay only on that ESP (NVS flash).

---

## Why

No sensors, no screens — a useful background service: you see when the network (or the PC link) dies and when it comes back, without guessing.

---

## Supported hardware

Default PlatformIO board: **`esp32dev`** (Espressif ESP32 Dev Module).

### Works out of the box (classic ESP32 / Xtensa dual-core)

| Module / board | Notes |
|----------------|--------|
| **ESP32-WROOM-32** / WROOM-32D / WROOM-32U | Primary target (most “ESP32 DevKit” clones) |
| **ESP32-WROVER** / WROVER-B / WROVER-E | Same chip family; extra PSRAM unused by Netwatch |
| ESP32 DevKit v1 / DOIT / NodeMCU-32S / similar | Usually `esp32dev` + CH340/CP2102 USB-UART |
| ESP32-D0WD / D0WD-V3 modules | Same classic ESP32 silicon |

Requirements for these boards:

- **2.4 GHz Wi‑Fi** only (no 5 GHz)
- USB-UART bridge for flashing (CH340, CP2102, FTDI, …)
- Enough flash for the Arduino image (~1.3 MB app partition is fine on typical 4 MB modules)

### Not the default (needs a different `board = …` in `platformio.ini`)

| Family | Status |
|--------|--------|
| ESP32-S2 / S3 | Same idea possible, but change board + re-test WiFiManager / ping / USB |
| ESP32-C3 / C6 / H2 (RISC-V) | Not configured in this repo |
| ESP8266 | Not supported |
| Boards without Wi‑Fi | Not supported |

If your board is a **classic ESP32 Dev Module / WROOM**, use the project as-is.  
If it is **S2/S3/C3**, fork and set the matching PlatformIO board — do not expect the stock `esp32dev` build to fit without changes.

---

## What you need

| Item | Notes |
|------|--------|
| Classic ESP32 board (see above) | Prefer ESP32-WROOM DevKit |
| USB cable | Data cable, not charge-only |
| Home Wi‑Fi | **2.4 GHz** |
| PC on the same LAN | Ethernet recommended; Wi‑Fi also fine |
| [PlatformIO Core](https://platformio.org/install/cli) | `pip install platformio` |
| Node.js 18+ | for the React UI |

---

## Repo layout

```
firmware/   PlatformIO firmware (Arduino / esp32dev)
web/        Vite + React terminal dashboard
```

---

## 1. Install tools

### PlatformIO (firmware)

```bash
python -m pip install -U platformio
pio --version
```

On Windows, if `pio` is not found, add this to your **User PATH** and open a **new** terminal:

`%USERPROFILE%\AppData\Roaming\Python\Python312\Scripts`

(Adjust `Python312` if your version differs.)

Or from the repo:

```bash
cd firmware
upload.cmd
```

### Node.js (UI)

```bash
node -v
npm -v
```

---

## 2. Find your PC IP (you will type it in the portal)

### Windows

```powershell
Get-NetIPConfiguration | Where-Object { $_.IPv4Address -and $_.NetAdapter.Status -eq 'Up' }
```

Or: **Settings → Network → Ethernet / Wi‑Fi → Properties → IPv4 address**.

Example:

- Ethernet: `192.168.1.50`
- Wi‑Fi (optional): `192.168.1.51`

Use the address of the interface you actually use day to day. Cable and Wi‑Fi are usually **different** IPs.

### Linux / macOS

```bash
ip addr          # Linux
ifconfig         # macOS
```

---

## 3. Flash the ESP32

1. Plug the ESP32 into USB.
2. Check the serial port:
   - Windows: Device Manager → Ports → e.g. `COM3`
   - Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
   - macOS: `/dev/cu.usbserial-*`
3. Edit `firmware/platformio.ini` if needed:

```ini
upload_port = COM3
monitor_port = COM3
```

4. Build & upload:

```bash
cd firmware
pio run -t upload
```

If upload fails with **Wrong boot mode**:

1. Hold the **BOOT** button on the board.
2. Run `pio run -t upload` again.
3. Release **BOOT** when you see `Connecting...` / writing progress.

Optional serial log:

```bash
pio device monitor
```

Baud: **115200**.

---

## 4. First-time Wi‑Fi + PC setup (portal)

After a fresh flash / credential reset the board opens a soft-AP:

**SSID:** `NETWATCH-SETUP` (open network)

### Steps

1. On phone or PC, join **`NETWATCH-SETUP`**.  
   Status like “No internet” is **normal**.
2. Open a browser: **http://192.168.4.1**
3. Tap **Configure WiFi**.
4. Fill in:

| Field | What to enter |
|--------|----------------|
| Home Wi‑Fi SSID + password | Your 2.4 GHz network |
| **PC Ethernet IP** | Your PC cable IP (e.g. `192.168.1.50`) |
| **PC WiFi IP** | Optional. Only if the PC also uses Wi‑Fi and you want that path watched |

5. Save. The ESP reboots and joins your home Wi‑Fi.
6. Reconnect your phone/PC to the **normal** home network (or keep the PC on Ethernet).

### After setup

- Serial should show something like:  
  `Wi-Fi OK  ip=192.168.x.x ...`  
  `PC eth=...  wifi=...`
- API: `http://<esp-ip>/api/status`  
  or try `http://netwatch.local/api/status` (mDNS; Windows support varies).

Find the ESP IP in:

- the serial monitor, or
- your router’s DHCP client list (hostname `netwatch`).

---

## 5. Run the React UI

```bash
cd web
npm install
npm run dev
```

Open **http://localhost:5173**.

In the footer **api** field enter the board URL, for example:

```text
http://192.168.1.20
```

Press **Apply**. The badge should become **LIVE**.

You should see:

- gateway / internet / extra probes  
- **pc** with `path=ethernet`, `wifi`, `both`, or `none`  
- event log for up/down

### Optional env

```bash
cp .env.example .env
```

- `VITE_API_BASE` — browser talks directly to the ESP  
- `VITE_ESP_PROXY` — used by Vite dev proxy for `/api`

Production build:

```bash
npm run build
npm run preview
```

---

## 6. Daily use

Typical setup:

| Device | Link |
|--------|------|
| ESP32 | Wi‑Fi to home router (powered from USB; can be the PC’s USB) |
| PC | Ethernet to the same router (faster / more stable than room Wi‑Fi) |
| UI | Browser on the PC → ESP API on LAN |

Leave the ESP plugged in. It probes every few seconds and records outages in a ring buffer.

---

## API

| Path | Description |
|------|-------------|
| `GET /api/status` | Live probes, RSSI, uptime, PC path, counters |
| `GET /api/history` | Recent up/down events |

CORS is open so the UI can call the API from another origin (`localhost` → ESP).

Example `pc` block inside `/api/status`:

```json
"pc": {
  "ok": true,
  "path": "ethernet",
  "ethernet": { "ok": true, "ms": 2, "host": "192.168.1.50" },
  "wifi": { "ok": false, "ms": -1, "host": "" }
}
```

`path`: `ethernet` | `wifi` | `both` | `none` | `unset`

---

## How PC detection works

1. ICMP ping to the configured PC IP(s).  
2. If ping is blocked by the OS firewall, TCP fallback to common ports (`445`, `135`, `3389`, `80`).

### Windows tip (optional)

If `pc` stays FAIL while the PC is clearly on, allow ICMPv4 echo in Windows Firewall (Admin PowerShell):

```powershell
netsh advfirewall firewall add rule name="NETWATCH ICMP Echo" protocol=icmpv4:8,any dir=in action=allow
```

Or enable **File and Printer Sharing (Echo Request - ICMPv4-In)** for Private networks.

---

## Reset Wi‑Fi / run setup again

Ways to re-open **`NETWATCH-SETUP`**:

1. **Re-flash** a build that bumps the internal config schema (clears saved Wi‑Fi once), or  
2. From firmware code call `WiFiManager::resetSettings()` and reboot, or  
3. Erase flash / NVS with `esptool` (wipes credentials).

Then repeat **section 4**.

---

## Config knobs

File: `firmware/include/config.h`

| Constant | Meaning |
|----------|---------|
| `kCheckIntervalMs` | Probe interval |
| `kInternetHost` / `kExtraHost` | Public ICMP targets |
| `kSetupApName` | Soft-AP name |
| `kHistoryCap` | Event log size |
| `kDefaultPcEth` / `kDefaultPcWifi` | Defaults (empty in repo — fill in portal) |

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| `pio` not recognized | Fix PATH / new terminal / use `firmware/upload.cmd` |
| Upload: Wrong boot mode | Hold **BOOT**, retry upload |
| Can’t join `NETWATCH-SETUP` | Wait 10–20 s after boot; retry; watch serial |
| Portal won’t open | Go to `http://192.168.4.1` manually |
| ESP not on home Wi‑Fi | 2.4 GHz only; wrong password; redo portal |
| UI **LINK DOWN** | PC and ESP must be on same LAN; set correct ESP IP in UI |
| `pc` always FAIL | Wrong PC IP; firewall; try TCP fallback / enable ICMP |
| `netwatch.local` fails | Use raw IP from serial / router instead |

---

## License

MIT
