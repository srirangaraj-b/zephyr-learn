"""
vdas.ui.tabs.pid
===================

The "PID" tab: two PID loop configuration panels plus a live monitor grid,
and the PIDWorker lifecycle / handle_pid_data() callback that feeds it.

Both loops' input channel (CH0/CH1) is permanently Current (4-20mA) - see
GraphTabMixin - so there is no "Channel Type" control here anymore; the
loop always treats its ADC input as 4-20mA internally, it's just not
surfaced in the UI.

Setpoint Mode has two options, and in both the Setpoint field is always a
0-100% value - there is no engineering-unit entry anymore:

- Manual:     Open-loop. Kp/Ki/Kd are disabled (not sent, not editable).
              Pressing UPDATE converts the percentage straight to a DAC
              drive voltage (0-100% -> 0-5V, the same range the old DAC
              tab used) and pushes it with the same SOUR:VOLT / OUTP ON
              calls the DAC tab used to make - i.e. it drives the DAC
              output directly, bypassing closed-loop control entirely.
- Percentage: Normal closed-loop PID. Kp/Ki/Kd are enabled and sent along
              with PID:SET:PERC, exactly like the previous Percentage mode.

The Live PID Monitor (and the graph's live PID curves) always show
Measured/Error/Output in percent, converted from the raw wire values using
the channel's fixed 4-20mA span (Measured, Error) and the DAC's 0-5V span
(Output).
"""

from PyQt6.QtGui import QDoubleValidator, QFont
from PyQt6.QtWidgets import (
    QComboBox, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit,
    QMessageBox, QPushButton, QVBoxLayout, QWidget,
)

from vdas.core.workers import PIDWorker
from vdas.ui.widgets import create_value_label

# Fixed engineering ranges used to convert raw PID wire values into
# percentages for display/plotting. CH0/CH1 are always 4-20mA (see
# GraphTabMixin), and the PID output drives the DAC's 0-5V range.
MEASURED_RANGE = (4.0, 20.0)   # mA - PID:MEAS?, and the span for PID:ERR?
DAC_RANGE = (0.0, 5.0)         # V  - PID:OUT? (drives the loop's DAC channel)


def _value_to_percent(raw: str, lo: float, hi: float, is_delta: bool = False):
    """
    Converts a raw PID:MEAS?/PID:ERR?/PID:OUT? reply into a 0-100(+)
    percentage of the given engineering-unit span. Returns None if raw
    isn't a parseable number (e.g. "---"). is_delta=True skips subtracting
    the range's lower bound, since PID:ERR? is already a difference
    (setpoint - measured), not an absolute reading - only the span itself
    is used to scale it.
    """
    try:
        value = float(raw)
    except (TypeError, ValueError):
        return None

    span = hi - lo
    if span == 0:
        return None

    if is_delta:
        return (value / span) * 100.0
    return ((value - lo) / span) * 100.0


