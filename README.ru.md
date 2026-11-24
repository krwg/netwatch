# netwatch

[English](README.md)

Прошивка для ESP32 и небольшой React‑UI: проверка доступности шлюза/интернета и ответа ПК по Ethernet и/или Wi‑Fi.

Обычная схема: ESP32 в 2.4 GHz Wi‑Fi (питание по USB), ПК по кабелю в тот же роутер, UI в браузере на ПК ходит к плате по LAN.

```
ESP32 (Wi-Fi)
  ├─ ICMP/TCP → gateway
  ├─ ICMP     → 1.1.1.1 / 8.8.8.8
  ├─ probe    → IP ПК eth [, IP ПК wifi]
  └─ HTTP     ← React UI (localhost)
```

Логины Wi‑Fi и адреса ПК **не** зашиты в репозиторий. Вводятся один раз в captive portal и лежат в NVS на конкретной плате.

## Железо

В `platformio.ini`: `board = esp32dev`.

**Без правок** (classic ESP32, Xtensa dual-core):

| Плата / модуль | Заметки |
|----------------|---------|
| ESP32-WROOM-32 / 32D / 32U | Основной таргет |
| ESP32-WROVER / B / E | То же семейство; PSRAM не используется |
| DevKit v1, DOIT, NodeMCU-32S и клоны | CH340 / CP2102 / аналоги |
| ESP32-D0WD / D0WD-V3 | Тот же кристалл |

Нужен Wi‑Fi **2.4 GHz** (5 GHz нет), USB‑UART для прошивки, хватает типичных 4 MB flash.

**Не настроено:** ESP32-S2/S3, C3/C6/H2, ESP8266 — другой `board` и отдельная проверка.

## Что нужно

- classic ESP32 (таблица выше)
- USB‑кабель с данными
- домашняя сеть 2.4 GHz
- ПК в той же LAN
- [PlatformIO Core](https://platformio.org/install/cli)
- Node.js 18+

```
firmware/   PlatformIO (Arduino)
web/        Vite + React
```

## Инструменты

```bash
python -m pip install -U platformio
pio --version
node -v && npm -v
```

Windows: если нет `pio`, добавь `%USERPROFILE%\AppData\Roaming\Python\Python3xx\Scripts` в User PATH и открой новый терминал. Либо `firmware/upload.cmd`.

## IP компьютера (для портала)

Его потом впишешь в форму настройки.

Windows:

```powershell
Get-NetIPConfiguration | Where-Object { $_.IPv4Address -and $_.NetAdapter.Status -eq 'Up' }
```

Или Параметры → Сеть → адаптер → IPv4. У Ethernet и Wi‑Fi адреса обычно разные.

Linux / macOS: `ip addr` / `ifconfig`.

## Прошивка

1. Подключи USB, посмотри порт (`COM3`, `/dev/ttyUSB0`, …).
2. Пропиши `upload_port` / `monitor_port` в `firmware/platformio.ini`.
3. Залей:

```bash
cd firmware
pio run -t upload
```

Ошибка `Wrong boot mode`: зажми **BOOT**, запусти upload, отпусти когда пойдёт соединение.

```bash
pio device monitor   # 115200
```

## Первая настройка (soft-AP)

После прошивки / сброса учёток поднимается открытая точка **`NETWATCH-SETUP`**.

1. Подключись к ней («Без интернета» — нормально).
2. Открой http://192.168.4.1 → **Configure WiFi**.
3. Домашний SSID/пароль, **PC Ethernet IP**, опционально **PC WiFi IP**.
4. Save → ESP уходит в домашнюю сеть. Клиент верни в обычный Wi‑Fi / ПК оставь на кабеле.

В Serial: `Wi-Fi OK ip=…`, `PC eth=…`. API: `http://<ip-esp>/api/status` (или `http://netwatch.local`, если mDNS живой). IP платы также в DHCP роутера (hostname `netwatch`).

## Веб‑UI

```bash
cd web
npm install
npm run dev
```

http://localhost:5173 → поле **api** = `http://<ip-esp>` → Apply. Должны быть пробы gateway/internet, `pc.path` (`ethernet` / `wifi` / `both` / `none`) и лог событий.

Опционально `.env` из `.env.example`: `VITE_API_BASE`, `VITE_ESP_PROXY`.

```bash
npm run build && npm run preview
```

## API

| Метод | Путь | |
|-------|------|--|
| GET | `/api/status` | пробы, RSSI, uptime, блок PC |
| GET | `/api/history` | кольцевой буфер up/down |

CORS `*` — UI с другого origin может ходить на плату.

```json
"pc": {
  "ok": true,
  "path": "ethernet",
  "ethernet": { "ok": true, "ms": 2, "host": "192.168.1.50" },
  "wifi": { "ok": false, "ms": -1, "host": "" }
}
```

Проверка ПК: сначала ICMP; если режет фаервол — TCP на `445` / `135` / `3389` / `80`.

Windows (от админа), если ICMP закрыт:

```powershell
netsh advfirewall firewall add rule name="NETWATCH ICMP Echo" protocol=icmpv4:8,any dir=in action=allow
```

## Конфиг

`firmware/include/config.h` — интервал, публичные хосты, имя AP, размер истории. Дефолты IP ПК в репо пустые.

Снова открыть портал: поднять schema / `WiFiManager::resetSettings()` / стереть NVS, перепрошить или перезагрузить и повторить soft-AP.

## Если что-то не так

| Симптом | Что смотреть |
|---------|----------------|
| нет `pio` | PATH / `upload.cmd` |
| Wrong boot mode | BOOT |
| нет `NETWATCH-SETUP` | подождать после boot; Serial |
| портал пустой | http://192.168.4.1 |
| не цепляется к дому | только 2.4 GHz; пароль; портал заново |
| UI LINK DOWN | одна LAN; верный IP ESP |
| `pc` FAIL | неверный IP; фаервол; TCP fallback |
| `netwatch.local` мёртв | бери числовой IP |

## Лицензия

MIT
