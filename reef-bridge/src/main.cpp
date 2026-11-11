#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <time.h>
#include "web_ui.h"

// Shared Studio UI; bounded local persistence and independent lamp runtimes.
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;
DynamicJsonDocument workspace(65536);
const int MAX_LAMPS = 4;
const char* codes[] = {"w","b","r","g","b2","p","uv","wm"};
const char* installerUrl = "https://xsiimonox.github.io/aipai-reef-studio/";
const char* defaultZone = "CET-1CEST,M3.5.0,M10.5.0/3";
struct Lamp {
  String mode="firmware", scene, error;
  int levels[8]={}, sent[8]={}, previewLevels[8]={};
  uint32_t until=0, previewEnd=0, lastOutput=0, lastClockSync=0;
  bool online=false, force=true, releaseRequested=false;
  float temperature=NAN;
  String logs[32]; unsigned logCount=0;
  String deviceClock, deviceTimezone, power, observed;
  int openHour=0, closeHour=0;
} lamps[MAX_LAMPS];
bool fsReady=false, updateAvailable=false;
String latestVersion, updateError, apPassword;
uint32_t lastUpdateCheck=0, lastReconnect=0;
bool expired(uint32_t deadline) { return int32_t(millis()-deadline)>=0; }
int count() { return workspace["lamps"].size(); }
int channels(int i) { return workspace["lamps"][i]["channels"] | 6; }
String newId() { return String((uint32_t)esp_random(),HEX)+String((uint32_t)esp_random(),HEX); }
void logEvent(int i, String text) {
  text="epoch="+String(long(time(nullptr)))+" uptime_ms="+String(millis())+" "+text;
  if(text.length()>240)text=text.substring(0,240);
  Serial.println(text); lamps[i].logs[lamps[i].logCount++%32]=text;
}
void fail(String message,int code=400) {
  DynamicJsonDocument data(512);data["error"]=message;String json;serializeJson(data,json);server.send(code,"application/json",json);
}
void ok() { server.send(200,"application/json","{\"ok\":true}"); }
bool body(JsonDocument& doc) {
  if(server.arg("plain").length()>48000 || deserializeJson(doc,server.arg("plain"))){fail("Ungültige oder zu große Daten.");return false;}return true;
}
bool validLevels(JsonVariantConst levels,int n=0) {
  if(!levels.is<JsonArrayConst>() || (n && levels.size()!=size_t(n)) || !levels.size() || levels.size()>8)return false;
  for(JsonVariantConst v:levels.as<JsonArrayConst>())if(!v.is<int>()||v.as<int>()<0||v.as<int>()>100)return false;
  return true;
}
bool validName(JsonVariantConst value) {return value.is<const char*>() && value.as<String>().length()>0 && value.as<String>().length()<=64;}
bool validItem(JsonVariantConst item,bool scene) {
  if(!item.is<JsonObjectConst>()||!validName(item["name"]))return false;
  if(scene)return validLevels(item["levels"]);
  if(!item["points"].is<JsonArrayConst>()||item["points"].size()<2||item["points"].size()>48)return false;
  int previous=-1,n=0;
  for(JsonObjectConst point:item["points"].as<JsonArrayConst>()) {
    if(!point["minute"].is<int>())return false;
    int minute=point["minute"];if(minute<=previous||minute>=1440||!validLevels(point["levels"],n))return false;
    n=point["levels"].size();previous=minute;
  }return true;
}
bool validFixture(JsonVariantConst lamp) {
  IPAddress ip;int n=lamp["channels"]|0;
  return lamp.is<JsonObjectConst>()&&validName(lamp["name"])&&(n==6||n==8)&&
    lamp["serial"].is<const char*>()&&lamp["serial"].as<String>().length()>0&&lamp["serial"].as<String>().length()<=32&&
    lamp["lamp_ip"].is<const char*>()&&ip.fromString(lamp["lamp_ip"].as<String>())&&
    lamp["timezone"].is<const char*>()&&lamp["timezone"].as<String>().length()>0&&lamp["timezone"].as<String>().length()<=96;
}
bool validWorkspace(JsonDocument& doc) {
  if(!doc["lamps"].is<JsonArray>()||doc["lamps"].size()>MAX_LAMPS||!doc["programs"].is<JsonObject>()||!doc["scenes"].is<JsonObject>()||!doc["groups"].is<JsonObject>())return false;
  for(const char* kind:{"programs","scenes"}){
    if(doc[kind].size()>32)return false;
    for(JsonPair item:doc[kind].as<JsonObject>())if(!validItem(item.value(),String(kind)=="scenes"))return false;
  }
  for(size_t i=0;i<doc["lamps"].size();i++){
    JsonVariant lamp=doc["lamps"][i];if(!validFixture(lamp))return false;
    for(size_t j=0;j<i;j++)if(lamp["serial"].as<String>()==doc["lamps"][j]["serial"].as<String>())return false;
    if(!lamp["program"].isNull()&&!doc["programs"].containsKey(lamp["program"].as<String>()))return false;
    DynamicJsonDocument fallback(8192);fallback["name"]="Fallback";fallback["points"]=lamp["points"];
    if(!validItem(fallback.as<JsonVariant>(),false))return false;
  }
  if(doc["groups"].size()>16)return false;
  for(JsonPair pair:doc["groups"].as<JsonObject>()){
    JsonVariant group=pair.value();if(!validName(group["name"])||!group["serials"].is<JsonArray>()||!group["serials"].size())return false;
    for(size_t i=0;i<group["serials"].size();i++){
      bool found=false;String serial=group["serials"][i];
      for(JsonObject lamp:doc["lamps"].as<JsonArray>())if(lamp["serial"].as<String>()==serial)found=true;
      if(!found)return false;
      for(size_t j=0;j<i;j++)if(group["serials"][j].as<String>()==serial)return false;
    }
  }
  return !doc.overflowed()&&measureJson(doc)<=48000;
}
bool persist(JsonDocument& next) {
  if(!fsReady){fail("Lokaler Speicher nicht verfügbar.",507);return false;}
  if(!validWorkspace(next)){fail("Ungültige Bibliothek oder Speichergrenze erreicht (48 KB).");return false;}
  File file=LittleFS.open("/studio.tmp","w");
  if(!file){fail("Sicherung konnte nicht geschrieben werden.",507);return false;}
  size_t bytes=serializeJson(next,file);file.close();
  if(bytes!=measureJson(next)||!LittleFS.rename("/studio.tmp","/studio.json")){fail("Speichern fehlgeschlagen.",507);return false;}
  workspace.set(next);return true;
}
void defaults() {
  workspace.clear();workspace["schema"]=1;workspace.createNestedArray("lamps");
  workspace.createNestedObject("programs");workspace.createNestedObject("scenes");workspace.createNestedObject("groups");
}
void fallbackPoints(JsonObject lamp) {
  JsonArray points=lamp.createNestedArray("points");
  for(int minute:{0,600,1200}){JsonObject point=points.createNestedObject();point["minute"]=minute;JsonArray levels=point.createNestedArray("levels");for(int j=0;j<8;j++)levels.add(0);}
}
void loadWorkspace() {
  defaults();
  if(fsReady&&LittleFS.exists("/studio.json")) {
    File file=LittleFS.open("/studio.json","r");DynamicJsonDocument data(65536);auto error=deserializeJson(data,file);file.close();
    if(!error&&validWorkspace(data)){workspace.set(data);return;}
    Serial.println("Stored Studio data invalid; file retained. Reimport a backup.");return;
  }
  // First 0.2 boot imports the previous one-lamp configuration. Keep the old
  // Preferences key as a recovery copy, never erase it during migration.
  DynamicJsonDocument legacy(16384);deserializeJson(legacy,prefs.getString("config","{}"));
  if(!legacy["serial"].isNull()) {
    JsonObject lamp=workspace["lamps"].createNestedObject();lamp.set(legacy.as<JsonObject>());lamp["name"]="AIPAI "+legacy["serial"].as<String>();
    JsonObject program=workspace["programs"].createNestedObject("legacy-day");program["name"]="Bisheriges Tagesprogramm";program["points"]=legacy["points"];lamp["program"]="legacy-day";lamp.remove("scenes");
    int i=0;for(JsonObject scene:legacy["scenes"].as<JsonArray>())workspace["scenes"]["legacy-"+String(i++)]=scene;
    if(validWorkspace(workspace)&&fsReady){File file=LittleFS.open("/studio.json","w");serializeJson(workspace,file);file.close();}
  }
}
bool fetch(int i,String query,String& response) {
  HTTPClient http;http.setConnectTimeout(900);http.setTimeout(1200);
  if(!http.begin("http://"+workspace["lamps"][i]["lamp_ip"].as<String>()+"/?"+query))return false;
  int code=http.GET();if(code==200)response=http.getString();http.end();
  if(code!=200){lamps[i].error="Lampen-HTTP "+String(code);return false;}return true;
}
bool lampState(int i,String* fields,int& n) {
  String raw;if(!fetch(i,"read=config",raw))return false;
  int begin=raw.indexOf("on|"),off=raw.indexOf("off|");if(off>=0&&(begin<0||off<begin))begin=off;
  if(begin<0||raw.length()>8192){lamps[i].error="Ungültige Lampenantwort";return false;}
  raw=raw.substring(begin);int end=raw.indexOf('"');if(end>=0)raw=raw.substring(0,end);end=raw.indexOf('\'');if(end>=0)raw=raw.substring(0,end);
  n=0;int start=0;do{end=raw.indexOf('|',start);if(n>=32)return false;fields[n]=end<0?raw.substring(start):raw.substring(start,end);fields[n++].trim();start=end+1;}while(end>=0);
  int roads=n>28?8:6;
  if(n<25||fields[21+2*(roads-6)]!=workspace["lamps"][i]["serial"].as<String>()||channels(i)>roads){lamps[i].error="Seriennummer oder Kanalprofil passt nicht; kein Schreibzugriff";return false;}
  if((fields[0]!="on"&&fields[0]!="off")||(fields[1]!="0"&&fields[1]!="1"))return false;
  Lamp& lamp=lamps[i];lamp.deviceClock=fields[18+2*(roads-6)];lamp.deviceTimezone=fields[23+2*(roads-6)];lamp.power=fields[0];lamp.openHour=fields[19+2*(roads-6)].toInt();lamp.closeHour=fields[20+2*(roads-6)].toInt();
  String observed=fields[0]+"|"+fields[1];for(int k=5;k<5+2*roads;k++)observed+="|"+fields[k];
  if(observed!=lamp.observed){logEvent(i,"Status geändert; Uhr="+lamp.deviceClock+"; UTC="+lamp.deviceTimezone+"; Schaltzeit="+String(lamp.openHour)+"–"+String(lamp.closeHour)+"; Status="+observed);lamp.observed=observed;}
  for(int k=2;k<5+2*roads;k++){
    if(fields[k].isEmpty())return false;
    for(size_t j=0;j<fields[k].length();j++){char c=fields[k][j];if((c<'0'||c>'9')&&!(k>=5+roads&&c==','))return false;}
  }
  char* tail=nullptr;String temp=fields[17+2*(roads-6)];lamps[i].temperature=strtof(temp.c_str(),&tail);
  if(tail==temp.c_str()||*tail!=0||!isfinite(lamps[i].temperature))lamps[i].temperature=NAN;
  return true;
}
void clockFor(int i,tm& local) {
  // Each lamp can use its own POSIX timezone. All access is on the main loop.
  String zone=workspace["lamps"][i]["timezone"]|defaultZone;setenv("TZ",zone.c_str(),1);tzset();time_t now=time(nullptr);localtime_r(&now,&local);
}
void levelsFrom(JsonVariantConst values,int* target,int n) {for(int j=0;j<n;j++)target[j]=values[j]|0;}
bool programLevels(JsonArray points,int minute,int* target,int n) {
  if(points.size()<2)return false;int right=0;
  for(size_t p=0;p<points.size();p++)if(points[p]["minute"].as<int>()>minute){right=p;break;}
  JsonObject a=points[(right+points.size()-1)%points.size()],b=points[right];
  int span=(b["minute"].as<int>()-a["minute"].as<int>()+1440)%1440;if(!span)return false;
  float fraction=float((minute-a["minute"].as<int>()+1440)%1440)/span;
  for(int j=0;j<n;j++){int av=a["levels"][j]|0,bv=b["levels"][j]|0;target[j]=lroundf(av+(bv-av)*fraction);}return true;
}
void returnToLampProgram(int i) {
  Lamp& lamp=lamps[i];lamp.scene="";lamp.until=0;
  if(String(workspace["lamps"][i]["program_execution"]|"")=="device"){
    lamp.mode="firmware";lamp.releaseRequested=true;
  }else lamp.mode="firmware";
  lamp.force=true;
}
void expire(int i) {
  Lamp& lamp=lamps[i];
  if(lamp.previewEnd&&expired(lamp.previewEnd)){lamp.previewEnd=0;if(lamp.mode=="firmware")lamp.releaseRequested=true;lamp.force=true;logEvent(i,"Vorschau beendet: vorheriger Lichtmodus");}
  if(lamp.until&&expired(lamp.until)){returnToLampProgram(i);logEvent(i,"Timer/HA-Lease beendet: lokales Lampenprogramm");}
}
bool desired(int i,int* target) {
  Lamp& lamp=lamps[i];expire(i);
  if(lamp.previewEnd){for(int j=0;j<channels(i);j++)target[j]=lamp.previewLevels[j];return true;}
  if(lamp.mode=="firmware")return false;
  if(lamp.mode!="schedule"){
    if(lamp.scene.length())levelsFrom(workspace["scenes"][lamp.scene]["levels"],target,channels(i));
    else for(int j=0;j<channels(i);j++)target[j]=lamp.levels[j];return true;
  }
  if(time(nullptr)<1700000000){lamp.error="Warte auf gültige NTP-Uhrzeit";return false;}
  String id=workspace["lamps"][i]["program"]|"";
  JsonArray points=id.length()?workspace["programs"][id]["points"].as<JsonArray>():workspace["lamps"][i]["points"].as<JsonArray>();
  tm local;clockFor(i,local);return programLevels(points,local.tm_hour*60+local.tm_min,target,channels(i));
}
bool syncLampClock(int i,String* fields,int roads) {
  if(time(nullptr)<1700000000)return false;
  time_t now=time(nullptr);tm local;clockFor(i,local);tm utc;gmtime_r(&now,&utc);utc.tm_isdst=-1;
  long offset=long(difftime(mktime(&local),mktime(&utc)));long deviceOffset=fields[23+2*(roads-6)].toInt()*3600L;String reply;
  if(!fetch(i,"clock="+String(long(now)+offset-deviceOffset),reply))return false;
  lamps[i].lastClockSync=millis();return true;
}
bool deployProgram(int i,String id) {
  Lamp& lamp=lamps[i];String fields[32];int n;
  if(time(nullptr)<1700000000){lamp.error="Warte vor dem Speichern auf gültige Netzwerkzeit";return false;}
  if(!lampState(i,fields,n)){if(lamp.error.isEmpty())lamp.error="Lampe für Programmspeicherung nicht erreichbar";return false;}
  if(fields[0]!="on"){lamp.error="Bitte die Lampe vor dem Speichern einschalten";return false;}
  JsonArray points=workspace["programs"][id]["points"].as<JsonArray>();int roads=n>28?8:6,physical=channels(i),target[8]={};
  String query="save=onx1";for(int k=2;k<5;k++)query+="x"+fields[k];
  for(int j=0;j<roads;j++)query+="x"+fields[5+j];
  for(int j=0;j<roads;j++){
    String row;
    for(int hour=0;hour<24;hour++){
      if(!programLevels(points,hour*60,target,physical)){lamp.error="Tagesprogramm kann nicht interpoliert werden";return false;}
      if(hour==23&&j<physical&&target[j]>0){lamp.error="Licht nach 23 Uhr benötigt externe Zeitplansteuerung; native Schaltzeit unbestätigt";return false;}
      if(hour)row+='y';row+=String(j<physical?target[j]:0);
    }
    query+="x"+row;
  }
  for(JsonObject point:points)if(point["minute"].as<int>()>=1380)for(JsonVariant level:point["levels"].as<JsonArray>())if(level.as<int>()>0){lamp.error="Licht nach 23 Uhr benötigt externe Zeitplansteuerung";return false;}
  query+="x0x23x"+fields[23+2*(roads-6)];
  String reply;if(!fetch(i,query,reply))return false;
  // Keep the autonomous schedule aligned with this fixture's own timezone,
  // including DST even though the lamp stores only a whole-hour UTC offset.
  if(!syncLampClock(i,fields,roads))return false;
  String confirmed[32];int confirmedCount;if(!lampState(i,confirmed,confirmedCount)||confirmed[1]!="1"){
    lamp.error="Lampenprogramm konnte nicht bestätigt werden";return false;
  }
  String saved="save=onx1";for(int k=2;k<5+roads;k++)saved+="x"+confirmed[k];
  for(int k=5+roads;k<5+2*roads;k++){String row=confirmed[k];row.replace(',', 'y');saved+="x"+row;}
  saved+="x"+confirmed[19+2*(roads-6)]+"x"+confirmed[20+2*(roads-6)]+"x"+confirmed[23+2*(roads-6)];
  if(saved!=query){lamp.error="Gespeicherte Tageskurve oder Schaltzeiten weichen ab";return false;}
  int hour=-1,minute=-1;tm local;clockFor(i,local);
  int parsed=sscanf(confirmed[18+2*(roads-6)].c_str(),"%d,%d",&hour,&minute);
  int delta=abs(hour*60+minute-local.tm_hour*60-local.tm_min);
  if(parsed!=2||hour<0||hour>23||minute<0||minute>59||min(delta,1440-delta)>2){lamp.error="Lampenuhr stimmt nach Synchronisierung nicht";return false;}
  lamp.mode="firmware";lamp.scene="";lamp.until=lamp.previewEnd=0;lamp.releaseRequested=false;lamp.force=false;lamp.online=true;lamp.error="";
  logEvent(i,"Tagesprogramm dauerhaft auf Lampe gespeichert: "+id);return true;
}
void output(int i) {
  Lamp& lamp=lamps[i];if(WiFi.status()!=WL_CONNECTED)return;int target[8]={};bool hasTarget=desired(i,target);
  String fields[32];int n;
  if(!lamp.releaseRequested&&!hasTarget){
    if(lamp.mode!="firmware")return;
    bool wasOnline=lamp.online;if(!lampState(i,fields,n)){lamp.online=false;if(lamp.error.isEmpty())lamp.error="Unbekanntes Lampenformat";return;}
    int roads=n>28?8:6;if((!wasOnline||!lamp.lastClockSync||millis()-lamp.lastClockSync>=6UL*60*60*1000)&&time(nullptr)>=1700000000)syncLampClock(i,fields,roads);
    lamp.online=true;lamp.error="";return;
  }
  if(!lampState(i,fields,n)){lamp.online=false;if(lamp.error.isEmpty())lamp.error="Unbekanntes Lampenformat; kein Schreibzugriff";logEvent(i,lamp.error);return;}
  lamp.online=true;int roads=n>28?8:6;String reply;
  logEvent(i,"Ausgabe "+lamp.mode+"; power="+fields[0]+", mode="+fields[1]);
  if(lamp.releaseRequested||fields[1]!="0"||fields[0]!="on"){
    String query=lamp.releaseRequested?"save=onx1":"save=onx0";for(int k=2;k<5;k++)query+="x"+fields[k];
    for(int j=0;j<roads;j++)query+="x"+(!lamp.releaseRequested&&j<channels(i)?String(target[j]):fields[5+j]);
    for(int j=0;j<roads;j++){String row=fields[5+roads+j];row.replace(',','y');query+="x"+row;}
    query+="x"+fields[19+2*(roads-6)]+"x"+fields[20+2*(roads-6)]+"x"+fields[23+2*(roads-6)];
    if(!fetch(i,query,reply)){logEvent(i,lamp.error);return;}
  }else for(int j=0;j<channels(i);j++)if(!fetch(i,String(codes[j])+"="+String(lroundf(target[j]*1023.f/100)),reply)){logEvent(i,lamp.error);return;}
  if(lamp.releaseRequested){lamp.mode="firmware";lamp.releaseRequested=false;}else for(int j=0;j<channels(i);j++)lamp.sent[j]=target[j];lamp.error="";
}
double deadline(uint32_t end) {return end&&!expired(end)?double(time(nullptr))+double(end-millis())/1000:0;}
void status() {
  DynamicJsonDocument doc(48000);
  doc["version"]=BRIDGE_VERSION;doc["now_epoch"]=double(time(nullptr));doc["clock_valid"]=time(nullptr)>=1700000000;doc["ip"]=WiFi.localIP().toString();
  JsonObject update=doc.createNestedObject("update");update["available"]=updateAvailable;update["latest"]=latestVersion;update["installer_url"]=installerUrl;update["error"]=updateError;
  JsonArray lights=doc.createNestedArray("lights");
  for(int i=0;i<count();i++){
    expire(i);Lamp& lamp=lamps[i];JsonObject state=lights.createNestedObject();tm local;clockFor(i,local);
    state["minute"]=local.tm_hour*60+local.tm_min;state["online"]=lamp.online;state["mode"]=lamp.mode;state["power"]="on";state["error"]=lamp.error;
    state["power"]=lamp.power;state["device_clock"]=lamp.deviceClock;state["device_timezone"]=lamp.deviceTimezone;state["open_hour"]=lamp.openHour;state["close_hour"]=lamp.closeHour;
    String program=workspace["lamps"][i]["program"]|"",execution=workspace["lamps"][i]["program_execution"]|"hold";
    if(program.length())state["program"]=program;state["program_execution"]=execution;state["startup_hold"]=program.length()&&execution!="device";
    if(isfinite(lamp.temperature))state["temperature"]=lamp.temperature;
    if(lamp.scene.length()) {JsonObject scene=state.createNestedObject("scene");scene["id"]=lamp.scene;if(lamp.until)scene["until"]=deadline(lamp.until);else scene["until"]=nullptr;scene["manual"]=lamp.mode=="manual";}
    if(lamp.previewEnd){JsonObject preview=state.createNestedObject("preview");preview["until"]=deadline(lamp.previewEnd);}
    if(lamp.mode!="firmware"){JsonArray sent=state.createNestedArray("sent");for(int j=0;j<channels(i);j++)sent.add(lamp.sent[j]);}
    JsonArray log=state.createNestedArray("log");unsigned start=lamp.logCount>32?lamp.logCount-32:0;
    for(unsigned j=start;j<lamp.logCount;j++){JsonObject event=log.createNestedObject();event["at"]=String(j+1);event["message"]=lamp.logs[j%32];}
  }
  // Preserve the first fixture's REST contract used by the HA lease package.
  if(count()){
    doc["mode"]=lamps[0].previewEnd?"preview":lamps[0].mode;doc["lamp_online"]=lamps[0].online;doc["error"]=lamps[0].error;if(isfinite(lamps[0].temperature))doc["temperature"]=lamps[0].temperature;
    uint32_t end=lamps[0].previewEnd?lamps[0].previewEnd:lamps[0].until;doc["remaining_seconds"]=end&&!expired(end)?(end-millis()+999)/1000:0;
  }
  // Stream the shared library once, avoiding a 4x copy of the JSON in RAM.
  size_t size=measureJson(doc)+measureJson(workspace)+13;
  server.setContentLength(size);server.send(200,"application/json","");
  WiFiClient client=server.client();client.print("{\"workspace\":");serializeJson(workspace,client);client.print(',');
  String state;serializeJson(doc,state);client.print(state.substring(1));
}
void studio(JsonDocument& msg) {
  String action=msg["action"]|"",kind=msg["kind"]|"programs",id=msg["item_id"]|"",serial=msg["serial"]|"";
  if(kind!="programs"&&kind!="scenes"){fail("Ungültige Bibliothek");return;}
  bool targets[MAX_LAMPS]={};int targetCount=0;
  if(serial.startsWith("group:")){
    JsonVariant group=workspace["groups"][serial.substring(6)];if(group.isNull()){fail("Gruppe nicht gefunden");return;}
    for(int i=0;i<count();i++)for(JsonVariant member:group["serials"].as<JsonArray>())if(member.as<String>()==workspace["lamps"][i]["serial"].as<String>()){targets[i]=true;targetCount++;}
  }else for(int i=0;i<count();i++)if(serial==workspace["lamps"][i]["serial"].as<String>()){targets[i]=true;targetCount++;}
  if(action=="save"||action=="delete"||action=="group_save"||action=="group_delete"){
    DynamicJsonDocument next(65536);next.set(workspace);
    if(action.startsWith("group_")) {
      if(id.length()&&!next["groups"].containsKey(id)){fail("Gruppe wurde gelöscht");return;}
      if(action=="group_delete")next["groups"].remove(id);
      else{if(!id.length())id=newId();next["groups"][id]["name"]=msg["name"];next["groups"][id]["serials"]=msg["serials"];}
    }else{
      if(id.length()&&!next[kind].containsKey(id)){fail("Eintrag wurde gelöscht");return;}
      if(action=="delete"){
        for(int i=0;i<count();i++)if((kind=="programs"&&workspace["lamps"][i]["program"].as<String>()==id)||(kind=="scenes"&&lamps[i].scene==id)){fail("Bitte zuerst das aktive Profil beenden");return;}
        next[kind].remove(id);
      }else{if(!id.length())id=newId();next[kind][id]=msg["item"];}
    }
    if(persist(next)){
      if(action=="save"&&kind=="programs")for(int i=0;i<count();i++)if(
        workspace["lamps"][i]["program"].as<String>()==id&&
        String(workspace["lamps"][i]["program_execution"]|"")=="device"
      )if(!deployProgram(i,id)){fail(lamps[i].error);return;}
      ok();
    }return;
  }
  if(!targetCount){fail("Lampe nicht gefunden");return;}
  if(action=="activate"||action=="manual"){
    if(!workspace[kind].containsKey(id)||(action=="manual"&&kind!="scenes")){fail("Bitte ein gespeichertes Profil wählen");return;}
    int minutes=msg["duration"]|15;if(minutes<1||minutes>60){fail("Dauer: 1–60 Minuten");return;}
    if(kind=="programs") {
      for(int i=0;i<count();i++)if(targets[i]&&!deployProgram(i,id)){fail(lamps[i].error);return;}
      DynamicJsonDocument next(65536);next.set(workspace);for(int i=0;i<count();i++)if(targets[i]){
        next["lamps"][i]["program"]=id;next["lamps"][i]["program_execution"]="device";
      }
      if(persist(next))ok();return;
    }
    for(int i=0;i<count();i++)if(targets[i]){Lamp& lamp=lamps[i];lamp.releaseRequested=false;lamp.previewEnd=0;lamp.scene=id;lamp.mode=action=="manual"?"manual":"scene";lamp.until=action!="manual"?millis()+minutes*60000UL:0;lamp.force=true;logEvent(i,"Aktiviert: "+id);}ok();return;
  }
  if(action=="preview"){
    int seconds=msg["preview_seconds"]|60;if(seconds<1||seconds>3600||!validLevels(msg["levels"])){fail("Ungültige Vorschau");return;}
    for(int i=0;i<count();i++)if(targets[i]){levelsFrom(msg["levels"],lamps[i].previewLevels,channels(i));lamps[i].previewEnd=millis()+seconds*1000UL;lamps[i].force=true;}ok();return;
  }
  if(action!="preview_cancel"&&action!="resume"&&action!="release"){fail("Unbekannte Aktion");return;}
  for(int i=0;i<count();i++)if(targets[i]){
    lamps[i].previewEnd=0;
    if(action=="resume"||action=="release"||(
      action=="preview_cancel"&&lamps[i].mode=="firmware"
    ))returnToLampProgram(i);else lamps[i].force=true;
  }ok();
}
void checkUpdate() {
  lastUpdateCheck=millis();WiFiClientSecure client;client.setInsecure();HTTPClient http;http.setConnectTimeout(1200);http.setTimeout(2000);
  if(!http.begin(client,String(installerUrl)+"bridge-update.json"))return;int code=http.GET();String raw=code==200?http.getString():"";http.end();
  DynamicJsonDocument data(1024);if(code!=200||deserializeJson(data,raw)){updateError="Update-Prüfung derzeit nicht erreichbar";return;}
  String remote=data["version"]|"";int a,b,c,x,y,z;
  if(sscanf(remote.c_str(),"%d.%d.%d",&a,&b,&c)!=3||sscanf(BRIDGE_VERSION,"%d.%d.%d",&x,&y,&z)!=3)return;
  latestVersion=remote;updateAvailable=a>x||(a==x&&(b>y||(b==y&&c>z)));updateError="";
}
void portal(){server.sendHeader("Location","http://192.168.4.1/",true);server.send(302,"text/plain","");}
void setup() {
  Serial.begin(115200);prefs.begin("reefbridge",false);fsReady=LittleFS.begin(false);
  // Initialize only on first use; a later mount failure must retain the data.
  if(!fsReady&&!prefs.getBool("studio_fs",false)){Serial.println("Initializing Studio filesystem");fsReady=LittleFS.begin(true);}
  if(fsReady)prefs.putBool("studio_fs",true);
  loadWorkspace();apPassword=prefs.getString("ap_pass","ReefBridge2026!");
  WiFi.mode(WIFI_AP_STA);String ssid="ReefBridge-"+String((uint32_t)ESP.getEfuseMac(),HEX);WiFi.softAP(ssid.c_str(),apPassword.c_str());dnsServer.start(53,"*",WiFi.softAPIP());
  WiFi.begin(prefs.getString("ssid","").c_str(),prefs.getString("wifi_pass","").c_str());configTime(0,0,"pool.ntp.org","time.cloudflare.com");
  Serial.println("Setup: http://192.168.4.1 | AP password: "+apPassword);
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",(const char*)WEB_UI,sizeof(WEB_UI));});
  for(const char* probe:{"/generate_204","/hotspot-detect.html","/connecttest.txt"})server.on(probe,HTTP_GET,portal);
  server.on("/api/status",HTTP_GET,status);
  server.on("/api/studio",HTTP_POST,[]{DynamicJsonDocument msg(16000);if(body(msg))studio(msg);});
  server.on("/api/fixture",HTTP_POST,[]{
    DynamicJsonDocument msg(2048);if(!body(msg))return;int index=msg["index"]|-1;
    if(index<0)index=count();if(index>count()||index>=MAX_LAMPS||!validFixture(msg.as<JsonVariant>())){fail("Bitte Name, IP, Seriennummer, Zeitzone und 6/8 Kanäle prüfen (maximal vier Lampen).");return;}
    DynamicJsonDocument next(65536);next.set(workspace);JsonObject lamp;
    if(index==count()){lamp=next["lamps"].createNestedObject();fallbackPoints(lamp);}else lamp=next["lamps"][index].as<JsonObject>();
    String oldSerial=lamp["serial"]|"";
    for(const char* key:{"name","serial","lamp_ip","channels","timezone"})lamp[key]=msg[key];
    for(JsonPair pair:next["groups"].as<JsonObject>())for(JsonVariant member:pair.value()["serials"].as<JsonArray>())if(member.as<String>()==oldSerial)member.set(msg["serial"]);
    if(persist(next)){lamps[index]=Lamp();ok();}
  });
  server.on("/api/restore",HTTP_POST,[]{DynamicJsonDocument next(65536);if(body(next)&&persist(next)){for(int i=0;i<MAX_LAMPS;i++)lamps[i]=Lamp();ok();}});
  server.on("/api/wifi",HTTP_POST,[]{DynamicJsonDocument msg(1024);if(!body(msg))return;String ssid=msg["ssid"]|"",pass=msg["password"]|"";if(!ssid.length()||ssid.length()>32||pass.length()>63){fail("WLAN-Daten ungültig");return;}prefs.putString("ssid",ssid);prefs.putString("wifi_pass",pass);ok();WiFi.begin(ssid.c_str(),pass.c_str());});
  server.on("/api/control",HTTP_POST,[]{
    DynamicJsonDocument msg(1024);if(!body(msg))return;if(!count()){fail("Bitte zuerst eine Lampe einrichten");return;}
    String action=msg["action"]|"";Lamp& lamp=lamps[0];int seconds=msg["seconds"]|90;
    if(action=="resume"){lamp.previewEnd=0;returnToLampProgram(0);ok();return;}
    if(action!="lease"&&action!="preview"&&action!="manual"){fail("Unbekannte Aktion");return;}
    if(!validLevels(msg["levels"],channels(0))||seconds<1||seconds>(action=="lease"?90:3600)){fail("Pegel/Dauer ungültig");return;}
    if(action=="lease"&&lamp.mode=="manual"){fail("Manueller Modus aktiv",409);return;}
    if(action=="preview"){levelsFrom(msg["levels"],lamp.previewLevels,channels(0));lamp.previewEnd=millis()+seconds*1000UL;}
    else{levelsFrom(msg["levels"],lamp.levels,channels(0));lamp.mode=action;lamp.scene="";lamp.until=action=="manual"?0:millis()+seconds*1000UL;lamp.previewEnd=0;}lamp.force=true;ok();
  });
  server.onNotFound([]{if(server.uri().startsWith("/api/")){fail("Schnittstelle nicht gefunden",404);return;}if(WiFi.softAPgetStationNum())portal();else fail("Nicht gefunden",404);});server.begin();
}
void loop() {
  dnsServer.processNextRequest();server.handleClient();
  if(Serial.available()){String command=Serial.readStringUntil('\n');command.trim();if(command=="INFO")Serial.println("AP password: "+apPassword+"\nIP: "+WiFi.localIP().toString());}
  if(WiFi.status()==WL_CONNECTED){static bool started=false;if(!started){MDNS.begin("reefbridge");dnsServer.stop();WiFi.softAPdisconnect(true);started=true;}
    // Round-robin: one lamp transaction per loop keeps HTTP/DNS responsive
    // between fixtures; each fixture has independent mode, timers and clock.
    static int cursor=0;for(int n=0;n<count();n++){int i=(cursor+n)%count();expire(i);if(lamps[i].force||millis()-lamps[i].lastOutput>=30000){lamps[i].force=false;lamps[i].lastOutput=millis();output(i);cursor=(i+1)%count();break;}}
    if(!lastUpdateCheck||millis()-lastUpdateCheck>=6UL*60*60*1000)checkUpdate();
  }else if(millis()-lastReconnect>30000){lastReconnect=millis();WiFi.reconnect();}
  delay(2);
}
