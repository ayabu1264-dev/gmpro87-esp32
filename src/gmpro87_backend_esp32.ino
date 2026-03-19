#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#define MAX_BEACON 50

WebServer server(80);

// Beacon Spam Setting
int beacon_count = 6;
String beacon_ssid[MAX_BEACON];

// WiFi scan result struct
struct ScanResult {
  String ssid;
  String bssid;
  int ch;
  int rssi;
};
ScanResult scanList[20];
int scanLen = 0;

// Mode State
bool beacon_mode = false;
bool evil_mode = false;
bool deauth_mode = false;
bool probe_mode = false;
bool jammer_mode = false;

// --- WiFi scan ---
void scanWiFi() {
  scanLen = WiFi.scanNetworks();
  for (int i = 0; i < scanLen && i < 20; i++) {
    scanList[i].ssid = WiFi.SSID(i);
    scanList[i].bssid = WiFi.BSSIDstr(i);
    scanList[i].ch = WiFi.channel(i);
    scanList[i].rssi = WiFi.RSSI(i);
  }
}

// --- LittleFS init & main page ---
void handleRoot() {
  File f = LittleFS.open("/index.html", "r");
  if (!f) { server.send(404, "text/html", "File Not Found"); return; }
  String html = f.readString();
  f.close();

  // Inject status bar
  html.replace("{BEACON_COUNT}", String(beacon_count));
  html.replace("{BEACON_SSID}", joinBeaconSSID());

  // Inject WiFi scan table
  String scanTable = "";
  for (int i = 0; i < scanLen; i++) {
    String ssidLabel = scanList[i].ssid.length()?scanList[i].ssid:"<i style='color:#888;'>[hidden]</i>";
    String rssiBar = "<span style='color:#F6C700;'>";
    int rssi = scanList[i].rssi;
    int bar = 1 + (rssi > -80) + (rssi > -70) + (rssi > -60) + (rssi > -50);
    for(int j=0;j<bar;j++) rssiBar += "&#9608;";
    rssiBar += "</span>";
    scanTable += "<tr>"
      "<td>" + ssidLabel + "</td>"
      "<td>" + scanList[i].bssid + "</td>"
      "<td>" + String(scanList[i].ch) + "</td>"
      "<td><span style='font-weight:bold;color:" + (rssi>-70?"#20c20e":"#df4237") + ";'>" + String(rssi) + " dBm</span> " + rssiBar + "</td>"
      "<td><form method='post' action='/?ap=" + scanList[i].bssid + "'><button class='select-btn'>Select</button></form></td></tr>";
  }
  html.replace("<!-- Contoh isi (backend inject di ESP32) -->", scanTable);

  server.send(200, "text/html", html);
}

// --- join SSID list with comma ---
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

// --- Mode toggle ---
void handleToggle() {
  if (server.hasArg("beacon")) beacon_mode = !beacon_mode;
  if (server.hasArg("evil")) evil_mode = !evil_mode;
  if (server.hasArg("deauth")) deauth_mode = !deauth_mode;
  if (server.hasArg("probe")) probe_mode = !probe_mode;
  if (server.hasArg("jam")) jammer_mode = !jammer_mode;
  handleRoot();
}

// --- Scan WiFi (manual scan) ---
void handleScan() {
  scanWiFi();
  handleRoot();
}

// --- Beacon Spam Setting ---
void handleBeaconSetting() {
  if (server.hasArg("beacon_count")) beacon_count = server.arg("beacon_count").toInt();
  beacon_count = constrain(beacon_count, 1, MAX_BEACON);
  for (int i = 0; i < beacon_count; i++) {
    String ssidName = "ssid_" + String(i+1);
    if (server.hasArg(ssidName)) beacon_ssid[i] = server.arg(ssidName);
    else beacon_ssid[i] = "";
  }
  handleRoot();
}

// --- Beacon Spam logic ---
// (In loop, kirim beacon spam sesuai beacon_ssid[] jika beacon_mode aktif)

// --- Upload HTML etwin file ---
void handleUploadHtml() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String fname = "/" + upload.filename;
    if (fname.endsWith(".html")) {
      File f = LittleFS.open(fname, "w");
      f.close();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String fname = "/" + upload.filename;
    File f = LittleFS.open(fname, "a");
    if (f) f.write(upload.buf, upload.currentSize);
    f.close();
  } else if (upload.status == UPLOAD_FILE_END) {
    // DONE
  }
  // redirect after upload
  server.sendHeader("Location", "/"); server.send(303);
}

// --- View HTML etwin file ---
void handleViewHtml() {
  String file = "/" + server.arg("file");
  if (LittleFS.exists(file)) {
    File f = LittleFS.open(file, "r");
    String c = f.readString();
    f.close();
    server.send(200, "text/html", c);
  } else {
    server.send(404, "text/html", "<html><body>File not found.</body></html>");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("GMpro87", "gmpro87admin");
  LittleFS.begin();
  scanWiFi();

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/?beacon=", HTTP_POST, handleToggle);
  server.on("/?evil=", HTTP_POST, handleToggle);
  server.on("/?deauth=", HTTP_POST, handleToggle);
  server.on("/?probe=", HTTP_POST, handleToggle);
  server.on("/?jam=", HTTP_POST, handleToggle);
  server.on("/?scan=1", HTTP_POST, handleScan); // manual scan
  server.on("/set_beacon", HTTP_POST, handleBeaconSetting);
  server.on("/upload_html", HTTP_POST, [](){ server.send(200, "text/html", "<html><body>Upload Starting...</body></html>"); }, handleUploadHtml);
  server.on("/view_html", HTTP_GET, handleViewHtml);

  server.begin();
}

void loop() {
  server.handleClient();

  // Beacon Spam logic (send beacon with beacon_ssid[])
  if (beacon_mode) {
    // Send beacon spam with all SSID in beacon_ssid[]
    // (Implement sesuai library beacon ESP32, contoh sebelumnya)
  }
}
