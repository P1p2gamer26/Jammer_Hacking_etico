#include <SPI.h>
#include "RF24.h"
#include <esp_bt.h>
#include <esp_wifi.h>

#define PIN_BOTON 33
#define PIN_LED   2

#define CH_SWEEP_LOW  2
#define CH_SWEEP_HIGH 80
#define PLL_DWELL_US  150

SPIClass *spiBus = nullptr;

RF24 radio(22, 21, 16000000);

bool jamming = false;
bool botonPrevio = HIGH;
byte hopIdx = 0;

void configurarRadio() {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setAddressWidth(3);
  radio.startConstCarrier(RF24_PA_MAX, CH_SWEEP_LOW);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  spiBus = new SPIClass(VSPI);
  spiBus->begin();

  if (!radio.begin(spiBus)) {
    Serial.println("FALLO: el nRF24 no responde.");
    Serial.println("Revisar: alimentacion 3.3V externa y cableado SPI.");
    while (true) {}
  }

  radio.printPrettyDetails();
  Serial.println("Radio OK. Presiona el boton para iniciar.");

  byte prev = HIGH;
  while (true) {
    bool ahora = digitalRead(PIN_BOTON);
    if (ahora != prev) {
      delay(25);
      if (digitalRead(PIN_BOTON) != ahora) {
        jamming = !jamming;
        if (jamming) {
          configurarRadio();
          hopIdx = CH_SWEEP_LOW;
          digitalWrite(PIN_LED, HIGH);
          Serial.print("JAMMING ON  (barrido ");
          Serial.print(2400 + CH_SWEEP_LOW);
          Serial.print("-");
          Serial.print(2400 + CH_SWEEP_HIGH);
          Serial.println(" MHz)");
        } else {
          radio.powerDown();
          digitalWrite(PIN_LED, LOW);
          Serial.println("JAMMING OFF");
        }
      }
      prev = ahora;
    }
    if (jamming) {
      radio.setChannel(hopIdx);
      delayMicroseconds(PLL_DWELL_US);
      hopIdx++;
      if (hopIdx > CH_SWEEP_HIGH) hopIdx = CH_SWEEP_LOW;
    }
  }
}

void loop() {}
