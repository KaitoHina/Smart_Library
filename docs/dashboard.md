# Smart Library Dashboard

Run `node dashboard-server.cjs` (Node.js 22 or newer) from the project folder and open `http://localhost:8080`. The HTML also opens directly for Demo mode. Use **Live hardware** when sending real data to the server.

## Calibration

Edit `CONFIG` near the top of the JavaScript in `smart-library-weekly-dashboard.html`, then reload the page:

- `seats.A` to `seats.D`: inclusive occupied and free ranges in centimetres. These are the fallback for legacy packets and demo scenes. Live hardware uses the firmware's debounced `seatOccupied` flags, calibrated with `SEAT_THRESHOLD` in the sketch.
- `noise`: moderate and noisy thresholds in peak-to-peak ADC counts measured over a consistent sampling window, plus how long noise must persist to raise an alert. These are the fallback for legacy packets and demo scenes; live hardware uses the firmware's zone-mode classification and `noiseWarning` flag. These are not dB values.
- `radar`: codes for area presence and absence. This check does not identify a particular seat.
- `staleAfterMs`: age at which an old packet stops counting as live. Send snapshots regularly, even when unchanged, at an interval shorter than this.

The defaults are examples for the demo, not calibrated measurements.

## Hardware input

The sender posts complete snapshots to `/api/telemetry` using `Content-Type: application/json`:

```json
{"distanceCm":{"A":65,"B":30,"C":30,"D":65},"seatOccupied":{"A":false,"B":true,"C":true,"D":false},"freeCount":2,"noiseAmplitude":15,"noiseLevel":"QUIET","noiseStage":0,"noiseWarning":false,"mode":"QUIET","radar":1,"disagree":false}
```

With the default `WIFI_TELEMETRY 1`, the Mega posts a complete JSON snapshot to `/api/telemetry` every second through the ESP8266. Set `WIFI_TELEMETRY` to `0` to print JSON over USB instead. Seat A–D use A1–A4, MAX9814 AO uses A0, and LD2410 OUT uses D26 (HIGH means presence). Use a common ground. The microphone uses 100 ms peak-to-peak windows with an 8-window moving average. An invalid IR driver reading becomes `null`; the radar OUT pin alone cannot detect a disconnected radar. The server timestamps each accepted packet.

`distanceCm`, `noiseAmplitude` and `radar` are the legacy fields and remain required. `seatOccupied`, `freeCount`, `noiseLevel`, `noiseStage`, `noiseWarning`, `mode` and `disagree` are optional; the dashboard prefers them when present and falls back to the legacy distance/noise thresholds otherwise.

For test-data logging, set `SERIAL_CSV` to `1` in the sketch; CSV output is printed on USB Serial.

## USB setup on Windows

Copy or extract the **whole project folder**. The school computer needs Arduino IDE with Arduino AVR Boards support for uploading, Node.js 22 or newer for the dashboard, and a USB data cable. For a preview without hardware or Node.js, open `smart-library-weekly-dashboard.html` and choose a demo scene.

Before uploading, edit the pin constants at the top of `Smart_Library/Smart_Library.ino` to match your wiring. Set `SEAT_A_PIN` through `SEAT_D_PIN` and `MIC_PIN` to distinct analog inputs. Set `MMWAVE_OUT_PIN` to the digital input number (for example, `30` for D30). The current defaults are seats A1–A4, mic A0, and radar D26. Reupload the sketch after changing pins. Live hardware uses the firmware's `SEAT_THRESHOLD` and zone-mode noise thresholds; the HTML `CONFIG` thresholds are fallbacks for legacy packets and demo scenes.

For USB-only operation, first set `WIFI_TELEMETRY` to `0` in `Smart_Library/Smart_Library.ino`, then upload it to the Mega. Open PowerShell in the project folder and run `.\start-dashboard.ps1`; enter the Mega COM port or pass it with `-Port COM3`. The launcher forwards USB JSON packets to the dashboard. For Wi-Fi operation, follow the next section instead.

### Troubleshooting

