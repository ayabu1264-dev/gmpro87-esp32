#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#define MAX_BEACON 50

WebServer server(80);

// Beacon Spam Setting
int beacon_count = 6;
String beacon_ssid[MAX_BEACON];

struct ScanResult {
  String ssid;
  String bssid;
  int ch;
  int rssi;
};
ScanResult scanList[20];
int scanLen = 0;

bool beacon_mode = false;
bool evil_mode = false;
bool deauth_mode = false;
bool probe_mode = false;
bool jammer_mode = false;

// Variabel target untuk inject ke HTML
String target_ssid = "";
String target_bssid = "";
int target_ch = 0;

void scanWiFi() {
  // Scan secara synchronous agar stabil saat diminta
  scanLen = WiFi.scanNetworks(false, false); 
  if (scanLen > 20) scanLen = 20;
  for (int i = 0; i < scanLen; i++) {
    scanList[i].ssid = WiFi.SSID(i);
    scanList[i].bssid = WiFi.BSSIDstr(i);
    scanList[i].ch = WiFi.channel(i);
    scanList[i].rssi = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
}

String joinBeaconSSID() {
  String s = "";
  for(int i=0; i<beacon_count; i++) {
    if (beacon_ssid[i].length()) {
      if (s.length()) s += ",";
      s += beacon_ssid[i];
    }
  }
  return s;
}

void handleRoot() {
  // Cek jika ada aksi dari URL query (karena HTML lu pake /?scan=1 dll)
  if (server.hasArg("scan")) scanWiFi();
  if (server.hasArg("beacon")) beacon_mode = !beacon_mode;
  if (server.hasArg("evil")) evil_mode = !evil_mode;
  if (server.hasArg("deauth")) deauth_mode = !deauth_mode;
  if (server.hasArg("probe")) probe_mode = !probe_mode;
  if (server.hasArg("jam")) jammer_mode = !jammer_mode;
  
  // Cek jika ada AP yang dipilih
  if (server.hasArg("ap")) {
    target_bssid = server.arg("ap");
    for(int i=0; i<scanLen; i++) {
      if(scanList[i].bssid == target_bssid) {
        target_ssid = scanList[i].ssid;
        target_ch = scanList[i].ch;
      }
    }
  }

  File f = LittleFS.open("/index.html", "r");
  if (!f) { server.send(404, "text/plain", "index.html not found! Upload folder data dulu."); return; }
  String html = f.readString();
  f.close();

  // INJECT STATUS (Sesuai ID di HTML lu)
  html.replace("id=\"beacon-status\">Aktif</span>", (beacon_mode ? "id=\"beacon-status\" style='color:#20c20e'>Aktif</span>" : "id=\"beacon-status\" style='color:#ff4444'>Mati</span>"));
  html.replace("id=\"evil-status\">Mati</span>", (evil_mode ? "id=\"evil-status\" style='color:#20c20e'>Aktif</span>" : "id=\"evil-status\" style='color:#ff4444'>Mati</span>"));
  html.replace("id=\"deauth-status\">Mati</span>", (deauth_mode ? "id=\"deauth-status\" style='color:#20c20e'>Aktif</span>" : "id=\"deauth-status\" style='color:#ff4444'>Mati</span>"));
  html.replace("id=\"probe-status\">Mati</span>", (probe_mode ? "id=\"probe-status\" style='color:#20c20e'>Aktif</span>" : "id=\"probe-status\" style='color:#ff4444'>Mati</span>"));
  html.replace("id=\"jammer-status\">Mati</span>", (jammer_mode ? "id=\"jammer-status\" style='color:#20c20e'>Aktif</span>" : "id=\"jammer-status\" style='color:#ff4444'>Mati</span>"));

  // INJECT WARNA TOMBOL
  if(beacon_mode) html.replace("name=\"beacon\" class=\"toggle-btn off\"", "name=\"beacon\" class=\"toggle-btn on\"");
  if(evil_mode)   html.replace("name=\"evil\" class=\"toggle-btn off\"",   "name=\"evil\" class=\"toggle-btn on\"");
  if(deauth_mode) html.replace("name=\"deauth\" class=\"toggle-btn off\"", "name=\"deauth\" class=\"toggle-btn on\"");

  // INJECT TABEL WIFI
  String scanTable = "";
  for (int i = 0; i < scanLen; i++) {
    String ssidLabel = scanList[i].ssid.length() ? scanList[i].ssid : "<i style='color:#888;'>[hidden]</i>";
    int rssi = scanList[i].rssi;
    int bar = map(rssi, -100, -40, 1, 5);
    String rssiBar = "";
    for(int j=0; j<bar; j++) rssiBar += "&#9608;";

    scanTable += "<tr><td>" + ssidLabel + "</td><td>" + scanList[i].bssid + "</td><td>" + String(scanList[i].ch) + "</td>";
    scanTable += "<td><span style='color:" + String(rssi > -70 ? "#20c20e" : "#df4237") + ";'>" + String(rssi) + " dBm</span> " + rssiBar + "</td>";
    scanTable += "<td><form method='post' action='/?ap=" + scanList[i].bssid + "'><button class='select-btn'>Select</button></form></td></tr>";
  }
  html.replace("", scanTable);

  // INJECT TARGET INFO
  html.replace("", target_ssid);
  html.replace("", target_bssid);
  html.replace("", target_ch > 0 ? String(target_ch) : "");

  server.send(200, "text/html", html);
}

void handleBeaconSetting() {
  if (server.hasArg("beacon_count")) beacon_count = server.arg("beacon_count").toInt();
  beacon_count = constrain(beacon_count, 1, MAX_BEACON);
  for (int i = 0; i < beacon_count; i++) {
    String ssidName = "ssid_" + String(i+1);
    if (server.hasArg(ssidName)) beacon_ssid[i] = server.arg(ssidName);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. Kunci mode WiFi agar tidak gampang hilang
  WiFi.mode(WIFI_AP_STA);
  
  // 2. Set SoftAP dengan channel tetap (Channel 1 sering paling stabil untuk awal)
  // Gunakan password minimal 8 karakter atau kosongkan sekalian
  WiFi.softAP("GMpro87", "gmpro87admin", 1, 0, 4);
  
  // 3. Inisialisasi File System
  if(!LittleFS.begin(true)) Serial.println("LittleFS Error");

  // 4. Routes (Handle semua query di root)
  server.on("/", HTTP_ANY, handleRoot); 
  server.on("/set_beacon", HTTP_POST, handleBeaconSetting);
  server.on("/upload_html", HTTP_POST, [](){ server.send(200); }, [](){
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      String fname = "/" + upload.filename;
      File f = LittleFS.open(fname, "w");
      f.close();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      File f = LittleFS.open("/" + upload.filename, "a");
      if (f) f.write(upload.buf, upload.currentSize);
      f.close();
    }
  });

  server.begin();
  Serial.println("System Ready. Connect to GMpro87");
}

void loop() {
  server.handleClient();
  // Logika serangan (Beacon, Deauth, dll) taruh di sini
}