class PIDTabMixin:
    def create_pid_tab(self):
        tab = QWidget()
        root = QVBoxLayout(tab)

        pid_row = QHBoxLayout()
        pid_row.addWidget(self.create_pid_panel(loop=0, title="PID Loop A", default_setpoint="50.00"))
        pid_row.addWidget(self.create_pid_panel(loop=1, title="PID Loop B", default_setpoint="50.00"))
        root.addLayout(pid_row)

        monitor_box = QGroupBox("Live PID Monitor (%)")
        monitor_layout = QGridLayout()

        headers = [("", 0), ("PID Loop A", 1), ("PID Loop B", 2)]
        for text, col in headers:
            lbl = QLabel(text)
            lbl.setFont(QFont("Arial", 9, QFont.Weight.Bold))
            monitor_layout.addWidget(lbl, 0, col)

        monitor_layout.addWidget(QLabel("Status"), 1, 0)
        monitor_layout.addWidget(QLabel("Measured"), 2, 0)
        monitor_layout.addWidget(QLabel("Error"), 3, 0)
        monitor_layout.addWidget(QLabel("Output"), 4, 0)

        for loop in (0, 1):
            col = loop + 1
            self.pid_monitor_labels[loop] = {
                "status": create_value_label(),
                "meas": create_value_label(),
                "err": create_value_label(),
                "out": create_value_label(),
            }
            monitor_layout.addWidget(self.pid_monitor_labels[loop]["status"], 1, col)
            monitor_layout.addWidget(self.pid_monitor_labels[loop]["meas"], 2, col)
            monitor_layout.addWidget(self.pid_monitor_labels[loop]["err"], 3, col)
            monitor_layout.addWidget(self.pid_monitor_labels[loop]["out"], 4, col)

        monitor_box.setLayout(monitor_layout)
        root.addWidget(monitor_box)
        root.addStretch()

        return tab

    def create_pid_panel(self, loop, title, default_setpoint):
        box = QGroupBox(title)
        layout = QGridLayout()
        layout.setVerticalSpacing(7)

        input_channel = str(loop)
        output_channel = str(loop)

        layout.addWidget(QLabel("PID Loop:"), 0, 0)
        layout.addWidget(QLabel(str(loop)), 0, 1)

        layout.addWidget(QLabel("ADC Input:"), 1, 0)
        layout.addWidget(QLabel(f"{input_channel} (fixed 4-20mA)"), 1, 1)

        layout.addWidget(QLabel("DAC Output:"), 2, 0)
        layout.addWidget(QLabel(output_channel), 2, 1)

        layout.addWidget(QLabel("Setpoint Mode:"), 3, 0)
        mode_combo = QComboBox()
        mode_combo.addItems(["Manual", "Percentage"])
        mode_combo.setCurrentIndex(1)
        layout.addWidget(mode_combo, 3, 1)

        layout.addWidget(QLabel("Setpoint:"), 4, 0)
        setpoint = QLineEdit(default_setpoint)
        setpoint.setValidator(QDoubleValidator(0.0, 100.0, 4))
        layout.addWidget(setpoint, 4, 1)

        unit_label = QLabel("%")
        layout.addWidget(unit_label, 4, 2)

        layout.addWidget(QLabel("Kp:"), 5, 0)
        kp = QLineEdit("1.0000")
        kp.setValidator(QDoubleValidator(0.0, 1000000.0, 6))
        layout.addWidget(kp, 5, 1)

        layout.addWidget(QLabel("Ki:"), 6, 0)
        ki = QLineEdit("0.0000")
        ki.setValidator(QDoubleValidator(0.0, 1000000.0, 6))
        layout.addWidget(ki, 6, 1)

        layout.addWidget(QLabel("Kd:"), 7, 0)
        kd = QLineEdit("0.0000")
        kd.setValidator(QDoubleValidator(0.0, 1000000.0, 6))
        layout.addWidget(kd, 7, 1)

        update_button = QPushButton("UPDATE")
        update_button.clicked.connect(
            lambda: self.update_pid(loop, mode_combo, setpoint, kp, ki, kd)
        )
        layout.addWidget(update_button, 8, 0, 1, 3)

        on_button = QPushButton("PID ON")
        on_button.clicked.connect(lambda: self.pid_on(loop))

        off_button = QPushButton("PID OFF")
        off_button.clicked.connect(lambda: self.pid_off(loop))

        button_layout = QHBoxLayout()
        button_layout.addWidget(on_button)
        button_layout.addWidget(off_button)
        layout.addLayout(button_layout, 9, 0, 1, 3)

        box.setLayout(layout)

        self.pid_inputs[loop] = {
            "mode": mode_combo, "setpoint": setpoint,
            "kp": kp, "ki": ki, "kd": kd, "unit_label": unit_label,
            "on_button": on_button, "off_button": off_button,
        }

        mode_combo.currentIndexChanged.connect(lambda idx, l=loop: self.on_pid_mode_changed(l, idx))
        # Apply the initial (Percentage) mode's enabled/disabled state.
        self.on_pid_mode_changed(loop, mode_combo.currentIndex())

        return box

    def on_pid_mode_changed(self, loop, index):
        """
        Manual (index 0) is open-loop and drives the DAC directly, so
        Kp/Ki/Kd don't apply - disable them so it's clear they won't be
        sent. Percentage (index 1) is closed-loop PID, so they're enabled.
        """
        is_manual = index == 0
        widgets = self.pid_inputs[loop]

        widgets["kp"].setEnabled(not is_manual)
        widgets["ki"].setEnabled(not is_manual)
        widgets["kd"].setEnabled(not is_manual)
        # Manual mode drives the DAC directly and never engages closed-loop
        # control, so PID ON has nothing to turn on.
        widgets["on_button"].setEnabled(not is_manual)

    # -------------------------------------------------------------------
    # PID MONITORING (worker thread)
    # -------------------------------------------------------------------
    def start_pid_worker(self):
        if self.pid_worker and self.pid_worker.isRunning():
            return

        self.pid_worker = PIDWorker(self.scpi)
        self.pid_worker.data_received.connect(self.handle_pid_data)
        self.pid_worker.start()

    def stop_pid_worker(self):
        if self.pid_worker:
            self.pid_worker.stop()
            self.pid_worker.wait()
            self.pid_worker = None

    def handle_pid_data(self, data: dict):
        for loop in (0, 1):
            raw = data.get(loop)
            if not raw:
                continue

            cache = self.pid_hold_cache[loop]

            # Resolve status: hold the last known status if this poll's
            # STAT? query failed (echo mismatch / no response). If we've
            # never received a valid status at all, fall back to "---".
            status_value = raw["status"]
            if status_value not in ("---", ""):
                cache["status"] = status_value
            effective_status = cache["status"] if cache["status"] is not None else "---"

            is_on = effective_status == "1"

            resolved = {"status": effective_status}

            for field in ("meas", "err", "out"):
                value = raw[field]

                if not is_on:
                    # Loop is actually OFF (or its state is unknown) -
                    # explicitly show "---" and drop any held value, so a
                    # loop that's off doesn't keep showing a stale reading
                    # from the last time it was running.
                    resolved[field] = "---"
                    cache[field] = None
                elif value not in ("---", ""):
                    # Fresh, verified value this cycle.
                    cache[field] = value
                    resolved[field] = value
                else:
                    # Loop is ON but this cycle's read failed - hold the
                    # last good value instead of blanking it to "---".
                    resolved[field] = cache[field] if cache[field] is not None else "---"

            # Convert the resolved raw wire values (mA for meas/err, V for
            # out) into percentages of their fixed engineering ranges. All
            # three convert to None if the raw value is "---"/unparseable.
            meas_pct = _value_to_percent(resolved["meas"], *MEASURED_RANGE)
            err_pct = _value_to_percent(resolved["err"], *MEASURED_RANGE, is_delta=True)
            out_pct = _value_to_percent(resolved["out"], *DAC_RANGE)

            def fmt(pct):
                # Plain numeric string (no unit) - kept this way so a
                # logged CSV cell is a clean number under a "_Pct" header;
                # the Live PID Monitor label adds the "%" suffix itself.
                return f"{pct:.2f}" if pct is not None else "---"

            snapshot = {
                "status": effective_status,
                "meas": fmt(meas_pct),
                "err": fmt(err_pct),
                "out": fmt(out_pct),
            }
            # Keep the resolved (percentage) snapshot for CSV logging too,
            # so a logged row matches what's actually shown on screen.
            self.last_pid_snapshot[loop] = snapshot

            for field in ("meas", "err", "out"):
                text = f"{snapshot[field]} %" if snapshot[field] != "---" else "---"
                self.pid_monitor_labels[loop][field].setText(text)

            status_label = self.pid_monitor_labels[loop]["status"]
            if effective_status == "1":
                status_label.setText("ACTIVE")
                status_label.setStyleSheet("color: #2fbf71; font-weight: bold;")
            elif effective_status == "0":
                status_label.setText("OFF")
                status_label.setStyleSheet("color: #e5484d; font-weight: bold;")
            else:
                status_label.setText(effective_status)
                status_label.setStyleSheet("")

            # Feed the graph's live PID % curves - only meaningful, and
            # only plotted, while the loop is actually ON (see
            # GraphTabMixin.append_pid_percent_sample).
            if is_on:
                self.append_pid_percent_sample(loop, meas_pct, err_pct, out_pct)

    # -------------------------------------------------------------------
    # PID CONFIGURATION
    # -------------------------------------------------------------------
    def update_pid(self, loop, mode_combo, setpoint_edit, kp_edit, ki_edit, kd_edit):
        if not self.scpi.is_connected():
            QMessageBox.warning(self, "Not Connected", "Connect to the VDAS first.")
            return

        try:
            setpoint_percent = float(setpoint_edit.text())
        except ValueError:
            QMessageBox.warning(self, "Invalid Value", "Please enter a valid numeric setpoint.")
            return

        if not 0.0 <= setpoint_percent <= 100.0:
            QMessageBox.warning(self, "Invalid Percentage", "Setpoint must be between 0 and 100%.")
            return

        channel = loop

        # CH0/CH1 are always Current (4-20mA) for a PID loop's input -
        # reassert it on every UPDATE so the ADC front-end never drifts
        # out of sync with what the loop assumes.
        self.sync_adc_channel_mode(loop, True)

        commands = [
            f"PID:IN:CHAN (@{loop}) {loop}",
            f"PID:OUT:CHAN (@{loop}) {loop}",
        ]

        mode = mode_combo.currentText()

        # Setpoint is always entered as 0-100% now; convert to the
        # channel's fixed 4-20mA engineering range purely to draw the
        # setpoint reference line on the graph's V/mA axis.
        engineering_value = self.percent_to_engineering(True, setpoint_percent)
        self.update_setpoint_line(loop, engineering_value)

        if mode == "Manual":
            # Open-loop: make sure closed-loop control is off, then drive
            # the DAC directly - same underlying calls the old DAC tab
            # used (SOUR:VOLT then OUTP ON), just addressed at this loop's
            # DAC output channel and computed from the percentage.
            commands.append(f"PID:SET:MODE (@{channel}) OFF")

            for command in commands:
                self.scpi.send_cmd(command)

            dac_voltage = (setpoint_percent / 100.0) * 5.0
            self.scpi.set_dac_voltage(loop, dac_voltage)
            self.scpi.set_dac_output(loop, True)

            self.log_message(
                f"PID {loop} Manual: setpoint {setpoint_percent:.2f}% -> "
                f"DAC {dac_voltage:.4f} V (output enabled)."
            )
            return

        # mode == "Percentage": normal closed-loop PID.
        try:
            kp = float(kp_edit.text())
            ki = float(ki_edit.text())
            kd = float(kd_edit.text())
        except ValueError:
            QMessageBox.warning(self, "Invalid Value", "Please enter valid numeric Kp/Ki/Kd values.")
            return

        commands.append(f"PID:SET:MODE (@{channel}) ON")
        commands.append(f"PID:SET:PERC (@{channel}) {setpoint_percent:.4f}")
        commands.append(f"PID:KP (@{channel}) {kp:.6f}")
        commands.append(f"PID:KI (@{channel}) {ki:.6f}")
        commands.append(f"PID:KD (@{channel}) {kd:.6f}")

        for command in commands:
            self.scpi.send_cmd(command)

        self.log_message(f"PID {loop} Percentage: setpoint {setpoint_percent:.2f}%, variables updated.")

    def pid_on(self, loop):
        if not self.scpi.is_connected():
            QMessageBox.warning(self, "Not Connected", "Connect to the VDAS first.")
            return
        self.scpi.pid_on(loop)

    def pid_off(self, loop):
        if not self.scpi.is_connected():
            QMessageBox.warning(self, "Not Connected", "Connect to the VDAS first.")
            return
        self.scpi.pid_off(loop)
