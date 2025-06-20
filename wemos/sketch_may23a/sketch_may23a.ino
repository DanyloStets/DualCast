#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define SS    D8
#define RST   D0
#define DIO0  D1

const char* ssid = "TP-Link_57DC";
const char* password = "22510037";
const char* serverUrl = "http://192.168.0.108:5000/api/lora";

void setup() {
  Serial.begin(9600);
  delay(2000);
  while (!Serial);


  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi  IP: " + WiFi.localIP().toString());

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa!");
    while (1);
  }
  Serial.println("LoRa r");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String message = "";
    while (LoRa.available()) {
      message += (char)LoRa.read();
    }

    Serial.println(" LoRa: " + message);

    String mac = extractValue(message, "MAC:", " ");
    String rssi = extractValue(message, "RSSI:", " ");
    String dist = extractValue(message, "DIST:", " ");
    String temp = extractValue(message, "TEMP:", " ");
    String press = extractValue(message, "PRESS:", " ");
    String hum = extractValue(message, "HUM:", " ");
    String device_id = extractValue(message, "DEVICE_ID:", " ");

 
    String jsonData = "{";
    bool first = true;

    if (mac != "") {
      jsonData += "\"mac\":\"" + mac + "\"";
      first = false;
    }
    if (rssi != "") {
      if (!first) jsonData += ",";
      jsonData += "\"rssi\":" + rssi;
      first = false;
    }
    if (dist != "") {
      if (!first) jsonData += ",";
      jsonData += "\"dist\":\"" + dist + "\"";
      first = false;
    }
    if (temp != "") {
      if (!first) jsonData += ",";
      jsonData += "\"temp\":\"" + temp + "\"";
      first = false;
    }
    if (press != "") {
      if (!first) jsonData += ",";
      jsonData += "\"press\":\"" + press + "\"";
      first = false;
    }
    if (hum != "") {
      if (!first) jsonData += ",";
      jsonData += "\"hum\":\"" + hum + "\"";
      first = false;
    }
    if (device_id != ""){
      if (!first) jsonData += ",";
      jsonData += "\"device_id\":\"" + device_id + "\"";
      //first = false;
    }

  jsonData += "}";


    Serial.println("Відправка JSON: " + jsonData);

    
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      http.begin(client, serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonData);
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("HTTP POST failed: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    } else {
      Serial.println("Wi-Fi не підключено");
    }
  }
}

String extractValue(String data, String key, String delimiter) {
  int start = data.indexOf(key);
  if (start == -1) return "";
  start += key.length();
  int end = data.indexOf(delimiter, start);
  if (end == -1) end = data.length();
  return data.substring(start, end);
}