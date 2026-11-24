#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"

struct Probe {
  bool ok = false;
  int ms = -1;
  char host[40] = {};
};

struct Event {
  uint32_t uptime_s = 0;
  uint32_t epoch = 0;
  char kind[8] = {};
  char target[16] = {};
  char detail[48] = {};
};

static WebServer server(kHttpPort);
static Preferences prefs;

static Probe gatewayProbe;
static Probe internetProbe;
static Probe extraProbe;
static Probe pcEthProbe;
static Probe pcWifiProbe;

static char pcEthHost[40] = {};
static char pcWifiHost[40] = {};

static bool lastInternetOk = true;
static bool lastPcOk = true;
static bool bootstrapped = false;
static uint32_t checksOk = 0;
static uint32_t checksFail = 0;
static uint32_t lastCheckMs = 0;
static uint32_t lastChangeUptimeS = 0;

static Event history[kHistoryCap];
static size_t historyHead = 0;
static size_t historyCount = 0;

static const char *kPortalCss = R"CSS(
<style>
  :root { color-scheme: dark; }
  html, body {
    margin: 0; min-height: 100%;
    background: #0a0a0a !important;
    color: #f2f2f2 !important;
    font-family: "Segoe UI", system-ui, -apple-system, sans-serif !important;
    -webkit-font-smoothing: antialiased;
  }
  body.home, body { background: #0a0a0a !important; }
  .wrap {
    min-width: min(92vw, 420px) !important;
    max-width: 420px !important;
    margin: 8vh auto 48px !important;
    padding: 28px 22px 22px !important;
    background: #111 !important;
    border: 1px solid #2a2a2a !important;
    border-radius: 16px !important;
    box-shadow: 0 24px 80px rgba(0,0,0,.55) !important;
    text-align: left !important;
  }
  h1 {
    margin: 0 0 4px !important;
    font-size: 1.55rem !important;
    font-weight: 600 !important;
    letter-spacing: .12em !important;
    text-transform: uppercase !important;
    text-align: left !important;
    color: #fff !important;
  }
  h3 {
    margin: 0 0 22px !important;
    font-size: .78rem !important;
    font-weight: 500 !important;
    letter-spacing: .08em !important;
    text-transform: uppercase !important;
    color: #888 !important;
    text-align: left !important;
  }
  a { color: #fff !important; font-weight: 600 !important; text-decoration: none !important; }
  a:hover { color: #fff !important; text-decoration: underline !important; }
  input, select {
    width: 100% !important;
    background: #0a0a0a !important;
    color: #f2f2f2 !important;
    border: 1px solid #333 !important;
    border-radius: 10px !important;
    padding: 12px 14px !important;
    margin: 6px 0 12px !important;
    font-size: 1rem !important;
    outline: none !important;
  }
  input:focus, select:focus { border-color: #fff !important; }
  button, input[type=submit], input[type=button] {
    width: 100% !important;
    margin: 8px 0 !important;
    padding: 0 !important;
    height: 46px !important;
    line-height: 46px !important;
    border: 0 !important;
    border-radius: 999px !important;
    background: #f2f2f2 !important;
    color: #0a0a0a !important;
    font-size: .92rem !important;
    font-weight: 600 !important;
    letter-spacing: .04em !important;
  }
  button:hover, input[type=submit]:hover { filter: brightness(1.05); }
  button.D, .D { background: #2a2a2a !important; color: #f2f2f2 !important; }
  hr { border: 0 !important; border-top: 1px solid #2a2a2a !important; margin: 18px 0 !important; }
  .msg {
    background: #161616 !important;
    border: 1px solid #2a2a2a !important;
    border-left: 3px solid #f2f2f2 !important;
    border-radius: 10px !important;
    color: #ddd !important;
    padding: 14px 16px !important;
  }
  .msg h4 { color: #fff !important; }
  .q:after, .q:before { filter: grayscale(1) invert(1); opacity: .85; }
  .footer { color: #666 !important; }
  label { color: #aaa !important; font-size: .85rem !important; }
</style>
)CSS";

static bool hostConfigured(const char *host) {
  return host && host[0] != '\0';
}

static void setHost(char *dst, size_t n, const char *src) {
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = '\0';
}

static void loadPcHosts() {
  prefs.begin("netwatch", true);
  String eth = prefs.getString("pc_eth", kDefaultPcEth);
  String wifi = prefs.getString("pc_wifi", kDefaultPcWifi);
  prefs.end();
  setHost(pcEthHost, sizeof(pcEthHost), eth.c_str());
  setHost(pcWifiHost, sizeof(pcWifiHost), wifi.c_str());
}

static void savePcHosts(const char *eth, const char *wifi) {
  prefs.begin("netwatch", false);
  prefs.putString("pc_eth", eth ? eth : "");
  prefs.putString("pc_wifi", wifi ? wifi : "");
  prefs.end();
  setHost(pcEthHost, sizeof(pcEthHost), eth);
  setHost(pcWifiHost, sizeof(pcWifiHost), wifi);
}

static void addCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJson(int code, const String &body) {
  addCors();
  server.send(code, "application/json", body);
}

static void pushEvent(const char *kind, const char *target, const char *detail) {
  Event &e = history[historyHead];
  e.uptime_s = millis() / 1000UL;
  e.epoch = (uint32_t)time(nullptr);
  strncpy(e.kind, kind, sizeof(e.kind) - 1);
  strncpy(e.target, target, sizeof(e.target) - 1);
  strncpy(e.detail, detail, sizeof(e.detail) - 1);
  historyHead = (historyHead + 1) % kHistoryCap;
  if (historyCount < kHistoryCap) historyCount++;
  lastChangeUptimeS = e.uptime_s;
}

static bool tcpProbe(const char *host, uint16_t port, uint32_t timeoutMs) {
  WiFiClient client;
  client.setTimeout(timeoutMs / 1000.0f);
  const uint32_t started = millis();
  if (!client.connect(host, port)) return false;
  client.stop();
  return (millis() - started) < timeoutMs + 50;
}

static Probe probeHost(const char *host) {
  Probe p;
  if (!hostConfigured(host)) return p;
  setHost(p.host, sizeof(p.host), host);

  const bool icmp = Ping.ping(host, 2);
  if (icmp) {
    p.ok = true;
    p.ms = (int)Ping.averageTime();
    return p;
  }

  const uint32_t t0 = millis();
  for (size_t i = 0; i < kPcTcpFallbackCount; i++) {
    if (tcpProbe(host, kPcTcpFallbackPorts[i], 400)) {
      p.ok = true;
      p.ms = (int)(millis() - t0);
      return p;
    }
  }
  p.ok = false;
  p.ms = -1;
  return p;
}

static Probe pingIp(const IPAddress &ip) {
  Probe p;
  if (ip == IPAddress((uint32_t)0)) return p;
  const String s = ip.toString();
  setHost(p.host, sizeof(p.host), s.c_str());
  const bool ok = Ping.ping(ip, 3);
  p.ok = ok;
  p.ms = ok ? (int)Ping.averageTime() : -1;
  return p;
}

static Probe pingName(const char *host) {
  Probe p;
  setHost(p.host, sizeof(p.host), host);
  const bool ok = Ping.ping(host, 3);
  p.ok = ok;
  p.ms = ok ? (int)Ping.averageTime() : -1;
  return p;
}

static const char *pcPath() {
  const bool eth = pcEthProbe.ok;
  const bool wifi = pcWifiProbe.ok;
  if (eth && wifi) return "both";
  if (eth) return "ethernet";
  if (wifi) return "wifi";
  if (!hostConfigured(pcEthHost) && !hostConfigured(pcWifiHost)) return "unset";
  return "none";
}

static bool pcReachable() {
  return pcEthProbe.ok || pcWifiProbe.ok;
}

static void runChecks() {
  IPAddress gw = WiFi.gatewayIP();
  gatewayProbe = pingIp(gw);
  internetProbe = pingName(kInternetHost);
  extraProbe = pingName(kExtraHost);
  pcEthProbe = probeHost(pcEthHost);
  pcWifiProbe = probeHost(pcWifiHost);

  const bool internetOk = internetProbe.ok || extraProbe.ok;
  const bool pcOk = pcReachable();
  if (internetOk) checksOk++;
  else checksFail++;

  if (!bootstrapped) {
    bootstrapped = true;
    lastInternetOk = internetOk;
    lastPcOk = pcOk;
    pushEvent(internetOk ? "up" : "down", "internet",
              internetOk ? "monitoring started (up)" : "monitoring started (down)");
    pushEvent(pcOk ? "up" : "down", "pc",
              pcOk ? pcPath() : "pc unreachable");
    return;
  }

  if (internetOk != lastInternetOk) {
    char detail[48];
    if (internetOk) {
      snprintf(detail, sizeof(detail), "restored gw=%dms net=%dms",
               gatewayProbe.ms, internetProbe.ms);
      pushEvent("up", "internet", detail);
    } else {
      snprintf(detail, sizeof(detail), "lost gw=%s net=%s",
               gatewayProbe.ok ? "ok" : "fail",
               internetProbe.ok ? "ok" : "fail");
      pushEvent("down", "internet", detail);
    }
    lastInternetOk = internetOk;
  }

  if (pcOk != lastPcOk) {
    char detail[48];
    if (pcOk) {
      snprintf(detail, sizeof(detail), "path=%s eth=%dms wifi=%dms",
               pcPath(), pcEthProbe.ms, pcWifiProbe.ms);
      pushEvent("up", "pc", detail);
    } else {
      snprintf(detail, sizeof(detail), "lost eth=%s wifi=%s",
               pcEthProbe.ok ? "ok" : "fail",
               pcWifiProbe.ok ? "ok" : "fail");
      pushEvent("down", "pc", detail);
    }
    lastPcOk = pcOk;
  }
}

static void fillProbe(JsonObject o, const Probe &p) {
  o["ok"] = p.ok;
  o["ms"] = p.ms;
  o["host"] = p.host;
}

static String statusJson() {
  JsonDocument doc;
  doc["name"] = "netwatch";
  doc["version"] = "0.2.0";
  doc["uptime_s"] = millis() / 1000UL;
  doc["wifi_ssid"] = WiFi.SSID();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["gateway_ip"] = WiFi.gatewayIP().toString();
  doc["mdns"] = String(kHostname) + ".local";
  doc["time_epoch"] = (uint32_t)time(nullptr);
  doc["last_change_uptime_s"] = lastChangeUptimeS;
  doc["checks_ok"] = checksOk;
  doc["checks_fail"] = checksFail;
  doc["interval_ms"] = kCheckIntervalMs;

  fillProbe(doc["gateway"].to<JsonObject>(), gatewayProbe);

  JsonObject net = doc["internet"].to<JsonObject>();
  net["ok"] = internetProbe.ok;
  net["ms"] = internetProbe.ms;
  net["host"] = kInternetHost;

  JsonObject extra = doc["extra"].to<JsonObject>();
  extra["ok"] = extraProbe.ok;
  extra["ms"] = extraProbe.ms;
  extra["host"] = kExtraHost;

  JsonObject pc = doc["pc"].to<JsonObject>();
  pc["ok"] = pcReachable();
  pc["path"] = pcPath();

  JsonObject eth = pc["ethernet"].to<JsonObject>();
  eth["ok"] = pcEthProbe.ok;
  eth["ms"] = pcEthProbe.ms;
  eth["host"] = pcEthHost;

  JsonObject wifi = pc["wifi"].to<JsonObject>();
  wifi["ok"] = pcWifiProbe.ok;
  wifi["ms"] = pcWifiProbe.ms;
  wifi["host"] = pcWifiHost;

  doc["online"] = lastInternetOk;

  String out;
  serializeJson(doc, out);
  return out;
}

static String historyJson() {
  JsonDocument doc;
  JsonArray arr = doc["events"].to<JsonArray>();
  for (size_t i = 0; i < historyCount; i++) {
    const size_t idx = (historyHead + kHistoryCap - historyCount + i) % kHistoryCap;
    const Event &e = history[idx];
    JsonObject o = arr.add<JsonObject>();
    o["uptime_s"] = e.uptime_s;
    o["epoch"] = e.epoch;
    o["kind"] = e.kind;
    o["target"] = e.target;
    o["detail"] = e.detail;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

static void handleOptions() {
  addCors();
  server.send(204);
}

static void setupHttp() {
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/history", HTTP_OPTIONS, handleOptions);
  server.on("/api/status", HTTP_GET, []() { sendJson(200, statusJson()); });
  server.on("/api/history", HTTP_GET, []() { sendJson(200, historyJson()); });
  server.on("/", HTTP_GET, []() {
    addCors();
    server.send(200, "text/plain",
                "netwatch ok\n"
                "API: /api/status  /api/history\n");
  });
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      handleOptions();
      return;
    }
    sendJson(404, "{\"error\":\"not_found\"}");
  });
  server.begin();
}

static void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

static void connectWifiAndMaybeReset() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setHostname(kHostname);
  wm.setTitle("NETWATCH");
  wm.setCustomHeadElement(kPortalCss);

  prefs.begin("netwatch", false);
  const uint32_t schema = prefs.getUInt("schema", 0);
  const bool forceReset = schema < kConfigSchema;
  if (forceReset) {
    wm.resetSettings();
    prefs.putUInt("schema", kConfigSchema);
    Serial.println("Wi-Fi credentials cleared — open NETWATCH-SETUP");
  }
  prefs.end();

  WiFiManagerParameter pcEth(
      "pc_eth",
      "PC Ethernet IP (your LAN cable address)",
      hostConfigured(pcEthHost) ? pcEthHost : kDefaultPcEth,
      39);
  WiFiManagerParameter pcWifi(
      "pc_wifi",
      "PC WiFi IP (optional, if PC also uses Wi-Fi)",
      hostConfigured(pcWifiHost) ? pcWifiHost : kDefaultPcWifi,
      39);
  wm.addParameter(&pcEth);
  wm.addParameter(&pcWifi);

  if (!wm.autoConnect(kSetupApName)) {
    Serial.println("Wi-Fi setup failed / timeout — restarting");
    delay(1500);
    ESP.restart();
  }

  savePcHosts(pcEth.getValue(), pcWifi.getValue());
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== NETWATCH ===");

  loadPcHosts();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  connectWifiAndMaybeReset();

  Serial.printf("Wi-Fi OK  ip=%s  gw=%s  rssi=%d\n",
                WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str(),
                WiFi.RSSI());
  Serial.printf("PC eth=%s  wifi=%s\n",
                hostConfigured(pcEthHost) ? pcEthHost : "-",
                hostConfigured(pcWifiHost) ? pcWifiHost : "-");

  if (MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", kHttpPort);
    Serial.printf("mDNS: http://%s.local\n", kHostname);
  }

  setupTime();
  setupHttp();
  runChecks();
  lastCheckMs = millis();
  Serial.println("HTTP API listening");
}

void loop() {
  server.handleClient();

  if (millis() - lastCheckMs >= kCheckIntervalMs) {
    lastCheckMs = millis();
    if (WiFi.status() == WL_CONNECTED) {
      runChecks();
      Serial.printf("[check] net=%d pc=%s eth=%d/%d wifi=%d/%d\n",
                    lastInternetOk ? 1 : 0,
                    pcPath(),
                    pcEthProbe.ok, pcEthProbe.ms,
                    pcWifiProbe.ok, pcWifiProbe.ms);
    } else {
      if (lastInternetOk || !bootstrapped) {
        pushEvent("down", "wifi", "station disconnected");
        lastInternetOk = false;
        bootstrapped = true;
      }
      if (lastPcOk) {
        pushEvent("down", "pc", "wifi station down");
        lastPcOk = false;
      }
      checksFail++;
      Serial.println("[check] wifi disconnected");
    }
  }
}
