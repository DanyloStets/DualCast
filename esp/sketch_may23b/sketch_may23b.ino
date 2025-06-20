#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <VL53L0X.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  2
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SCLK 18

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

TwoWire vl53_i2c = TwoWire(1);  
VL53L0X vl53;
Adafruit_BME280 bme;

#define BTN_WIFI_ANALYZER 26
#define BTN_SEND_SENSORS  25

bool wifiAnalyzerEnabled = false;

typedef struct {
  uint8_t mac[6];
  int8_t rssi;
} PacketInfo;


bool lastStateBtnWifi = HIGH;
bool lastStateBtnSend = HIGH;
uint32_t lastDebounceTimeWifi = 0;
uint32_t lastDebounceTimeSend = 0;
const uint32_t  debounceDelay = 50;


int32_t lastStableBtnWifi = HIGH;
int32_t lastReadingBtnWifi = HIGH;
uint32_t lastChangeTimeWifi = 0;

int32_t lastStableBtnSend = HIGH;
int32_t lastReadingBtnSend = HIGH;
uint32_t lastChangeTimeSend = 0;

String deviceId = "A1";


struct wifi_ieee80211_hdr {
  uint8_t frame_ctrl[2];
  uint8_t duration_id[2];
  uint8_t addr1[6];
  uint8_t addr2[6]; 
  uint8_t addr3[6];
  uint8_t seq_ctrl[2];
};

//Wi-Fi
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

  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("MAC:");
  display.println(String(macStr));
  display.print("RSSI: ");
  display.println(String(info.rssi));
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(BTN_WIFI_ANALYZER, INPUT_PULLUP);
  pinMode(BTN_SEND_SENSORS, INPUT_PULLUP);


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED не знайдено!"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED");
  display.display();

  
  SPI.begin(LORA_SCLK, LORA_MISO, LORA_MOSI, LORA_SS);  // SCK, MISO, MOSI, SS
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa  not r");
    while (1);
  }
  Serial.println("LoRa r");


  Wire.begin(21, 22);
  if (!bme.begin(0x76)) {
    Serial.println(" BME280 not r");
    while (1);
  }


  vl53_i2c.begin(17, 16);
  vl53.setBus(&vl53_i2c);
  if (!vl53.init()) {
    Serial.println("VL53L0X not r");
    while (1);
  }
  vl53.setTimeout(500);
  vl53.startContinuous();


  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_callback);

  wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
  };
  esp_wifi_set_promiscuous_ctrl_filter(&filter);

  Serial.println("ready");
}


void loop() {

  int currentReadingWifi = digitalRead(BTN_WIFI_ANALYZER);
  if (currentReadingWifi != lastReadingBtnWifi) {
    lastChangeTimeWifi = millis(); 
  }
  if ((millis() - lastChangeTimeWifi) > debounceDelay) {
    if (currentReadingWifi != lastStableBtnWifi) {
      lastStableBtnWifi = currentReadingWifi;
      if (currentReadingWifi == LOW) {
        wifiAnalyzerEnabled = !wifiAnalyzerEnabled;
        if (wifiAnalyzerEnabled) {
          Serial.println("Wi-Fi analis on");
          esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE); 
          esp_wifi_set_promiscuous(true);
        } else {
          Serial.println("Wi-Fi analis off");
          esp_wifi_set_promiscuous(false);
        }
      }
    }
  }

  lastReadingBtnWifi = currentReadingWifi;
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