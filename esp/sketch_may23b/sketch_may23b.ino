#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <VL53L0X.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ---------------- LoRa Pins ----------------
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  2

// ---------------- I2C ----------------
TwoWire vl53_i2c = TwoWire(1);  // альтернативна I2C для VL53L0X
VL53L0X vl53;
Adafruit_BME280 bme;

// ---------------- Кнопки ----------------
#define BTN_WIFI_ANALYZER 26
#define BTN_SEND_SENSORS  25

bool wifiAnalyzerEnabled = false;

// ---------------- Структура для callback Wi-Fi ----------------
typedef struct {
  uint8_t mac[6];
  int8_t rssi;
} PacketInfo;


bool lastStateBtnWifi = HIGH;
bool lastStateBtnSend = HIGH;
unsigned long lastDebounceTimeWifi = 0;
unsigned long lastDebounceTimeSend = 0;
const unsigned long debounceDelay = 50;


// ---------------- Глобальні змінні ----------------
int lastStableBtnWifi = HIGH;
int lastReadingBtnWifi = HIGH;
unsigned long lastChangeTimeWifi = 0;

int lastStableBtnSend = HIGH;
int lastReadingBtnSend = HIGH;
unsigned long lastChangeTimeSend = 0;

String deviceId = "A1";


struct wifi_ieee80211_hdr {
  uint8_t frame_ctrl[2];
  uint8_t duration_id[2];
  uint8_t addr1[6];
  uint8_t addr2[6];  // MAC-адреса джерела
  uint8_t addr3[6];
  uint8_t seq_ctrl[2];
};

// Повний пакет Wi-Fi
struct wifi_ieee80211_packet_t {
  struct wifi_ieee80211_hdr hdr;
  uint8_t payload[0];
};

void wifi_sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
  Serial.println("callback");
  if (!wifiAnalyzerEnabled) return;

  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const wifi_ieee80211_packet_t* ipkt = (wifi_ieee80211_packet_t*)pkt->payload;

  PacketInfo info;
  memcpy(info.mac, ipkt->hdr.addr2, 6);
  info.rssi = pkt->rx_ctrl.rssi;

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          info.mac[0], info.mac[1], info.mac[2],
          info.mac[3], info.mac[4], info.mac[5]);

  String message = "MAC:" + String(macStr) + " RSSI:" + String(info.rssi) + " DEVICE_ID:" + deviceId;

  Serial.println("WiFi Аналізатор → LoRa: " + message);
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  // ---------------- Кнопки ----------------
  pinMode(BTN_WIFI_ANALYZER, INPUT_PULLUP);
  pinMode(BTN_SEND_SENSORS, INPUT_PULLUP);

  // ---------------- LoRa ----------------
  SPI.begin(18, 19, 23, LORA_SS);  // SCK, MISO, MOSI, SS
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa не ініціалізовано!");
    while (1);
  }
  Serial.println("LoRa готова");

  // ---------------- BME280 ----------------
  Wire.begin(21, 22);
  if (!bme.begin(0x76)) {
    Serial.println("Не знайдено BME280!");
    while (1);
  }

  // ---------------- VL53L0X ----------------
  vl53_i2c.begin(17, 16);
  vl53.setBus(&vl53_i2c);
  if (!vl53.init()) {
    Serial.println("Не знайдено VL53L0X!");
    while (1);
  }
  vl53.setTimeout(500);
  vl53.startContinuous();

  // ---------------- WiFi Sniffer ----------------
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_callback);

  wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
  };
  esp_wifi_set_promiscuous_ctrl_filter(&filter);

  Serial.println("Все готово!");
}


void loop() {
  // ----------- Wi-Fi аналізатор -------------
  int currentReadingWifi = digitalRead(BTN_WIFI_ANALYZER);
  if (currentReadingWifi != lastReadingBtnWifi) {
    lastChangeTimeWifi = millis();  // запам'ятай час зміни
  }

    if ((millis() - lastChangeTimeWifi) > debounceDelay) {
    if (currentReadingWifi != lastStableBtnWifi) {
      lastStableBtnWifi = currentReadingWifi;
      if (currentReadingWifi == LOW) {
        wifiAnalyzerEnabled = !wifiAnalyzerEnabled;
        if (wifiAnalyzerEnabled) {
          Serial.println("Wi-Fi аналізатор УВІМКНЕНИЙ");
          esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);  // Краще 6 канал
          esp_wifi_set_promiscuous(true);
        } else {
          Serial.println("Wi-Fi аналізатор ВИМКНЕНИЙ");
          esp_wifi_set_promiscuous(false);
        }
      }
    }
  }

  lastReadingBtnWifi = currentReadingWifi;
// ----------- Надсилання сенсорів -------------
  int currentReadingSend = digitalRead(BTN_SEND_SENSORS);
  if (currentReadingSend != lastReadingBtnSend) {
    lastChangeTimeSend = millis();
  }

  if ((millis() - lastChangeTimeSend) > debounceDelay) {
    if (currentReadingSend != lastStableBtnSend) {
      lastStableBtnSend = currentReadingSend;
      if (currentReadingSend == LOW) {
        uint16_t distance = vl53.readRangeContinuousMillimeters();
        float temperature = bme.readTemperature();
        float pressure = bme.readPressure() / 100.0F;
        float humidity = bme.readHumidity();

        String message = "DIST:" + String(distance) + "mm, ";
        message += "TEMP:" + String(temperature, 1) + "C, ";
        message += "PRESS:" + String(pressure, 1) + "hPa, ";
        message += "HUM:" + String(humidity, 1) + "%, ";
        message += "DEVICE_ID:" + deviceId;

        Serial.println("Надіслано по LoRa: " + message);
        LoRa.beginPacket();
        LoRa.print(message);
        LoRa.endPacket();
      }
    }
  }
  lastReadingBtnSend = currentReadingSend;
}

