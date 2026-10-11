# Brief: Dashboard web para RF Console (ESP32 + nRF24)

## Qué es
Panel web local para una demo de clase de hacking ético. Muestra dispositivos BLE
detectados por un ESP32 (con RSSI real) y permite controlar un jammer 2.4 GHz basado
en nRF24. Es una herramienta educativa sobre equipos propios/autorizados.

## Entregable
UN solo archivo: `C:\Users\Julian\Downloads\dashboard\static\index.html`
- HTML + CSS + JavaScript TODO inline (no build step, no frameworks).
- Puede usar Google Fonts (una o dos familias). Nada más externo.
- La UI va en ESPAÑOL.
- Debe funcionar servido desde `http://localhost:8080`.

## Backend (ya existe, no lo toques)
Mismo origen. Endpoints:

- `GET /api/state` devuelve JSON:
```json
{
  "connected": true,
  "port": "COM3",
  "mode": "IDLE | SCAN | JAM | UNKNOWN",
  "devices": [
    { "addr": "d4:9d:c0:77:4d:41", "name": "PIPE", "rssi": -66, "age": 0.9,
      "history": [-66, -64, -70, -66] }
  ],
  "log": ["[21:03:11] Conectado a COM3", ">> SCAN", "..."],
  "counters": { "scan_cycles": 3, "devices_seen": 9 },
  "ports": ["COM3"]
}
```
Notas: `rssi` es dBm (más cercano a 0 = más fuerte; rango típico -30 a -100).
`age` son segundos desde la última vez visto. `history` es el RSSI reciente (para sparkline).
`name` puede venir vacío.

- `POST /api/command` con body JSON `{ "cmd": "scan" | "jam" | "stop" }`
  devuelve `{ "ok": true, "sent": "SCAN", "detail": "ok" }`.

## Comportamiento requerido
1. Polling a `GET /api/state` cada 1 segundo (usa `setInterval`). Maneja errores de red con
   gracia (si falla, marca "desconectado" pero no rompe la UI).
2. **Header**: título del proyecto, indicador de conexión (punto verde/rojo + puerto COM),
   y un badge grande con el `mode` actual (IDLE gris, SCAN cian, JAM rojo).
3. **Controles** (botones grandes):
   - "Escanear"        -> POST {"cmd":"scan"}
   - "JAM"             -> POST {"cmd":"jam"}  (estilo peligro, con confirmación previa)
   - "Detener"         -> POST {"cmd":"stop"}
   Deshabilita el botón correspondiente al modo activo. Feedback visual al hacer click.
4. **Lista de dispositivos** ordenada por RSSI (más fuerte primero). Por cada uno:
   - Dirección MAC (monoespaciada)
   - Nombre (o "—" si vacío)
   - RSSI en dBm con color: fuerte(-30..-60) verde, medio(-60..-80) ámbar, débil(-80..-100) rojo
   - Barras de señal (5 barras) según RSSI
   - "hace Xs" (usando `age`)
   - Sparkline del `history` (SVG o canvas inline, sin librerías)
5. **Panel de log**: consola con autoscroll, fuente monoespaciada, últimas ~50 líneas.
6. **Panel de estadísticas**: total de dispositivos, ciclos de escaneo, último puerto.
7. Estados vacíos: si no hay dispositivos, mostrá un mensaje claro ("Sin dispositivos…
   presioná Escanear").
8. Responsive (se ve bien en laptop y en tablet).

## Estética
- Tema OSCURO, moderno y "bonito". Fondo tipo #0b0e14 con leve gradiente.
- Acentos: cian/verde para SCAN, rojo/ámbar para JAM.
- Estilo "cyber/terminal elegante": tipografía tipo JetBrains Mono / IBM Plex Mono para datos,
  y una sans (Inter / Geist) para el texto.
- Animaciones suaves (transiciones de color, pulse sutil en el badge JAM, fade en filas nuevas).
- Sin emojis en el HTML (usá iconos SVG inline simples si querés).

## Criterios de aceptación
- El archivo abre y, con el backend corriendo, muestra dispositivos y responde a los botones.
- No lanza errores en consola del navegador.
- Todo en un único archivo, sin dependencias de build.

Generá el archivo completo en esa ruta. No expliques, solo creá `static/index.html`.
