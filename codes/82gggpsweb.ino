#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

const char* ssid = "tushar";
const char* password = "tushar21";

TinyGPSPlus gps;

// GPS TX -> GPIO4
// GPS RX -> GPIO5
SoftwareSerial gpsSerial(4, 5);

ESP8266WebServer server(80);

String htmlPage() {

  String lat = gps.location.isValid() ? String(gps.location.lat(), 6) : "Waiting...";
  String lon = gps.location.isValid() ? String(gps.location.lng(), 6) : "Waiting...";
  String alt = gps.altitude.isValid() ? String(gps.altitude.meters()) : "0";
  String sat = gps.satellites.isValid() ? String(gps.satellites.value()) : "0";

  String html =
  "<html><head>"
  "<meta http-equiv='refresh' content='2'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<style>"
  "body{background:#0a0f1f;color:white;font-family:Arial;text-align:center;}"
  ".card{background:#151c35;padding:15px;margin:10px;border-radius:15px;box-shadow:0 0 10px cyan;}"
  ".value{font-size:24px;color:cyan;}"
  "</style></head><body>";

  html += "<h1>GPS Dashboard</h1>";

  html += "<div class='card'>Latitude<div class='value'>" + lat + "</div></div>";
  html += "<div class='card'>Longitude<div class='value'>" + lon + "</div></div>";
  html += "<div class='card'>Altitude<div class='value'>" + alt + " m</div></div>";
  html += "<div class='card'>Satellites<div class='value'>" + sat + "</div></div>";

  if (gps.location.isValid()) {
    html += "<a href='https://maps.google.com/?q=" + lat + "," + lon + "'>";
    html += "<button style='padding:15px;'>Open Google Maps</button></a>";
  }

  html += "</body></html>";

  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void setup() {

  Serial.begin(115200);
  gpsSerial.begin(9600);

  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
}

void loop() {

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  server.handleClient();
}