#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <time.h>
#include "web_ui.h"

// First hardware target: classic ESP32 WROOM / DevKit, 4 MB flash.
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;
DynamicJsonDocument config(16384);
String apPassword, events[80], lastError;
unsigned eventCount = 0;
uint32_t lastOutput = 0, overrideEnd = 0, lastReconnect = 0, lastUpdateCheck = 0;
String mode = "schedule";
int overrideLevels[8] = {}, sent[8] = {};
float temperature = NAN;
bool lampOnline = false, forceOutput = true, updateAvailable = false;
String latestVersion, updateUrl;
const char* codes[] = {"w", "b", "r", "g", "b2", "p", "uv", "wm"};
const char* defaultApPassword = "ReefBridge2026!";
const char* updateManifestUrl = "https://xsiimonox.github.io/aipai-reef-studio/bridge-update.json";

bool expired(uint32_t deadline) { return int32_t(millis() - deadline) >= 0; }
void logEvent(String text) {
  time_t now = time(nullptr);
  events[eventCount++ % 80] = String((long)now) + " | " + text;
  Serial.println(text);
}
int channels() { return config["channels"] | 6; }
int compareVersion(const String& a, const String& b) {
  int ai = 0, bi = 0;
  for (int part = 0; part < 3; part++) {
    int ae = a.indexOf('.', ai), be = b.indexOf('.', bi);
    int an = a.substring(ai, ae < 0 ? a.length() : ae).toInt();
    int bn = b.substring(bi, be < 0 ? b.length() : be).toInt();
    if (an != bn) return an < bn ? -1 : 1;
    ai = ae < 0 ? a.length() : ae + 1;
    bi = be < 0 ? b.length() : be + 1;
  }
  return 0;
}
void checkForUpdate() {
  lastUpdateCheck = millis();
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setConnectTimeout(3000); http.setTimeout(4000);
  if (!http.begin(client, updateManifestUrl)) return;
  int status = http.GET();
  String body = status == 200 ? http.getString() : "";
  http.end();
  if (status != 200) { logEvent("Update check unavailable (HTTP " + String(status) + ")"); return; }
  DynamicJsonDocument update(1024);
  if (deserializeJson(update, body)) { logEvent("Update manifest is invalid"); return; }
  String version = update["version"] | "";
  if (version.isEmpty()) { logEvent("Update manifest has no version"); return; }
  latestVersion = version; updateUrl = update["installer_url"] | "";
  bool wasAvailable = updateAvailable;
  updateAvailable = compareVersion(BRIDGE_VERSION, latestVersion) < 0;
  if (updateAvailable && !wasAvailable) logEvent("Firmware update available: v" + latestVersion);
}
void saveConfig() {
  String json; serializeJson(config, json); prefs.putString("config", json);
}
void applyClock() {
  String zone = config["timezone"] | "CET-1CEST,M3.5.0,M10.5.0/3";
  configTzTime(zone.c_str(), "pool.ntp.org", "time.cloudflare.com");
}
void fail(String text, int status = 400) {
  DynamicJsonDocument doc(512); doc["error"] = text;
  String json; serializeJson(doc, json); server.send(status, "application/json", json);
}
bool readBody(JsonDocument& doc) {
  if (server.arg("plain").length() > 14000 || deserializeJson(doc, server.arg("plain"))) {
    fail("Invalid or oversized JSON"); return false;
  }
  return true;
}
bool validLevels(JsonArrayConst values, int count) {
  if (values.size() != size_t(count)) return false;
  for (JsonVariantConst value : values)
    if (!value.is<int>() || value.as<int>() < 0 || value.as<int>() > 100) return false;
  return true;
}
bool validConfig(JsonDocument& doc) {
  int count = doc["channels"] | 0;
  IPAddress ip;
  if ((count != 6 && count != 8) || !ip.fromString(doc["lamp_ip"].as<String>()) ||
      doc["serial"].as<String>().length() == 0 || doc["serial"].as<String>().length() > 32 ||
      doc["timezone"].as<String>().length() > 96) return false;
  JsonArray points = doc["points"].as<JsonArray>();
  if (points.size() < 2 || points.size() > 48) return false;
  int previous = -1;
  for (JsonObject p : points) {
    if (!p["minute"].is<int>()) return false;
    int minute = p["minute"];
    if (minute <= previous || minute >= 1440 || !validLevels(p["levels"], count)) return false;
    previous = minute;
  }
  JsonArray scenes = doc["scenes"].as<JsonArray>();
  if (scenes.size() > 16) return false;
  for (JsonObject scene : scenes)
    if (scene["name"].as<String>().isEmpty() || !validLevels(scene["levels"], count)) return false;
  return true;
}
bool fetch(String query, String& body) {
  HTTPClient http;
  http.setConnectTimeout(1200); http.setTimeout(1500);
  String url = "http://" + config["lamp_ip"].as<String>() + "/?" + query;
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code == 200) body = http.getString();
  http.end();
  if (code != 200) { lastError = "HTTP error " + String(code); return false; }
  return true;
}
bool lampState(String* fields, int& n) {
  String raw;
  if (!fetch("read=config", raw)) return false;
  int begin = raw.indexOf("on|");
  int off = raw.indexOf("off|");
  if (off >= 0 && (begin < 0 || off < begin)) begin = off;
  if (begin < 0 || raw.length() > 8192) { lastError = "Invalid lamp response"; return false; }
  raw = raw.substring(begin);
  int end = raw.indexOf('"'); if (end >= 0) raw = raw.substring(0, end);
  end = raw.indexOf('\''); if (end >= 0) raw = raw.substring(0, end);
  n = 0; int start = 0;
  do {
    end = raw.indexOf('|', start);
    if (n >= 32) { lastError = "Too many protocol fields"; return false; }
    fields[n] = end < 0 ? raw.substring(start) : raw.substring(start, end);
    fields[n++].trim(); start = end + 1;
  } while (end >= 0);
  int roads = n > 28 ? 8 : 6;
  if (n < 25 || fields[21 + 2 * (roads - 6)] != config["serial"].as<String>() || channels() > roads) {
    lastError = "Serial or channel profile mismatch; no writes sent"; return false;
  }
  if ((fields[0] != "on" && fields[0] != "off") || (fields[1] != "0" && fields[1] != "1")) {
    lastError = "Unknown lamp mode; no writes sent"; return false;
  }
  // A verified serial alone is not sufficient to safely rebuild saveconfig.
  for (int i = 2; i < 5 + 2 * roads; ++i) {
    if (fields[i].isEmpty()) { lastError = "Empty lamp configuration field"; return false; }
    for (size_t j = 0; j < fields[i].length(); ++j) {
      char c = fields[i][j];
      if ((c < '0' || c > '9') && !(i >= 5 + roads && c == ',')) {
        lastError = "Unexpected lamp configuration field"; return false;
      }
    }
  }
  String temp = fields[17 + 2 * (roads - 6)];
  char* tail = nullptr; temperature = strtof(temp.c_str(), &tail);
  if (tail == temp.c_str() || *tail != 0 || !isfinite(temperature)) temperature = NAN;
  return true;
}
bool desired(int* target) {
  if (mode != "schedule") {
    if (overrideEnd && expired(overrideEnd)) { mode = "schedule"; overrideEnd = 0; logEvent("Timer/HA lease expired: schedule resumed"); }
    else { for (int i = 0; i < channels(); i++) target[i] = overrideLevels[i]; return true; }
  }
  if (time(nullptr) < 1700000000) { lastError = "Waiting for valid NTP time; no schedule writes"; return false; }
  time_t now = time(nullptr); tm local; localtime_r(&now, &local);
  int minute = local.tm_hour * 60 + local.tm_min;
  JsonArray points = config["points"].as<JsonArray>();
  if (points.size() < 2) return false;
  int right = 0;
  for (size_t i = 0; i < points.size(); i++) if (points[i]["minute"].as<int>() > minute) { right = i; break; }
  JsonObject a = points[(right + points.size() - 1) % points.size()], b = points[right];
  int span = (b["minute"].as<int>() - a["minute"].as<int>() + 1440) % 1440;
  if (!span) return false;
  float fraction = float((minute - a["minute"].as<int>() + 1440) % 1440) / span;
  for (int i = 0; i < channels(); i++) target[i] = lroundf(a["levels"][i].as<int>() +
      (b["levels"][i].as<int>() - a["levels"][i].as<int>()) * fraction);
  return true;
}
void output() {
  if (WiFi.status() != WL_CONNECTED || config["serial"].isNull()) return;
  int target[8] = {};
  if (!desired(target)) return;
  String fields[32]; int n;
  if (!lampState(fields, n)) { lampOnline = false; logEvent(lastError); return; }
  lampOnline = true; int roads = n > 28 ? 8 : 6;
  logEvent("Output " + mode + "; observed power=" + fields[0] + ", mode=" + fields[1]);
  String reply;
  // Preserve firmware schedule, fan thresholds, open/close hours and timezone.
  if (fields[1] != "0" || fields[0] != "on") {
    String query = "save=onx0";
    for (int i = 2; i < 5; i++) query += "x" + fields[i];
    for (int i = 0; i < roads; i++) query += "x" + (i < channels() ? String(target[i]) : fields[5+i]);
    for (int i = 0; i < roads; i++) { String row = fields[5+roads+i]; row.replace(',', 'y'); query += "x" + row; }
    query += "x" + fields[19+2*(roads-6)] + "x" + fields[20+2*(roads-6)] + "x" + fields[23+2*(roads-6)];
    if (!fetch(query, reply)) { logEvent(lastError); return; }
  } else {
    for (int i = 0; i < channels(); i++) {
      if (!fetch(String(codes[i]) + "=" + String(lroundf(target[i] * 1023.0f / 100)), reply)) { logEvent(lastError); return; }
      delay(1);
    }
  }
  for (int i = 0; i < channels(); i++) sent[i] = target[i];
  lastError = "";
}
void setup() {
  Serial.begin(115200); prefs.begin("reefbridge", false);
  apPassword = prefs.getString("ap_pass", "");
  if (apPassword.isEmpty()) { apPassword = defaultApPassword; prefs.putString("ap_pass", apPassword); }
  deserializeJson(config, prefs.getString("config", "{}"));
  WiFi.mode(WIFI_AP_STA);
  String ssid = "ReefBridge-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  WiFi.softAP(ssid.c_str(), apPassword.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());
  WiFi.begin(prefs.getString("ssid", "").c_str(), prefs.getString("wifi_pass", "").c_str());
  Serial.println("Setup SSID: " + ssid + "\nSetup password: " + apPassword);
  Serial.println("Setup: captive portal / http://192.168.4.1  |  Serial commands: INFO");
  applyClock();
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", (const char*)WEB_UI, sizeof(WEB_UI)); });
  server.on("/generate_204", HTTP_GET, [] { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", (const char*)WEB_UI, sizeof(WEB_UI)); });
  server.on("/connecttest.txt", HTTP_GET, [] { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.on("/api/status", HTTP_GET, [] {
    DynamicJsonDocument doc(16384);
    doc["version"] = BRIDGE_VERSION; doc["mode"] = mode; doc["lamp_online"] = lampOnline;
    doc["clock_valid"] = time(nullptr) >= 1700000000; doc["ip"] = WiFi.localIP().toString();
    doc["remaining_seconds"] = overrideEnd && !expired(overrideEnd) ? (overrideEnd - millis() + 999) / 1000 : 0;
    if (isfinite(temperature)) doc["temperature"] = temperature;
    doc["error"] = lastError; doc["config"].set(config.as<JsonObject>());
    JsonObject update = doc.createNestedObject("update");
    update["available"] = updateAvailable; update["latest"] = latestVersion;
    update["installer_url"] = updateUrl;
    JsonArray values = doc.createNestedArray("sent"); for (int i=0;i<channels();i++) values.add(sent[i]);
    JsonArray log = doc.createNestedArray("log"); unsigned start = eventCount > 80 ? eventCount-80 : 0;
    for (unsigned i=start;i<eventCount;i++) log.add(events[i%80]);
    String json; serializeJson(doc, json); server.send(200, "application/json", json);
  });
  server.on("/api/wifi", HTTP_POST, [] {
    DynamicJsonDocument doc(1024); if (!readBody(doc)) return;
    String ssid = doc["ssid"] | "", pass = doc["password"] | "";
    if (ssid.isEmpty() || ssid.length()>32 || pass.length()>63) { fail("Invalid WiFi credentials"); return; }
    prefs.putString("ssid", ssid); prefs.putString("wifi_pass", pass);
    server.send(200, "application/json", "{\"ok\":true}"); WiFi.begin(ssid.c_str(), pass.c_str());
  });
  server.on("/api/config", HTTP_POST, [] {
    DynamicJsonDocument doc(16384); if (!readBody(doc)) return;
    if (!validConfig(doc)) { fail("Invalid configuration: sorted points, IP, serial and channel levels required"); return; }
    config.set(doc); saveConfig(); applyClock(); forceOutput = true; logEvent("Configuration saved");
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/control", HTTP_POST, [] {
    DynamicJsonDocument doc(2048); if (!readBody(doc)) return;
    if (config["serial"].isNull()) { fail("Configure a lamp first",409); return; }
    String action = doc["action"] | "";
    if (action == "resume") { mode="schedule"; overrideEnd=0; }
    else {
      if (!validLevels(doc["levels"], channels())) { fail("Invalid channel levels"); return; }
      if (action != "preview" && action != "manual" && action != "lease") { fail("Unknown action"); return; }
      if (mode == "manual" && action == "lease") { fail("Manual mode owns the output",409); return; }
      int seconds = doc["seconds"] | (action == "lease" ? 90 : 60);
      if (seconds < 1 || seconds > (action == "lease" ? 90 : 3600)) { fail("Invalid duration"); return; }
      for (int i=0;i<channels();i++) overrideLevels[i]=doc["levels"][i];
      mode=action; overrideEnd=action == "manual" ? 0 : millis()+uint32_t(seconds)*1000;
    }
    forceOutput=true; logEvent("Control: " + mode); server.send(200,"application/json","{\"ok\":true}");
  });
  server.onNotFound([] {
    // The AP DNS points all captive-portal probes here. Redirect them to the
    // local dashboard so iOS, Android, macOS and Windows can open it directly.
    if (WiFi.softAPgetStationNum()) {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
      return;
    }
    fail("Not found",404);
  }); server.begin();
  logEvent("Bridge booted: local schedule; temporary overrides cleared");
}
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  if (Serial.available()) { String command=Serial.readStringUntil('\n'); command.trim();
    if(command=="INFO") Serial.println("AP password: "+apPassword+"\nIP: "+WiFi.localIP().toString());
  }
  if (WiFi.status()==WL_CONNECTED) {
    static bool mdns=false; if(!mdns) mdns=MDNS.begin("reefbridge");
    // Setup AP shuts down after the first successful station connection.
    static bool apStopped=false; if(!apStopped) { dnsServer.stop(); WiFi.softAPdisconnect(true); apStopped=true; }
    if (!lastUpdateCheck || millis() - lastUpdateCheck >= 6UL * 60UL * 60UL * 1000UL) checkForUpdate();
  } else if(millis()-lastReconnect>30000) { lastReconnect=millis(); WiFi.reconnect(); }
  if (forceOutput || millis()-lastOutput>=30000 || (overrideEnd && expired(overrideEnd))) {
    forceOutput=false; lastOutput=millis(); output();
  }
  delay(2);
}
