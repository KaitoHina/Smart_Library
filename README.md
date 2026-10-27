# Smart Library

Arduino Mega 2560 based seat availability + noise monitoring system for library study zones.

- Live seat map and free-seat count (4 seats)
- Noise level classification with warning feedback
- mmWave radar cross-check (sensor fusion)
- OLED local dashboard

Project proposal: `docs/PPR-L01-GP6.pdf`
Task list and schedule: `docs/task-list.md`

---

## 1. How to clone

The Arduino IDE requires the sketch folder name to match the `.ino` file name, so clone it **directly into your `Documents/Arduino` folder**.

```bash
cd ~/Documents/Arduino
git clone https://github.com/KaitoHina/Smart_Library.git
```

Windows (PowerShell):

```powershell
cd $HOME\Documents\Arduino
git clone https://github.com/KaitoHina/Smart_Library.git
```

Result:

```
Documents/Arduino/Smart_Library/
├── Smart_Library.ino        <- main sketch
├── GP2Y0A21YK0F.h / .cpp    <- IR distance sensor driver
├── README.md
└── docs/                    <- proposal, gantt chart, task list
```

If you already have a folder named `Smart_Library` there, move or delete it first.

## 2. How to open it

1. Install **Arduino IDE 2.x** from https://www.arduino.cc/en/software
2. `File > Open...` and select:
   `Documents/Arduino/Smart_Library/Smart_Library.ino`
   (or double-click the `.ino` file)
3. `Tools > Board` → **Arduino Mega or Mega 2560**
4. `Tools > Processor` → **ATmega2560**
5. `Tools > Port` → the port that appears when you plug the board in
6. Press **Verify** (✓), then **Upload** (→)

Install these libraries via `Tools > Manage Libraries...`:

| Library | Author | Used for |
|---|---|---|
| Adafruit SSD1306 | Adafruit | OLED display |
| Adafruit GFX Library | Adafruit | graphics primitives |

## 3. Hardware / components

| Component | Qty | Notes |
|---|---|---|
| Arduino Mega 2560 | 1 | Main controller |
| GP2Y0A21YK0F IR distance sensor | 4 | One per seat. Analog output, usable range 10–80 cm |
| MAX9814 microphone module | 1 | Analog output, adjustable gain jumper |
| LD2410 mmWave radar | 1 | Presence detection. 3.3 V logic — level shifter needed if using UART |
| SSD1306 OLED 128×64 | 1 | I2C, address 0x3C |
| RGB LED module | 1 | Zone status colour |
| LED + resistor | 4 | Per-seat status LED |
| Active buzzer | 1 | Short non-blocking beep |
| Push button | 2 | Mode switch, warning acknowledge |

### Provisional pin map

> Not final yet — see task 1 in `docs/task-list.md`. Do not wire two functions to the same pin.

| Function | Pin |
|---|---|
| IR seat sensors 1–4 | A0, A1, A2, A3 |
| Microphone (MAX9814 AO) | A4 |
| OLED SDA / SCL | 20 / 21 |
| Seat status LEDs 1–4 | D27, D28, D29, D30 |
| RGB LED (R, G, B) | D4, D3, D2 |
| Buzzer | D5 |
| Mode button / Acknowledge button | D31 / D32 (INPUT_PULLUP) |
| Radar OUT | D26 |
| Radar UART (optional, Serial2) | TX2 = D16, RX2 = D17 |

### IR sensor notes

- Output is **analog voltage that decreases as the object gets closer** — it is not a linear distance sensor.
- The driver `GP2Y0A21YK0F.cpp` converts voltage to cm using `distance = 27.86 / (V - 0.42)` and clamps to 10–80 cm.
- Readings below 10 cm return `-1` (out of range). A hand directly in front of the sensor may report far or invalid — mount so the seated person is roughly 30–60 cm away.
- Always read 5–10 samples and average before deciding occupied/free.

## 4. Git workflow for group members

```bash
# before starting work
git pull

# after finishing a piece of work
git add .
git commit -m "seat: add debounce logic"
git push
```

Rules:

- Pull before you start, push when you finish. Do not leave work uncommitted overnight.
- One commit per task from `docs/task-list.md`, message starts with the module: `seat:`, `noise:`, `radar:`, `oled:`, `web:`, `docs:`.
- `.vscode/` is git-ignored — your editor settings stay local, do not force-add it.
- Do not commit `build/` output or library folders.
- If two people edit `Smart_Library.ino` at the same time, pull first. Conflicts in the `.ino` file are painful.

## 5. What to work on

Open `docs/task-list.md`. Pick a main task, put your name in the **Owner** column, and commit that change so others can see it.

Current status: components have arrived, IR sensor reading verified, everything else pending.

## 6. Troubleshooting

| Symptom | Check |
|---|---|
| Upload fails: port not found | Install CH340 driver if the board is a clone; try another USB cable (some are charge-only) |
| OLED blank | I2C address — try `0x3D` if `0x3C` fails; check SDA/SCL are not swapped |
| IR readings jump around | Power the sensors from 5 V, add a 10 µF capacitor near the sensor, average multiple samples |
| Radar always reports presence | Wait 10 s after power-up, radar needs warm-up; check the level shifter supply |
| Noise alert never fires | Check the MAX9814 gain jumper and read the raw values over Serial first |
