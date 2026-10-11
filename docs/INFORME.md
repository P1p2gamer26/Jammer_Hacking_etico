# Informe: por qué el nRF24 no afecta a los audífonos Bluetooth

<p align="center">
  <img src="https://img.shields.io/badge/banda-2.4%20GHz-22d3ee" alt="2.4 GHz">
  <img src="https://img.shields.io/badge/nRF24L01%2B-3%20canales-orange" alt="3 canales">
  <img src="https://img.shields.io/badge/Bluetooth%20Classic-79%20canales%20%2F%20AFH-blue" alt="79 canales">
</p>

## Conclusión

No es una falla del firmware ni del panel. El jam del nRF24 solo cubre **3 canales** de
radio, mientras que Bluetooth Classic (el que usan casi todos los audífonos para el audio)
**salta entre 79 canales** y esquiva esos 3. El audio sigue sonando porque el protocolo está
diseñado justamente para sobrevivir a interferencia en pocos canales.

## 1. Qué emite el nRF24

- Banda 2.4&nbsp;GHz ISM, canales de **1&nbsp;MHz**: frecuencia = `2400 + canal` MHz.
- El firmware transmite portadora fija en **canales 2 / 26 / 80** = **2402 / 2426 / 2480&nbsp;MHz**.
- Esos tres son exactamente los **canales de *advertising* de BLE** (37, 38, 39).
- Potencia útil del módulo PA/LNA: **+20&nbsp;dBm = 100&nbsp;mW**, alcance ~5–10&nbsp;m en un cuarto.

## 2. Cómo se comunican los audífonos

- La mayoría usa **Bluetooth Classic (BR/EDR)** con **A2DP** para el audio (códec
  SBC/AAC/aptX/LDAC).
- Classic usa **79 canales** (2402–2480&nbsp;MHz) y hace **Adaptive Frequency Hopping (AFH)**:
  - **1600 saltos por segundo** (un slot cada 625&nbsp;µs).
  - Si un canal está *ocupado/ruidoso*, lo marca como **"malo"** y usa los otros **~76**.
- Los modelos nuevos con **LE Audio (LC3)** usan BLE, pero también hacen *hopping* adaptativo.

## 3. La cuenta

| Parámetro | Valor |
|-----------|-------|
| Canales que tapa el nRF24 | **3** |
| Canales que usa Bluetooth Classic | **79** |
| Cobertura | **3 / 79 ≈ 3.8 %** |

AFH detecta esos 3 canales, los descarta y sigue por los demás. Resultado: algún paquete se
reintenta y el audio **se mantiene**, sin cortes perceptibles.

## 4. Por qué "antes parecía funcionar"

- Lo que se interrumpía era el **descubrimiento/anuncio BLE** (aparecer o emparejar), que sí
  vive en los canales 2402/2426/2480.
- Una **conexión de audio ya establecida** (Classic/A2DP) no depende de esos canales → por eso
  ahora "no le hace nada".

## 5. El modo de emisión

El nRF24 con `startConstCarrier` emite una **portadora continua fija**; no puede saltar rápido
como Bluetooth. Sin salto, su efecto se limita a la frecuencia exacta donde está parado →
cobertura mínima frente a un sistema que salta 1600 veces por segundo.

## 6. Marco legal

> [!WARNING]
> Interferir deliberadamente comunicaciones es **interferencia perjudicial** y está prohibido
> (Colombia: CRC/ANE; EE.UU.: FCC, sin excepción por baja potencia o corto alcance). El
> alcance corto **no** lo vuelve legal.

## 7. Cómo medir el efecto (legal)

- **Monitor pasivo 2.4&nbsp;GHz**: el nRF24 recorre los canales y mide ocupación/RSSI; el panel
  lo grafica. Se ve el "ruido" y la afectación **observando**, sin atacar dispositivos.
- Contraste con `tools/ble_scan.py` (RSSI vía `bleak`) para comparar lo que se ve con y sin
  portadora.

## 8. Monitor pasivo (modo `MON`) y su limitación

El firmware incorpora un **barrido pasivo** de los 126 canales (2400–2525 MHz) que mide
energía con el bit **RPD** del nRF24 (umbral ≈ −64 dBm) y el panel lo dibuja como espectro.

Configuración correcta (según datasheet y el *scanner* oficial de RF24):

- Re-entrar en RX por canal: `stopListening()` → `setChannel()` → `startListening()` →
  esperar ≥170&nbsp;µs → `testRPD()`. El RPD es un *latch* por periodo de escucha; si no se
  re-entra en RX, se lee un valor obsoleto.
- `setDataRate(RF24_1MBPS)` (a 250&nbsp;kbps el detector suele no disparar), `disableCRC()`,
  `setAutoAck(false)`, `setAddressWidth(2)` y abrir 6 *pipes* con direcciones de ruido.
- Acumular varias pasadas: el RPD es **1 bit** y su umbral varía ±5&nbsp;dB con la temperatura.

> [!NOTE]
> **Limitación observada.** En el módulo de prueba el barrido devolvió **0/126 canales
> ocupados** de forma consistente. No significa que el aire esté limpio: es el comportamiento
> típico de los **clones**, muy sensibles al ruido de la fuente, que a menudo reportan
> `RPD = 0` de forma permanente. Mitigación: capacitor de **10–47&nbsp;µF** de baja ESR junto a
> VCC/GND, alimentación **lineal** (no conmutada), cables cortos y bajar la potencia del PA.