| Symptom | What to check |
|---|---|
| No COM port | Check Arduino IDE's port menu, try a USB data cable, and check Windows Device Manager. |
| COM port access denied | Close Serial Monitor and any other bridge using that port. |
| Port 8080 in use | Stop the old server or use `.\start-dashboard.ps1 -WebPort 8081`. |
| Waiting for data or Data stale (USB mode) | Confirm `WIFI_TELEMETRY` is `0`, the correct COM port is selected, and the Mega prints JSON at 115200 baud. |
| Readings arrive but seat or noise labels are wrong | Confirm the pin assignments, calibrate `SEAT_THRESHOLD` in the sketch, and check the firmware zone mode. Tune the HTML `CONFIG` only for legacy/demo fallback. |
| School policy blocks PowerShell scripts | Ask the school IT team to allow the script. The HTML demo opens directly in a browser. |

## Wi-Fi setup with ESP8266

The default path is:

```text
Mega 2560 sensors -> Mega Serial1 -> ESP8266 -> Wi-Fi -> dashboard computer
```

The Mega runs the `WiFiEsp` library and controls an ESP8266 that already has AT firmware. You only upload the main sketch to the Mega; no ESP8266 sketch upload is needed. The Mega's USB `Serial` remains available for event/debug messages.

### Wiring

Use a proper 3.3 V supply for the ESP8266 (at least 500 mA) and connect the grounds:

| Mega 2560 | ESP8266 | Note |
|---|---|---|
| D18 / TX1 | RX / GPIO3 | Required. Put a level shifter or resistor divider here; Mega TX is 5 V. |
| D19 / RX1 | TX / GPIO1 | Required for the ESP8266's AT responses; 3.3 V TX is normally readable by the Mega. |
| GND | GND | Common ground is required. |
| 3.3 V supply | VCC / EN | Do not power the ESP8266 from an unsuitable 5 V pin. |

For an ESP-01, use its pins labelled `RX`, `TX`, `VCC`, `EN/CH_PD`, and `GND`. Do not connect Mega TX directly to ESP RX. Set `ESP_AT_BAUD` in the Mega sketch to match the AT firmware's UART baud rate.

### Firmware and dashboard

1. Install the **WiFiEsp** library from Arduino IDE's Library Manager.
2. In `Smart_Library/Smart_Library.ino`, set `WIFI_SSID`, `WIFI_PASSWORD`, and `DASHBOARD_HOST`. Use the PC's LAN IPv4 address for `DASHBOARD_HOST`, for example `192.168.1.23`; keep `DASHBOARD_PORT` at `8080` unless you changed the server port.
3. Confirm the ESP8266 responds to AT commands and that `ESP_AT_BAUD` matches its UART baud rate. This approach requires AT firmware on the ESP8266; if it has other firmware or no firmware, WiFiEsp cannot control it until compatible AT firmware is installed.
4. Upload `Smart_Library/Smart_Library.ino` to the Mega. No ESP8266 Arduino sketch is uploaded.
5. On the dashboard computer, run `.\start-dashboard-wifi.ps1`. This starts the server listening on the LAN and opens the browser.
6. Select **Live hardware**. The page should change from **Waiting for data** to **Receiving data**.

On macOS or Linux, use `DASHBOARD_HOST=0.0.0.0 PORT=8080 node dashboard-server.cjs` instead of the PowerShell launcher, then open `http://localhost:8080`.

Use `ipconfig` on Windows to find the computer's Ethernet IPv4 address. The ESP8266 should join a Wi-Fi network that can reach that wired LAN. If HTTP POST fails, allow Node.js through the Windows firewall on the private network and check that the router does not block Wi-Fi-to-Ethernet traffic.

To switch back to USB telemetry, set `WIFI_TELEMETRY` to `0`, upload the Mega sketch again, and use `.\start-dashboard.ps1 -Port COMx`.

For a trusted demo LAN, set `$env:DASHBOARD_HOST = '0.0.0.0'` in PowerShell before starting the server. Use the computer's LAN IP in the browser and hardware sender, with port 8080. The telemetry endpoint has no authentication, so keep it on the trusted demo network.
