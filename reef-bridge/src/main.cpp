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
const IPAddress setupIp(192,168,8,1),setupGateway(192,168,8,1),setupMask(255,255,255,0);
struct Lamp {
  String mode="firmware", scene, error;
  int indicatorLight=-1; // Last command only; no vendor readback is documented.
  int physicalResume=-1; // Last turnoffset command only; no readback is documented.
  String fwCurrent,fwLatest,fwVendorModel,fwNotes,fwError;
  uint32_t fwAttempt=0;time_t fwChecked=0;bool fwQueried=false,fwAvailable=false;
  int levels[8]={}, sent[8]={}, previewLevels[8]={};
  uint32_t until=0, previewEnd=0, lastOutput=0, lastClockSync=0;
  bool online=false, force=true, releaseRequested=false;
  float temperature=NAN;
  String logs[32]; unsigned logCount=0;
  String deviceClock, deviceTimezone, power, observed;
  int openHour=0, closeHour=0;
} lamps[MAX_LAMPS];
bool fsReady=false, updateAvailable=false, provisioningAp=false;
String latestVersion, updateError, apPassword, apSsid;
uint32_t lastUpdateCheck=0, lastReconnect=0, provisioningApStop=0;
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
void startSetupAp(bool provisioning=false) {
  WiFi.softAPdisconnect(true);delay(100);WiFi.softAPConfig(setupIp,setupGateway,setupMask);
  WiFi.softAP(apSsid.c_str(),apPassword.c_str());dnsServer.start(53,"*",setupIp);
  provisioningAp=provisioning;if(provisioning)provisioningApStop=millis()+10UL*60*1000;
}
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
  if(n<25){
    lamps[i].error="Ungültige Lampenantwort ("+String(n)+" Felder); kein Schreibzugriff";
    return false;
  }
  String reportedSerial=fields[21+2*(roads-6)], configuredSerial=workspace["lamps"][i]["serial"].as<String>();
  if(reportedSerial!=configuredSerial){
    lamps[i].error="Seriennummer passt nicht: erwartet "+configuredSerial+", gemeldet "+reportedSerial+"; kein Schreibzugriff";
    return false;
  }
  if(channels(i)>roads){
    lamps[i].error="Kanalprofil passt nicht: eingerichtet "+String(channels(i))+", Firmware "+String(roads)+" Kanäle; kein Schreibzugriff";
    return false;
  }
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
  lamps[i].lastClockSync=millis();logEvent(i,"Uhrkorrektur gesendet; Bestätigung beim nächsten Statusabruf");return true;
}
bool lampClockWrong(int i,String* fields,int roads) {
  tm local;clockFor(i,local);int hour=-1,minute=-1;
  String clock=fields[18+2*(roads-6)];clock.replace(':', ',');
  if(sscanf(clock.c_str(),"%d,%d",&hour,&minute)!=2||hour<0||hour>23||minute<0||minute>59)return true;
  int delta=abs(hour*60+minute-local.tm_hour*60-local.tm_min);
  return min(delta,1440-delta)>2;
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
      if(hour)row+='y';row+=String(j<physical?target[j]:0);
    }
    query+="x"+row;
  }
  // APK 3.1.93 disables the separate on/off timer with 0/0. A close value
  // of 23 really switches the fixture off and must not delimit the curve.
  query+="x0x0x"+fields[23+2*(roads-6)];
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
  logEvent(i,"Tagesprogramm dauerhaft gespeichert; Zusatz-Schalttimer 0–0 (aus): "+id);return true;
}
void output(int i) {
  Lamp& lamp=lamps[i];if(WiFi.status()!=WL_CONNECTED)return;int target[8]={};bool hasTarget=desired(i,target);
  String fields[32];int n;
  if(!lamp.releaseRequested&&!hasTarget){
    if(lamp.mode!="firmware")return;
    bool wasOnline=lamp.online;if(!lampState(i,fields,n)){lamp.online=false;if(lamp.error.isEmpty())lamp.error="Unbekanntes Lampenformat";return;}
    int roads=n>28?8:6;
    if(time(nullptr)>=1700000000&&(!wasOnline||!lamp.lastClockSync||millis()-lamp.lastClockSync>=6UL*60*60*1000||(millis()-lamp.lastClockSync>=60000&&lampClockWrong(i,fields,roads)))){
      logEvent(i,"Lampenuhr prüfen/korrigieren: "+lamp.deviceClock);
      if(!syncLampClock(i,fields,roads)){lamp.error="Uhrkorrektur fehlgeschlagen";return;}
    }
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
bool fwIdentifier(String value){if(!value.length()||value.length()>64)return false;for(size_t j=0;j<value.length();j++)if(!isalnum((unsigned char)value[j])&&value[j]!='-'&&value[j]!='_')return false;return true;}
bool fwVersion(String value){int dots=0,digits=0;for(size_t j=0;j<value.length();j++){char c=value[j];if(c=='.'){if(!digits||++dots>3)return false;digits=0;}else if(c<'0'||c>'9'||++digits>10)return false;}return digits>0;}
int fwCompare(String a,String b){for(int part=0;part<4;part++){int ea=a.indexOf('.'),eb=b.indexOf('.');String x=ea<0?a:a.substring(0,ea),y=eb<0?b:b.substring(0,eb);uint64_t vx=strtoull(x.c_str(),nullptr,10),vy=strtoull(y.c_str(),nullptr,10);if(vx!=vy)return vx>vy?1:-1;a=ea<0?"":a.substring(ea+1);b=eb<0?"":b.substring(eb+1);}return 0;}
String fwEncode(String text){String result;const char* hex="0123456789ABCDEF";for(size_t j=0;j<text.length();j++){unsigned char c=text[j];if(isalnum(c)||c=='-'||c=='_')result+=char(c);else{result+='%';result+=hex[c>>4];result+=hex[c&15];}}return result;}
bool apRequest(IPAddress gateway,String query,String& response,uint16_t timeout=3000){
  HTTPClient http;http.setConnectTimeout(1500);http.setTimeout(timeout);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if(!http.begin("http://"+gateway.toString()+"/?"+query))return false;
  int code=http.GET(),length=http.getSize();if(code==200&&(length<0||length<=4096))response=http.getString();http.end();
  return code==200&&response.length()<=4096;
}
bool factoryIdentity(String raw,String& serial,String& model,int& roads){
  int begin=raw.indexOf("on|"),off=raw.indexOf("off|");if(off>=0&&(begin<0||off<begin))begin=off;
  if(begin<0)return false;raw=raw.substring(begin);int quote=raw.indexOf('"');if(quote>=0)raw=raw.substring(0,quote);quote=raw.indexOf('\'');if(quote>=0)raw=raw.substring(0,quote);
  String fields[32];int n=0,start=0,end;
  do{end=raw.indexOf('|',start);if(n>=32)return false;fields[n]=end<0?raw.substring(start):raw.substring(start,end);fields[n++].trim();start=end+1;}while(end>=0);
  roads=n>28?8:6;int d2=2*(roads-6);if(n<25+d2)return false;serial=fields[21+d2];model=fields[24+d2];
  return fwIdentifier(serial)&&fwIdentifier(model);
}
void reconnectHome(){
  String homeSsid=prefs.getString("ssid",""),homePass=prefs.getString("wifi_pass","");
  WiFi.disconnect(false,false);if(homeSsid.length())WiFi.begin(homeSsid.c_str(),homePass.c_str());
}
void provisionLamp(JsonDocument& msg){
  String lampSsid=msg["lamp_ssid"]|"",lampPass=msg["lamp_password"]|"",name=msg["name"]|"",zone=msg["timezone"]|defaultZone;
  int requestedChannels=msg["channels"]|0;String homeSsid=prefs.getString("ssid",""),homePass=prefs.getString("wifi_pass","");
  auto abortProvision=[&](String message){reconnectHome();fail(message);};
  if(!provisioningAp){fail("Bitte zuerst das Einrichtungs-WLAN der Reef Bridge starten.",409);return;}
  if(!homeSsid.length()){fail("Bitte zuerst das Heim-WLAN der Reef Bridge speichern.",409);return;}
  if(count()>=MAX_LAMPS||!validName(msg["name"])||!lampSsid.length()||lampSsid.length()>32||
     (lampPass.length()&&lampPass.length()<8)||lampPass.length()>63||(requestedChannels!=6&&requestedChannels!=8)||!zone.length()||zone.length()>96){
    fail("Bitte Lampen-Hotspot, Name, Kanalzahl und Zeitzone prüfen.");return;
  }
  WiFi.disconnect(false,false);delay(150);WiFi.begin(lampSsid.c_str(),lampPass.c_str());
  uint32_t deadline=millis()+20000;while(WiFi.status()!=WL_CONNECTED&&!expired(deadline))delay(100);
  if(WiFi.status()!=WL_CONNECTED){abortProvision("Der ausgewählte Lampen-Hotspot konnte nicht verbunden werden.");return;}
  IPAddress gateway=WiFi.gatewayIP();if(gateway!=IPAddress(192,168,4,1)){abortProvision("Sicherheitsabbruch: Der Lampen-Hotspot verwendet nicht die bestätigte Adresse 192.168.4.1.");return;}
  String raw,serial,model;int firmwareRoads=0;
  if(!apRequest(gateway,"read=config",raw)||!factoryIdentity(raw,serial,model,firmwareRoads)){
    abortProvision("Sicherheitsabbruch: Der Hotspot antwortet nicht mit einer gültigen AIPAI-Lampenidentität.");return;
  }
  if(requestedChannels>firmwareRoads){abortProvision("Das gewählte Kanalprofil passt nicht zur erkannten Lampen-Firmware.");return;}
  String reply,connectQuery="sta=apconnect&ssid="+fwEncode(homeSsid)+"&pwd="+fwEncode(homePass)+"&callback=1";
  if(!apRequest(gateway,connectQuery,reply,5000)){abortProvision("Die WLAN-Daten wurden von der Lampe nicht angenommen.");return;}
  String networkInfo;for(int attempt=0;attempt<5&&!networkInfo.length();attempt++){
    delay(3000);String candidate;if(apRequest(gateway,"sta=getip&callback=1",candidate,5000)&&candidate!="false")networkInfo=candidate;
  }
  int first=networkInfo.indexOf(','),second=networkInfo.indexOf(',',first+1);String ipText=first>0?networkInfo.substring(0,first):"";
  String reportedSerial=second>first?networkInfo.substring(first+1,second):"",activation=second>first?networkInfo.substring(second+1):"";
  ipText.trim();reportedSerial.trim();activation.trim();IPAddress lampIp;
  if(!lampIp.fromString(ipText)||lampIp==gateway||reportedSerial!=serial){abortProvision("Die Lampe hat nach der WLAN-Übertragung keine bestätigte IP und Seriennummer geliefert.");return;}
  DynamicJsonDocument next(65536);next.set(workspace);JsonObject fixture=next["lamps"].createNestedObject();fallbackPoints(fixture);
  fixture["name"]=name;fixture["serial"]=serial;fixture["lamp_ip"]=ipText;fixture["channels"]=requestedChannels;fixture["timezone"]=zone;
  if(!validWorkspace(next)){abortProvision("Die erkannte Lampe kann nicht sicher in die Bridge-Konfiguration übernommen werden.");return;}
  apRequest(gateway,"node=restart",reply,1500);reconnectHome();
  if(!persist(next))return;lamps[count()-1]=Lamp();
  DynamicJsonDocument result(512);result["ok"]=true;result["serial"]=serial;result["model"]=model;result["lamp_ip"]=ipText;result["activation"]=activation;
  String json;serializeJson(result,json);server.send(200,"application/json",json);
}
class FirmwareBuffer:public Stream{
 public:String data;bool overflow=false;
 int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}
 size_t write(uint8_t c)override{return write(&c,1);}
 size_t write(const uint8_t* buffer,size_t size)override{if(data.length()+size>8192){overflow=true;return 0;}data.concat((const char*)buffer,size);return size;}
};
bool fwQuery(String sql,DynamicJsonDocument& data){
 HTTPClient http;http.setConnectTimeout(1500);http.setTimeout(4000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
 if(!http.begin("http://ahome.doseen.com/api.ashx?action=data_read&sql="+fwEncode(sql)))return false;
 int code=http.GET();FirmwareBuffer buffer;int read=code==200?http.writeToStream(&buffer):-1;http.end();
 return code==200&&read>=0&&!buffer.overflow&&!deserializeJson(data,buffer.data)&&data.is<JsonArray>()&&data.size()==1;
}
void checkLampFirmware(int i,bool batch=false){
 Lamp& lamp=lamps[i];if(lamp.fwQueried&&millis()-lamp.fwAttempt<60000){if(!batch)fail("Bitte eine Minute zwischen Herstellerabfragen warten");return;}
 lamp.fwAttempt=millis();lamp.fwQueried=true;lamp.fwCurrent="";lamp.fwLatest="";lamp.fwVendorModel="";lamp.fwNotes="";lamp.fwError="Herstellerabfrage fehlgeschlagen oder Antwort ungültig";lamp.fwAvailable=false;lamp.fwChecked=time(nullptr);
 String serial=workspace["lamps"][i]["serial"]|"";DynamicJsonDocument device(12288),release(12288);
 if(fwIdentifier(serial)&&fwQuery("select d_sn,d_class,d_ver from tbl_device where d_sn='"+serial+"'",device)&&device[0]["d_sn"].as<String>()==serial){
  String model=device[0]["d_class"]|"",current=device[0]["d_ver"].as<String>();
  if(fwIdentifier(model)&&fwVersion(current)&&fwQuery("select classname,status,remark from tbl_class where classname='"+model+"'",release)&&release[0]["classname"].as<String>()==model){
   String latest=release[0]["status"].as<String>();if(fwVersion(latest)){lamp.fwCurrent=current;lamp.fwLatest=latest;lamp.fwVendorModel=model;lamp.fwNotes=release[0]["remark"].as<String>().substring(0,512);lamp.fwAvailable=fwCompare(latest,current)>0;lamp.fwError="";}
  }
 }
 logEvent(i,lamp.fwError.length()?lamp.fwError:"Hersteller meldet Firmware "+lamp.fwCurrent+"; neueste "+lamp.fwLatest+"; nicht lokal bestätigt");if(!batch)ok();
}
void status() {
  DynamicJsonDocument doc(56000);
  doc["version"]=BRIDGE_VERSION;doc["now_epoch"]=double(time(nullptr));doc["clock_valid"]=time(nullptr)>=1700000000;doc["ip"]=WiFi.localIP().toString();
  JsonObject provision=doc.createNestedObject("provision");provision["active"]=provisioningAp;provision["ssid"]=apSsid;provision["url"]="http://192.168.8.1";provision["home_configured"]=prefs.getString("ssid","").length()>0;
  JsonObject update=doc.createNestedObject("update");update["available"]=updateAvailable;update["latest"]=latestVersion;update["installer_url"]=installerUrl;update["error"]=updateError;
  JsonArray lights=doc.createNestedArray("lights");
  for(int i=0;i<count();i++){
    expire(i);Lamp& lamp=lamps[i];JsonObject state=lights.createNestedObject();tm local;clockFor(i,local);
    state["minute"]=local.tm_hour*60+local.tm_min;state["online"]=lamp.online;state["mode"]=lamp.mode;state["power"]="on";state["error"]=lamp.error;
    state["power"]=lamp.power;state["device_clock"]=lamp.deviceClock;state["device_timezone"]=lamp.deviceTimezone;state["open_hour"]=lamp.openHour;state["close_hour"]=lamp.closeHour;
    state["native_timer_enabled"]=lamp.openHour||lamp.closeHour;
    if(lamp.indicatorLight<0)state["indicator_light"]=nullptr;else state["indicator_light"]=lamp.indicatorLight==1;
    if(lamp.physicalResume<0)state["physical_resume"]=nullptr;else state["physical_resume"]=lamp.physicalResume==1;
    JsonObject fw=state.createNestedObject("firmware");fw["source"]="manufacturer_http";fw["local_version"]=nullptr;fw["install_allowed"]=false;fw["block_reason"]="Installation gesperrt: Herstellerweg ohne belegte Firmware-Signaturprüfung und ohne verifizierte lokale Versionskontrolle.";
    if(lamp.fwCurrent.length())fw["manufacturer_version"]=lamp.fwCurrent;if(lamp.fwLatest.length())fw["latest"]=lamp.fwLatest;fw["vendor_model"]=lamp.fwVendorModel;fw["notes"]=lamp.fwNotes;fw["error"]=lamp.fwError;
    if(lamp.fwQueried&&!lamp.fwError.length())fw["available"]=lamp.fwAvailable;else fw["available"]=nullptr;
    if(lamp.fwChecked>=1700000000){char stamp[32];tm utc;gmtime_r(&lamp.fwChecked,&utc);strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",&utc);fw["checked_at"]=stamp;}
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
  if(action=="firmware_check_all"){for(int i=0;i<count();i++)checkLampFirmware(i,true);ok();return;}
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
  if(action=="firmware_install"){fail("Installation gesperrt: keine verifizierte Firmware-Signatur oder lokale Versionskontrolle");return;}
  if(action=="firmware_check"){
    if(serial.startsWith("group:")||targetCount!=1){fail("Firmwareprüfung nur für einzelne Lampen");return;}
    for(int i=0;i<count();i++)if(targets[i]){checkLampFirmware(i);return;}
  }
  if(action=="indicator"){
    if(!msg["enabled"].is<bool>()){fail("Tastenbeleuchtung erwartet Ein oder Aus");return;}
    for(int i=0;i<count();i++)if(targets[i]){
      String fields[32],reply;int n;
      if(!lampState(i,fields,n)){fail("Lampenidentität für Tastenbeleuchtung nicht bestätigt");return;}
      bool enabled=msg["enabled"].as<bool>();
      if(!fetch(i,enabled?"mculed=on":"mculed=off",reply)){logEvent(i,"Tastenbeleuchtung fehlgeschlagen");fail(lamps[i].error);return;}
      lamps[i].indicatorLight=enabled?1:0;logEvent(i,enabled?"Tastenbeleuchtung Ein gesendet":"Tastenbeleuchtung Aus gesendet");
    }ok();return;
  }
  if(action=="physical_resume"){
    if(!msg["enabled"].is<bool>()){fail("Profilfortsetzung erwartet Ein oder Aus");return;}
    for(int i=0;i<count();i++)if(targets[i]){
      String fields[32],reply;int n;
      if(!lampState(i,fields,n)){fail("Lampenidentität für Tastenverhalten nicht bestätigt");return;}
      bool enabled=msg["enabled"].as<bool>();
      if(!fetch(i,enabled?"turnoffset=1":"turnoffset=0",reply)){logEvent(i,"Tastenverhalten fehlgeschlagen");fail(lamps[i].error);return;}
      lamps[i].physicalResume=enabled?1:0;logEvent(i,enabled?"Profil nach physischem Ausschalten fortsetzen":"Nach physischem Ausschalten aus bleiben");
    }ok();return;
  }
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
void portal(){server.sendHeader("Location","http://192.168.8.1/",true);server.send(302,"text/plain","");}
void setup() {
  Serial.begin(115200);prefs.begin("reefbridge",false);fsReady=LittleFS.begin(false);
  // Initialize only on first use; a later mount failure must retain the data.
  if(!fsReady&&!prefs.getBool("studio_fs",false)){Serial.println("Initializing Studio filesystem");fsReady=LittleFS.begin(true);}
  if(fsReady)prefs.putBool("studio_fs",true);
  loadWorkspace();apPassword=prefs.getString("ap_pass","ReefBridge2026!");apSsid="ReefBridge-"+String((uint32_t)ESP.getEfuseMac(),HEX);
  WiFi.mode(WIFI_AP_STA);startSetupAp(false);
  WiFi.begin(prefs.getString("ssid","").c_str(),prefs.getString("wifi_pass","").c_str());configTime(0,0,"pool.ntp.org","time.cloudflare.com");
  Serial.println("Setup: http://192.168.8.1 | AP password: "+apPassword);
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",(const char*)WEB_UI,sizeof(WEB_UI));});
  for(const char* probe:{"/generate_204","/hotspot-detect.html","/connecttest.txt"})server.on(probe,HTTP_GET,portal);
  server.on("/api/status",HTTP_GET,status);
  server.on("/api/provision/enable",HTTP_POST,[]{
    startSetupAp(true);DynamicJsonDocument result(384);result["ok"]=true;result["ssid"]=apSsid;result["password"]=apPassword;result["url"]="http://192.168.8.1";
    String json;serializeJson(result,json);server.send(200,"application/json",json);
  });
  server.on("/api/provision/scan",HTTP_GET,[]{
    int found=WiFi.scanNetworks(false,true);DynamicJsonDocument result(4096);JsonArray networks=result.createNestedArray("networks");
    for(int i=0;i<found&&networks.size()<24;i++){String ssid=WiFi.SSID(i);if(!ssid.length()||ssid==apSsid)continue;JsonObject network=networks.createNestedObject();network["ssid"]=ssid;network["rssi"]=WiFi.RSSI(i);network["open"]=WiFi.encryptionType(i)==WIFI_AUTH_OPEN;}
    WiFi.scanDelete();String json;serializeJson(result,json);server.send(200,"application/json",json);
  });
  server.on("/api/provision/start",HTTP_POST,[]{DynamicJsonDocument msg(2048);if(body(msg))provisionLamp(msg);});
  server.on("/api/provision/disable",HTTP_POST,[]{ok();provisioningApStop=millis()+1500;});
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
  if(provisioningAp&&provisioningApStop&&expired(provisioningApStop)){dnsServer.stop();WiFi.softAPdisconnect(true);provisioningAp=false;provisioningApStop=0;}
  if(Serial.available()){String command=Serial.readStringUntil('\n');command.trim();if(command=="INFO")Serial.println("AP password: "+apPassword+"\nHome IP: "+WiFi.localIP().toString()+"\nSetup IP: 192.168.8.1");}
  if(WiFi.status()==WL_CONNECTED){static bool started=false;if(!started){MDNS.begin("reefbridge");if(!provisioningAp){dnsServer.stop();WiFi.softAPdisconnect(true);}started=true;}
    // Round-robin: one lamp transaction per loop keeps HTTP/DNS responsive
    // between fixtures; each fixture has independent mode, timers and clock.
    static int cursor=0;for(int n=0;n<count();n++){int i=(cursor+n)%count();expire(i);if(lamps[i].force||millis()-lamps[i].lastOutput>=30000){lamps[i].force=false;lamps[i].lastOutput=millis();output(i);cursor=(i+1)%count();break;}}
    if(!lastUpdateCheck||millis()-lastUpdateCheck>=6UL*60*60*1000)checkUpdate();
  }else if(millis()-lastReconnect>30000){lastReconnect=millis();WiFi.reconnect();}
  delay(2);
}
