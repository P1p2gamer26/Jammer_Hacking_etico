#include <SPI.h>
#include "RF24.h"
#include <esp_bt.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define PIN_BOTON 33
#define PIN_LED   2

#define MON_MUESTRAS 6
#define MON_CANALES  126

SPIClass *spiBus = nullptr;
RF24 radio(22, 21, 16000000);

const byte BLE_CH[] = {2, 26, 80};
const byte N_BLE     = 3;

bool jamming    = false;
bool scanning   = false;
bool monitoring = false;
byte hopIdx     = 0;

BLEScan *pBLEScan = nullptr;

class ScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) {
    if (!scanning || jamming || monitoring) return;
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

void configurarMonitor() {
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MIN, true);
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setAddressWidth(2);
  const uint8_t noise[6][5] = {
    {0x55, 0x55, 0, 0, 0}, {0xAA, 0xAA, 0, 0, 0}, {0xA0, 0xAA, 0, 0, 0},
    {0xAB, 0xAA, 0, 0, 0}, {0xAC, 0xAA, 0, 0, 0}, {0xAD, 0xAA, 0, 0, 0},
  };
  for (byte p = 0; p < 6; p++) radio.openReadingPipe(p, noise[p]);
  radio.startListening();
}

void emitirEstado() {
  Serial.print("MODE ");
  if (jamming)          Serial.println("JAM");
  else if (scanning)    Serial.println("SCAN");
  else if (monitoring)  Serial.println("MON");
  else                  Serial.println("IDLE");
}

void startScan() {
  if (jamming || monitoring) return;
  scanning = true;
  Serial.println("OK SCAN ON");
  emitirEstado();
}

void stopScan() {
  if (!scanning) return;
  scanning = false;
  Serial.println("OK SCAN OFF");
  emitirEstado();
}

void startJam() {
  stopScan();
  stopMonitor();
  configurarRadio();
  radio.startConstCarrier(RF24_PA_MAX, BLE_CH[0]);
  hopIdx  = 0;
  jamming = true;
  digitalWrite(PIN_LED, HIGH);
  Serial.println("OK JAM ON (canales 2/26/80)");
  emitirEstado();
}

void stopJam() {
  if (!jamming) return;
  radio.powerDown();
  jamming = false;
  digitalWrite(PIN_LED, LOW);
  Serial.println("OK JAM OFF");
  emitirEstado();
}

void startMonitor() {
  if (jamming) return;
  scanning = false;
  if (pBLEScan) pBLEScan->stop();
  configurarMonitor();
  monitoring = true;
  Serial.println("OK MON ON (barrido pasivo 0-125)");
  emitirEstado();
}

void stopMonitor() {
  if (!monitoring) return;
  monitoring = false;
  Serial.println("OK MON OFF");
  emitirEstado();
}

void hacerSweep() {
  for (byte ch = 0; ch < MON_CANALES; ch++) {
    byte hits = 0;
    for (byte i = 0; i < MON_MUESTRAS; i++) {
      radio.stopListening();
      radio.setChannel(ch);
      radio.startListening();
      delayMicroseconds(180);
      if (radio.testRPD()) hits++;
    }
    radio.flush_rx();
    Serial.print("CH ");
    Serial.print(ch);
    Serial.print(" ");
    Serial.print(hits);
    Serial.print(" ");
    Serial.println(MON_MUESTRAS);
  }
  radio.stopListening();
  Serial.println("SWEEPEND");
}

void procesarComando(String cmd) {
  cmd.trim();
  String c = cmd;
  c.toUpperCase();
  if      (c == "J" || c == "JAM")     startJam();
  else if (c == "X" || c == "STOP")    { stopJam(); stopScan(); stopMonitor(); }
  else if (c == "SCAN")                startScan();
  else if (c == "NOSCAN" || c == "SCANOFF") stopScan();
  else if (c == "MON" || c == "MONITOR" || c == "SWEEP") startMonitor();
  else if (c == "S" || c == "STATUS")  emitirEstado();
  else if (c == "PING")                Serial.println("PONG");
  else if (c == "H" || c == "HELP")    Serial.println("CMDS: SCAN | NOSCAN | JAM | STOP | MON | STATUS | PING");
  else if (c.length())                 { Serial.print("ERR desconocido: "); Serial.println(cmd); }
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

  if (monitoring && Serial.availableForWrite() > 64) {
    hacerSweep();
  }

  if (jamming) {
    radio.setChannel(BLE_CH[hopIdx]);
    hopIdx = (byte)((hopIdx + 1) % N_BLE);
  }

  if (scanning && !jamming && !monitoring) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 1500) {
      pBLEScan->start(1, false);
      pBLEScan->clearResults();
      Serial.println("SCANEND");
      lastScan = millis();
    }
  }
}
