#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

const char* ssid = "tushar";
const char* password = "tushar21";

TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5); // D2 RX, D1 TX

ESP8266WebServer server(80);

double lat = 0;
double lon = 0;
double alt = 0;

// ---------------- JSON DATA ----------------
void handleData() {
  String json = "{";
  json += "\"lat\":" + String(lat, 6) + ",";
  json += "\"lon\":" + String(lon, 6) + ",";
  json += "\"alt\":" + String(alt, 1);
  json += "}";

  server.send(200, "application/json", json);
}

// ---------------- WEB PAGE ----------------
void handleRoot() {

  String page = "";

  page += "<!DOCTYPE html><html><head>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>GPS Tracker</title>";

  page += "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>";
  page += "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>";

  page += "<style>";
  pa  ge += "body{margin:0;font-family:Arial;background:#0b1220;color:white;text-align:center;}";