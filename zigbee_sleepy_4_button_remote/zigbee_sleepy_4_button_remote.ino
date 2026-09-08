#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools → Zigbee mode"
#endif

#include "Zigbee.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include <Wire.h>

#define BTN1_PIN 0
#define BTN2_PIN 1
#define BTN3_PIN 2
#define BTN4_PIN 4
#define ADXL_INT_PIN 5

#define SDA_PIN 22
#define SCL_PIN 23

#define IDLE_TIMEOUT_MS  15000
#define DEBOUNCE_MS      40
#define LONG_PRESS_MS    600
#define DOUBLE_PRESS_MS  400
#define DIM_INTERVAL_MS  250

#define WAKE_MASK ((1ULL << BTN1_PIN) | (1ULL << BTN2_PIN) | \
                   (1ULL << BTN3_PIN) | (1ULL << BTN4_PIN) | \
                   (1ULL << ADXL_INT_PIN))

uint8_t adxlAddr = 0x53;

ZigbeeColorDimmerSwitch zb1(1);
ZigbeeColorDimmerSwitch zb2(2);
ZigbeeColorDimmerSwitch zb3(3);
ZigbeeColorDimmerSwitch zb4(4);
ZigbeeColorDimmerSwitch zb5(5);   // motion / tap only

struct Button {
  uint8_t pin;
  bool lastState;
  uint32_t pressStart;
  bool longPressSent;
  bool dimDirection;
  uint32_t lastDimTime;
  uint32_t lastReleaseTime;
  uint8_t clickCount;
};

Button btn1 = {BTN1_PIN, HIGH, 0, false, true, 0, 0, 0};
Button btn2 = {BTN2_PIN, HIGH, 0, false, true, 0, 0, 0};
Button btn3 = {BTN3_PIN, HIGH, 0, false, true, 0, 0, 0};
Button btn4 = {BTN4_PIN, HIGH, 0, false, true, 0, 0, 0};

uint32_t lastActivity = 0;
uint32_t lastMotionAction = 0;
bool adxlOk = false;
bool zigbeeReady = false;

void markActivity() {
  lastActivity = millis();
}

