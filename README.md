# Wheeljack
**Authors:** Daniel Desai, Noah Vawter  
**Version:** 0.0.2  
**Updated:** 2026-03-04  

Automated vehicle lift system controller. Drives up to 4 DC motors via a Numato USBRELAY16 relay board and monitors power delivery via a Rigol DP831 programmable DC power supply. A Streamlit Python UI (`python_motor_controller.py`) wraps the compiled C++ executable for operator use.

---

## Hardware

| Device | Role | Interface |
|--------|------|-----------|
| Numato USBRELAY16 | 16-channel relay board — switches motor direction and enable | USB serial (COM port, 19200 baud) |
| Rigol DP831 | 3-channel programmable DC power supply — powers motors, monitors current | USB (VISA) |
| Motors 0–3 | DC motors mounted to jack stands | Relay-switched H-bridge via relay board |

---

## Relay Truth Table

Each motor uses 3 relays. Relay numbers match the Numato board (1-indexed). Commands are sent zero-padded: `relay on 01`, `relay off 10`, etc.

| Motor | Direction | Relays ON | Enable | Dir A | Dir B |
|-------|-----------|-----------|--------|-------|-------|
| 0 | Forward  | 01        | R01 | —   | —   |
| 0 | Reverse  | 01, 02, 03 | R01 | R02 | R03 |
| 1 | Forward  | 04        | R04 | —   | —   |
| 1 | Reverse  | 04, 05, 06 | R04 | R05 | R06 |
| 2 | Forward  | 07        | R07 | —   | —   |
| 2 | Reverse  | 07, 08, 09 | R07 | R08 | R09 |
| 3 | Forward  | 10        | R10 | —   | —   |
| 3 | Reverse  | 10, 11, 12 | R10 | R11 | R12 |

### Switching Sequence
**To run a motor:**
1. Set direction relays (Dir A / Dir B ON or OFF)
2. Close enable relay
3. Enable power supply output

**To stop a motor:**
1. Disable power supply output
2. Open enable relay
3. Direction relays remain unchanged

> ⚠️ Never change direction of relays while the enable relay is closed.

---

## Project Structure

```
Wheeljack/
├── wheeljack.cpp               # Main entry point — UI, jack_up(), emergency_shutoff()
├── motor_controller.cpp        # NumatoRelayController, MotorController, runMotorController()
├── motor_controller.h          # Class declarations and runMotorController() signatures
├── rigol_controller.cpp        # RigolDP831 implementation — VISA comms and test mode
├── rigol_controller.h          # RigolDP831 class declaration
├── python_motor_controller.py  # Streamlit UI — calls wheeljack.exe as subprocess
├── motor_configs.json          # Motor sequence configurations loaded by Python UI
├── Wheeljack_metadata.py       # Metadata tab content for Streamlit UI
└── Logo.png                    # Optional logo for Streamlit UI
```

---

## File Descriptions

### `wheeljack.cpp`
Main program entry point. Handles:
- Test mode selection UI
- Emergency power supply shutoff on startup
- COM port, pattern, direction, voltage, current, and runtime input
- `jack_up()` — runs the motor sequence with live power monitoring
- Current-limiting stall detection (95% of set current, 3/5 samples, 4 consecutive windows)

### `motor_controller.h` / `motor_controller.cpp`
Two classes:

**`NumatoRelayController`** — manages the serial connection to the Numato USBRELAY16:
- Opens COM port at 19200 baud with DTR asserted
- Sends zero-padded relay commands: `relay on 01`, `relay off 10`
- Tracks relay state internally

**`MotorController`** — wraps relay control with motor-level logic:
- `motorForward(n)` — sets direction OFF, closes enable relay
- `motorReverse(n)` — sets direction ON, closes enable relay
- `motorStop(n)` — opens enable relay, leaves direction unchanged
- `motorBrake(n)` — falls back to stop (no brake state in truth table)
- `stopAll()` — stops all 4 motors

Two overloads of `runMotorController()`:
- Interactive CLI mode
- Automated `std::bitset<4>` pattern mode with per-motor timeout and inter-motor delay

### `rigol_controller.h` / `rigol_controller.cpp`
**`RigolDP831`** — controls the Rigol DP831 power supply over VISA (USB):
- Auto-discovers the device if no resource string is provided
- `setVoltage(channel, v)` / `setCurrent(channel, i)` — configure output
- `enableOutput(channel, state)` — switch output on/off
- `getAllMeasurements(channel)` — returns voltage, current, and power
- `getSetVoltage()` / `getSetCurrent()` — read back programmed values
- Full test mode: simulates all responses with small random noise, no hardware required

### `python_motor_controller.py`
Streamlit web UI with 4 tabs:
- **INTAKE** — car number, VIN lookup (via NHTSA vPIC API), tire code, rim width
- **LIFT** — reserved for lift controls
- **SECURE** — motor control panel; per-motor BUMP / UP / DOWN buttons, MULTI-MOTOR sequence, ALL DOWN, STOP ALL; operating mode selector and COM port input
- **METADATA** — tabular metadata from `Wheeljack_metadata.py`

Spawns `wheeljack.exe` as a subprocess, pipes all inputs, and parses `[Sample ...]` lines from stdout for live voltage/current/power readback.

### `motor_configs.json`
Defines all named motor sequences as lists of rows with fields:
`motor_id`, `pattern`, `voltage`, `current`, `runtime`, `reverse`

Multi-motor sequences are assembled from references to rows in the simple configs.

---

## Build

Requires:
- MinGW g++ (MSYS2 ucrt64)
- NI-VISA or compatible VISA runtime installed at `C:\Program Files\IVI Foundation\VISA\`

```powershell
g++ wheeljack.cpp motor_controller.cpp rigol_controller.cpp -o wheeljack.exe -I"C:\Program Files\IVI Foundation\VISA\Win64\Include" "C:\Program Files\IVI Foundation\VISA\Win64\Lib_x64\msc\visa64.lib" -lsetupapi -lole32
```

---

## Test Modes

The system supports 4 operating modes selectable at startup:

| Mode | Power Supply | Relay Board | Use case |
|------|-------------|-------------|----------|
| 1 — Normal | Hardware | Hardware | Full operation |
| 2 — Power supply test | Simulated | Hardware | Test relay board only |
| 3 — Motor control test | Hardware | Simulated | Test power supply only |
| 4 — Full test | Simulated | Simulated | No hardware required |

In test mode, all VISA and serial commands are printed to stdout but not transmitted to any device. The Rigol test mode adds small random noise to simulated measurements to approximate real behaviour.

---

## Python UI Dependencies

```
streamlit
pandas
vpic
```

Install with:
```powershell
pip install streamlit pandas vpic
```

Run with:
```powershell
streamlit run python_motor_controller.py
```

> `wheeljack.exe` must be in the same directory as `python_motor_controller.py`.

---

## Safety Notes

- The system performs an **emergency shutoff** of the power supply on every startup before accepting any user input.
- The enable relay is always the **last relay closed** and **first relay opened** — the direction relays are never switched under load.
- The current monitoring loop detects motor stall via current limiting (≥95% of set current limit across 3 of 5 samples for 4 consecutive windows) and stops the motor automatically.
- **Never enable the power supply output while switching relays.**
