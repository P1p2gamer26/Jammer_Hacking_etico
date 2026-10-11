#include <SPI.h>
#include "RF24.h"
#include <esp_bt.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define PIN_BOTON 33
#define PIN_LED   2

SPIClass *spiBus = nullptr;
RF24 radio(22, 21, 16000000);

const byte BLE_CH[] = {2, 26, 80};
const byte N_BLE     = 3;

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
  radio.startConstCarrier(RF24_PA_MAX, BLE_CH[0]);
  hopIdx  = 0;
  jamming = true;
  digitalWrite(PIN_LED, HIGH);
  Serial.println("OK JAM ON (canales 2/26/80)");
  emitirEstado();
}

void stopJam() {
  radio.powerDown();
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
    radio.setChannel(BLE_CH[hopIdx]);
    hopIdx = (byte)((hopIdx + 1) % N_BLE);
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
}