bool adxlWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(adxlAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

uint8_t adxlRead(uint8_t reg) {
  Wire.beginTransmission(adxlAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom(adxlAddr, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0;
}

bool probeAddr(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool initAdxl() {
  Wire.end();
  delay(10);
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(20);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(50);

  adxlOk = false;
  for (int i = 0; i < 8; i++) {
    if (probeAddr(0x53)) { adxlAddr = 0x53; adxlOk = true; break; }
    if (probeAddr(0x1D)) { adxlAddr = 0x1D; adxlOk = true; break; }
    delay(80);
  }

  if (!adxlOk) {
    Serial.println("ADXL345 not found");
    return false;
  }

  uint8_t devid = adxlRead(0x00);
  Serial.printf("ADXL345 at 0x%02X DEVID=0x%02X\n", adxlAddr, devid);

  adxlWrite(0x2D, 0x00);
  adxlWrite(0x2C, 0x0A);
  adxlWrite(0x31, 0x2B);
  adxlWrite(0x1D, 40);
  adxlWrite(0x21, 48);
  adxlWrite(0x22, 40);
  adxlWrite(0x23, 80);
  adxlWrite(0x2A, 0x07);
  adxlWrite(0x24, 12);
  adxlWrite(0x27, 0xF0);
  adxlWrite(0x2F, 0x00);
  adxlWrite(0x2E, 0x50);
  adxlRead(0x30);
  adxlWrite(0x2D, 0x08);
  Serial.println("ADXL345 ready");
  return true;
}

void handleButton(Button &btn, ZigbeeColorDimmerSwitch &sw, const char *name) {
  bool current = digitalRead(btn.pin);
  uint32_t now = millis();

  if (current != btn.lastState) {
    delay(DEBOUNCE_MS);
    current = digitalRead(btn.pin);

    if (current == LOW) {
      btn.pressStart = now;
      btn.longPressSent = false;
      markActivity();
      Serial.printf("%s pressed\n", name);
    } else {
      uint32_t duration = now - btn.pressStart;
      if (!btn.longPressSent && duration > 50 && duration < LONG_PRESS_MS) {
        if (now - btn.lastReleaseTime < DOUBLE_PRESS_MS) {
          btn.clickCount = 0;
          if (zigbeeReady) sw.lightOn();
          Serial.printf("%s DOUBLE → lightOn\n", name);
        } else {
          btn.clickCount = 1;
          btn.lastReleaseTime = now;
        }
      }
      markActivity();
    }
    btn.lastState = current;
  }

  if (btn.clickCount == 1 && (now - btn.lastReleaseTime > DOUBLE_PRESS_MS)) {
    btn.clickCount = 0;
    if (zigbeeReady) sw.lightToggle();
    Serial.printf("%s SINGLE → toggle\n", name);
    markActivity();
  }

  if (current == LOW && !btn.longPressSent) {
    if (now - btn.pressStart > LONG_PRESS_MS) {
      btn.longPressSent = true;
      btn.clickCount = 0;
      btn.dimDirection = !btn.dimDirection;
      Serial.printf("%s hold → dim %s\n", name, btn.dimDirection ? "UP" : "DOWN");
    }
  }

  if (current == LOW && btn.longPressSent) {
    if (now - btn.lastDimTime > DIM_INTERVAL_MS) {
      btn.lastDimTime = now;
      markActivity();
      if (zigbeeReady) {
        if (btn.dimDirection) sw.setLightLevelStep(ZIGBEE_LEVEL_STEP_UP, 10, 2);
        else sw.setLightLevelStep(ZIGBEE_LEVEL_STEP_DOWN, 10, 2);
      }
    }
  }
}

void handleAdxl() {
  if (!adxlOk) return;

  uint8_t src = adxlRead(0x30);
  if (src == 0) return;
  if (millis() - lastMotionAction < 400) return;

  if ((src & 0x40) || (src & 0x10)) {
    Serial.printf("Motion → endpoint 5, INT_SOURCE=0x%02X\n", src);
    if (zigbeeReady) zb5.lightToggle();
    lastMotionAction = millis();
    markActivity();
  }
}

void pollInputs() {
  handleButton(btn1, zb1, "B1");
  handleButton(btn2, zb2, "B2");
  handleButton(btn3, zb3, "B3");
  handleButton(btn4, zb4, "B4");
  handleAdxl();
}

void goToDeepSleep() {
  Serial.println("Idle → preparing deep sleep");

  if (adxlOk) adxlRead(0x30);

  uint32_t start = millis();
  while (digitalRead(BTN1_PIN) == LOW || digitalRead(BTN2_PIN) == LOW ||
         digitalRead(BTN3_PIN) == LOW || digitalRead(BTN4_PIN) == LOW ||
         digitalRead(ADXL_INT_PIN) == LOW) {
    if (adxlOk) adxlRead(0x30);
    if (millis() - start > 3000) {
      Serial.println("Pins still LOW after 3s – sleeping anyway");
      break;
    }
    delay(20);
  }
  delay(150);
  if (adxlOk) adxlRead(0x30);

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  const uint8_t pins[] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, ADXL_INT_PIN};
  for (uint8_t i = 0; i < 5; i++) {
    gpio_sleep_set_direction((gpio_num_t)pins[i], GPIO_MODE_INPUT);
    gpio_sleep_set_pull_mode((gpio_num_t)pins[i], GPIO_PULLUP_ONLY);
    gpio_hold_en((gpio_num_t)pins[i]);
  }

  esp_deep_sleep_enable_gpio_wakeup(WAKE_MASK, ESP_GPIO_WAKEUP_GPIO_LOW);

  Serial.println("Entering deep sleep...");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(400);

  gpio_hold_dis((gpio_num_t)BTN1_PIN);
  gpio_hold_dis((gpio_num_t)BTN2_PIN);
  gpio_hold_dis((gpio_num_t)BTN3_PIN);
  gpio_hold_dis((gpio_num_t)BTN4_PIN);
  gpio_hold_dis((gpio_num_t)ADXL_INT_PIN);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(ADXL_INT_PIN, INPUT_PULLUP);

  Serial.println("=== Boot / Wake ===");
  Serial.printf("Wake cause: %d\n", (int)esp_sleep_get_wakeup_cause());
  Serial.printf("Pins B1=%d B2=%d B3=%d B4=%d INT=%d\n",
                digitalRead(BTN1_PIN), digitalRead(BTN2_PIN),
                digitalRead(BTN3_PIN), digitalRead(BTN4_PIN),
                digitalRead(ADXL_INT_PIN));

  initAdxl();

  zb1.setManufacturerAndModel("DIY", "FourButton");
  zb2.setManufacturerAndModel("DIY", "FourButton");
  zb3.setManufacturerAndModel("DIY", "FourButton");
  zb4.setManufacturerAndModel("DIY", "FourButton");
  zb5.setManufacturerAndModel("DIY", "Motion");

  Zigbee.addEndpoint(&zb1);
  Zigbee.addEndpoint(&zb2);
  Zigbee.addEndpoint(&zb3);
  Zigbee.addEndpoint(&zb4);
  Zigbee.addEndpoint(&zb5);

  if (!Zigbee.begin()) {
    Serial.println("Zigbee begin failed");
    delay(1000);
    ESP.restart();
  }

  Serial.println("Connecting (buttons still live)...");
  uint32_t start = millis();
  while (!Zigbee.connected() && millis() - start < 20000) {
    pollInputs();
    delay(10);
  }

  zigbeeReady = Zigbee.connected();
  Serial.println(zigbeeReady ? "Zigbee connected" : "Zigbee not connected");

  uint64_t wakePins = esp_sleep_get_gpio_wakeup_status();
  if (adxlOk && (wakePins & (1ULL << ADXL_INT_PIN))) {
    uint8_t src = adxlRead(0x30);
    Serial.printf("Woke from ADXL → endpoint 5, INT_SOURCE=0x%02X\n", src);
    if (zigbeeReady) zb5.lightToggle();
    lastMotionAction = millis();
  }

  markActivity();
}

void loop() {
  if (!zigbeeReady && Zigbee.connected()) {
    zigbeeReady = true;
    Serial.println("Zigbee connected");
  }

  pollInputs();

  if (millis() - lastActivity > IDLE_TIMEOUT_MS) {
    goToDeepSleep();
  }

  delay(10);
}
