# VDAS Unified Control Application

A PyQt6 desktop application for controlling a VIMICRO VDAS unit over a
serial/SCPI connection: 6-channel ADC live acquisition and scope, 4-channel
DAC control, and dual PID loop control (Loop A / Loop B), with CSV data
logging and a diagnostics console.

This is a **multi-file, restructured** version of the original single-file
script. Every SCPI command, timing rule, hold-last-good/settle-cycle
behavior, and CSV log format is identical to the original — nothing was
added or removed functionally, except the new **device identification
display** described below. The UI has been restyled (dark theme, consistent
spacing/typography) for readability.

---

## 1. What's new vs. the original single-file version

- **Device identification on connect.** Immediately after a successful
  connection, the app sends `*IDN?`. The VDAS replies in the form:

  ```
  *IDN?->(VIMICRO,VDAS,01,3.3)
  ```

  This is parsed (with the same echo-verification the rest of the app
  uses — a mismatched echo is discarded, never guessed at) into:

  | Field      | Meaning             | Example   |
  |------------|---------------------|-----------|
  | Company    | Manufacturer        | VIMICRO   |
  | Product    | Product name        | VDAS      |
  | Model      | Model number        | 01        |
  | Firmware   | Firmware version    | 3.3       |

  These four fields are shown directly in the **Hardware Connection**
  panel at the top of the window, right under the port/baud/connect
  controls. They reset to `---` on disconnect, and if the identification
  query ever fails or returns something malformed, they stay at `---`
  and the mismatch is logged to the Diagnostics tab instead of showing a
  guess.

- **Restyled UI.** A single dark QSS theme (`vdas/ui/style.py`) is applied
  application-wide: consistent card-style group boxes, rounded buttons/
  inputs, a proper tab bar, monospace value/log displays, and color-coded
  status text (green = active/connected, red = off/disconnected). No
  control was moved, renamed, removed, or had its behavior changed —
  only appearance.

- **Split into a package** instead of one ~1000-line file, so each panel
  can be read, tested, and modified independently (see structure below).

---

## 2. Project structure

```
vdas_control/
├── main.py                     # Entry point — run this
├── requirements.txt
├── vdas.spec                   # PyInstaller build spec
├── build_exe.bat               # One-click Windows exe build
├── build_exe.sh                # Linux/macOS build (native binary, not .exe)
├── README.md
└── vdas/
    ├── core/
    │   ├── scpi_controller.py  # Serial ownership, echo-verified parsing,
    │   │                       # ADC/DAC/PID commands, *IDN? parsing
    │   └── workers.py          # ADCWorker, PIDWorker (background QThreads)
    └── ui/
        ├── style.py            # App-wide QSS theme + button style helpers
        ├── widgets.py          # create_value_label(), DACChannelCard
        ├── main_window.py      # VDASApplication — combines all tabs below
        └── tabs/
            ├── connection.py   # Connection panel + *IDN? identity display
            ├── dashboard.py    # Dashboard tab
            ├── graph.py        # Graph (ADC Scope) tab + acquisition worker
            ├── logging_panel.py# CSV data logging (shown inside Graph tab)
            ├── dac.py          # DAC tab
            ├── pid.py          # PID tab + PID worker
            ├── diagnostics.py  # Diagnostics tab (SCPI log, manual commands)
            └── safety.py       # SAFE SHUTDOWN button + close-event handling
```

`VDASApplication` (in `main_window.py`) is built from mixins — one class
per tab file — so it still behaves as a single window with everything
wired together exactly like the original, but each tab's code lives in
its own file.

---

## 3. Setup

Requires Python 3.10+.

```bash
cd vdas_control
python -m venv venv

# Windows
venv\Scripts\activate

# Linux/macOS
source venv/bin/activate

pip install -r requirements.txt
```

Run it:

```bash
python main.py
```

---

## 4. Controlling the application

### 4.1 Connect to hardware

1. Plug in the VDAS unit. Click **Refresh Ports** if it doesn't
   appear in the **Serial Port** dropdown.
2. Set the **Baud** rate (default `115200`, matching the device).
3. Click **Connect Hardware**.
4. On success:
   - The status dot turns green (**● Connected**).
   - The app sends `*IDN?` and fills in **Company / Product / Model /
     Firmware** in the same panel.
   - All 6 ADC channels are configured on the device (`CONF:VOLT`/
     `CONF:CURR` per the current per-channel mode).
   - The PID monitor worker starts automatically (polls both loops
     every 0.5 s).
5. Click **Disconnect Hardware** to safely shut everything down (PID
   loops off, DAC outputs off), stop all background polling, stop any
   active CSV logging, and close the port.

### 4.2 Dashboard tab

