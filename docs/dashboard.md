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

The Arduino sketch emits one JSON line every 500 ms at 115200 baud. Seat A–D use A1–A4, MAX9814 AO uses A0, and LD2410 OUT uses D26 (HIGH means presence). Use a common ground. The microphone uses non-blocking 100 ms peak-to-peak windows with an 8-window moving average. An invalid IR driver reading becomes `null`; the radar OUT pin alone cannot detect a disconnected radar. The server timestamps each accepted packet.

`distanceCm`, `noiseAmplitude` and `radar` are the legacy fields and remain required. `seatOccupied`, `freeCount`, `noiseLevel`, `noiseStage`, `noiseWarning`, `mode` and `disagree` are optional; the dashboard prefers them when present and falls back to the legacy distance/noise thresholds otherwise.

For test-data logging, set `SERIAL_CSV` to `1` in the sketch; the USB bridge ignores the non-JSON CSV lines.

## USB setup on Windows

Copy or extract the **whole project folder**. The school computer needs Arduino IDE with Arduino AVR Boards support for uploading, Node.js 22 or newer for the dashboard, and a USB data cable. For a preview without hardware or Node.js, open `smart-library-weekly-dashboard.html` and choose a demo scene.

Before uploading, edit the pin constants at the top of `Smart_Library/Smart_Library.ino` to match your wiring. Set `SEAT_A_PIN` through `SEAT_D_PIN` and `MIC_PIN` to distinct analog inputs. Set `MMWAVE_OUT_PIN` to the digital input number (for example, `30` for D30). The current defaults are seats A1–A4, mic A0, and radar D26. Reupload the sketch after changing pins. Live hardware uses the firmware's `SEAT_THRESHOLD` and zone-mode noise thresholds; the HTML `CONFIG` thresholds are fallbacks for legacy packets and demo scenes.

1. Open `Smart_Library/Smart_Library.ino` in Arduino IDE, select **Arduino Mega or Mega 2560** and the board's COM port, then upload.
2. If you check the JSON in Serial Monitor at 115200 baud, close Serial Monitor before starting the bridge so it can open the COM port.
3. Open PowerShell in the project folder and run `.\start-dashboard.ps1`. Enter the Mega's COM port when prompted, or pass it directly with `.\start-dashboard.ps1 -Port COM3`. The launcher starts the local server, opens the browser, and forwards serial packets to the dashboard. Keep its terminal open during the demo.
4. Choose **Live hardware** and look for **Receiving data**. Use the seat details to read the measured distance. To inspect microphone and radar readings in Serial Monitor, first stop the launcher with Ctrl+C so it releases the COM port.

### Troubleshooting

| Symptom | What to check |
|---|---|
| No COM port | Check Arduino IDE's port menu, try a USB data cable, and check Windows Device Manager. |
| COM port access denied | Close Serial Monitor and any other bridge using that port. |
| Port 8080 in use | Stop the old server or use `.\start-dashboard.ps1 -WebPort 8081`. |
| Waiting for data or Data stale | Confirm the correct COM port and upload. With the bridge stopped, check for one JSON line roughly every half second in Serial Monitor at 115200 baud. |
| Readings arrive but seat or noise labels are wrong | Confirm the pin assignments, calibrate `SEAT_THRESHOLD` in the sketch, and check the firmware zone mode. Tune the HTML `CONFIG` only for legacy/demo fallback. |
| School policy blocks PowerShell scripts | Ask the school IT team to allow the script. The HTML demo opens directly in a browser. |

For a trusted demo LAN, set `$env:DASHBOARD_HOST = '0.0.0.0'` in PowerShell before starting the server. Use the computer's LAN IP in the browser and hardware sender, with port 8080. The telemetry endpoint has no authentication, so keep it on the trusted demo network.