Read-only live summary: all 6 ADC channel values, and both PID loops'
Status/Measured/Error/Output. Useful as an at-a-glance view while working
in another tab.

### 4.3 Graph (ADC Scope) tab

- **ADC Channel Setup & Live Values** — CH0 and CH1 can be switched
  between `CONF:VOLT (0-5V)` and `CONF:CURR (4-20mA)` from the dropdown;
  CH2–CH5 are fixed to voltage mode. Changing a mode here also updates
  the matching PID loop's "Channel Type" if that channel is a PID input.
- **Re-Apply All ADC Hardware Configurations** — re-sends `CONF:VOLT`/
  `CONF:CURR` for all 6 channels (useful after a device reset).
- **Acquisition Settings** — set the sample rate (1–50 Hz) and, if you
  ever want to bound memory on a very long run, a **History Limit** (0 =
  Unlimited by default, meaning every sample this session stays in the
  buffers). Then **Start/Stop Acquisition**.
- Per-channel checkboxes toggle which curves are drawn. **Clear Plot**
  empties all buffers and the plot (the only thing that actually deletes
  data). **Auto Range** fits the view to everything currently plotted —
  use it after dragging or zooming to jump back to seeing the whole
  trace. Dragging/zooming itself never deletes anything; with History
  Limit left at "Unlimited," you can always scroll back through the full
  session.
- **Data Logging (CSV)** — pick a `.csv` path, then **Start Logging**.
  Each logged row while acquisition is running contains all 6 ADC
  values + units, and both PID loops' configured mode/setpoint/Kp/Ki/Kd
  plus their live status/measured/error/output. **Stop Logging** closes
  the file.

### 4.4 DAC tab

Four independent channel cards (A–D). For each: set a voltage (spin box
or slider, 0–5 V), **Apply Voltage** to push it to the device, and
**Enable Output** / **Disable Output** to turn that channel's output on
or off.

### 4.5 PID tab

Two independent loop panels (Loop A defaults to voltage control, Loop B
to current — both are user-selectable):

- **Channel Type** — Voltage (V) or Current (mA). Changing this also
  reconfigures the underlying ADC channel via `CONF:VOLT`/`CONF:CURR`
  and keeps the Graph tab's dropdown for that channel in sync.
- **Setpoint Mode** — `OFF / Follower`, `Engineering` (direct setpoint
  value), or `Percentage` (0–100%).
- **Setpoint / Kp / Ki / Kd** — enter values, then click **UPDATE** to
  push everything (input/output channel assignment, mode, setpoint,
  gains) to the device in one go.
- **PID ON / PID OFF** — enable or disable that loop.
- **Live PID Monitor** — status (ACTIVE/OFF), measured value, error, and
  output for both loops, polled continuously in the background.

### 4.6 Diagnostics tab

- A live, timestamped log of every command sent and every response
  received (and any comm/parsing errors).
- **Query \*IDN?** — manually re-sends the identification query.
- **Custom command** — send any raw SCPI command directly to the device.

### 4.7 SAFE SHUTDOWN

The red bar at the bottom of the window immediately turns both PID loops
off and all 4 DAC outputs off. This also runs automatically when you
disconnect, and you'll be prompted to run it if you close the window
while still connected.

---

## 5. Building a standalone .exe (Windows)

This uses [PyInstaller](https://pyinstaller.org/) to bundle the app,
Python, and all dependencies into a single `VDAS.exe` that runs without
a Python install on the target machine.

**Important:** PyInstaller does not cross-compile. To produce a Windows
`.exe` you must run the build **on a Windows machine** (or a Windows VM).
Building on Linux/macOS produces a native binary for that OS instead.

### Option A — one-click script

On Windows, from the project folder:

```
build_exe.bat
```

This installs `requirements.txt` plus PyInstaller, then builds using the
included `vdas.spec`. The result is at:

```
dist\VDAS.exe
```

### Option B — manual steps

```powershell
cd vdas_control
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
pip install pyinstaller

pyinstaller --noconfirm --clean vdas.spec
```

The finished executable is `dist\VDAS.exe` — copy that single file
anywhere and run it directly.

### Notes

- The build is **windowed** (no console window pops up behind the GUI).
  If you need to see console output/tracebacks while debugging a build,
  temporarily edit `vdas.spec` and set `console=True`, rebuild, then set
  it back to `False` for the release build.
- To give the exe a custom icon, provide a `.ico` file in the project
  folder and uncomment the `icon=...` line near the bottom of
  `vdas.spec`.
- If Windows Defender/SmartScreen flags the freshly built exe (common
  for unsigned PyInstaller binaries), that's expected for an unsigned
  executable — code-signing it is outside the scope of this build script.
- On Linux/macOS, `./build_exe.sh` produces an equivalent native
  standalone binary in `dist/` using the same `vdas.spec`.
