"""
vdas.ui.tabs.graph
=====================

The "Graph (ADC Scope)" tab: CH0/CH1 are permanently fixed to Current
(4-20mA) mode (they feed the two PID loops), CH2-CH5 are plain Voltage
monitoring channels. This tab also owns the ADCWorker lifecycle, the
handle_telemetry() callback that feeds both the plot and (via
DataLoggingMixin) the CSV logger, and the two PID loops' live
Measured/Error/Output curves, which are plotted in percentage alongside
the raw ADC traces whenever a loop's PID is ON.

Every plotted trace (the 6 ADC channels and the 6 PID % curves) has an
adjustable color via the swatch button next to its checkbox.
"""

import time

import pyqtgraph as pg
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QColor
from PyQt6.QtWidgets import (
    QCheckBox, QColorDialog, QComboBox, QGridLayout, QGroupBox, QHBoxLayout,
    QLabel, QMessageBox, QPushButton, QSpinBox, QVBoxLayout, QWidget,
)

from vdas.core.workers import ADCWorker
from vdas.ui.style import BORDER, danger_button_style, primary_button_style

CHANNEL_COLORS = ["#00e5ff", "#ff4081", "#76ff03", "#ffd600", "#d500f9", "#ff6d00"]

# PID loop -> the ADC channel it drives (loop 0 -> CH0, loop 1 -> CH1),
# reused to color-match each setpoint line to that channel's plot trace.
PID_LOOP_LABELS = {0: "A", 1: "B"}

# Engineering-unit range for each ADC channel mode, taken from the same
# CONF:VOLT (0-5V) / CONF:CURR (4-20mA) ranges used by the hardware. Used
# to convert a Percentage/Manual-mode PID setpoint into a plottable V/mA
# value for the graph's setpoint line, since the graph's y-axis is always
# in engineering units, never in percent.
ADC_ENGINEERING_RANGE = {False: (0.0, 5.0), True: (4.0, 20.0)}  # is_current -> (min, max)

# Default colors for the 6 PID live-percentage curves (2 loops x
# Measured/Error/Output). User-adjustable at runtime via the color swatch
# buttons next to each checkbox - see _make_color_button().
PID_PERCENT_DEFAULT_COLORS = {
    (0, "meas"): "#ffffff",
    (0, "err"): "#ff9100",
    (0, "out"): "#2fbf71",
    (1, "meas"): "#8b93a1",
    (1, "err"): "#ffab40",
    (1, "out"): "#00c8ff",
}
PID_FIELD_LABELS = {"meas": "Measured", "err": "Error", "out": "Output"}


class GraphTabMixin:
    def create_graph_tab(self):
        tab = QWidget()
        root = QHBoxLayout(tab)

        # LEFT: ADC configuration
        left = QVBoxLayout()

        config_box = QGroupBox("ADC Channel Setup & Live Values")
        config_layout = QVBoxLayout(config_box)

        for ch in range(6):
            row = QHBoxLayout()

            lbl_ch = QLabel(f"<b>CH {ch}:</b>")
            lbl_val = QLabel("0.0000 V")
            lbl_val.setStyleSheet("font-family: monospace; font-size: 13px; font-weight: bold;")
            self.lbl_readouts[ch] = lbl_val

            if ch in (0, 1):
                # CH0/CH1 are permanently dedicated to PID Loop A/B and are
                # always read as Current (4-20mA) - there's no combo here
                # on purpose, so the channel can never be left in the
                # wrong mode for the PID loop driving it.
                lbl_mode = QLabel(f"CONF:CURR (4-20mA) — fixed for PID Loop {PID_LOOP_LABELS[ch]}")
                lbl_mode.setStyleSheet("color: #8b93a1; font-weight: 600;")
            else:
                lbl_mode = QLabel("CONF:VOLT (0-5V)")
                lbl_mode.setStyleSheet("color: #888888;")

            row.addWidget(lbl_ch)
            row.addWidget(lbl_mode, 1)
            row.addWidget(lbl_val)
            config_layout.addLayout(row)

        left.addWidget(config_box)

        self.btn_reapply = QPushButton("Re-Apply All ADC Hardware Configurations")
        self.btn_reapply.setFixedHeight(32)
        left.addWidget(self.btn_reapply)

        left.addWidget(self.create_logging_box())
        left.addStretch()

        root.addLayout(left, 1)

        # RIGHT: acquisition + graph
        right = QVBoxLayout()

        acq_box = QGroupBox("Acquisition Settings")
        acq_layout = QHBoxLayout(acq_box)

        self.spin_rate = QSpinBox()
        self.spin_rate.setRange(1, 50)
        self.spin_rate.setValue(10)
        self.spin_rate.setSuffix(" Hz")

        self.spin_window = QSpinBox()
        self.spin_window.setRange(0, 500000)
        self.spin_window.setValue(0)
        self.spin_window.setSpecialValueText("Unlimited")
        self.spin_window.setSuffix(" pts")
        self.spin_window.setToolTip(
            "Caps how many recent samples are kept per channel. Leave at "
            "\"Unlimited\" (0) to retain everything collected this session, "
            "so dragging/zooming the plot always has history to show. Set "
            "a number only if you need to bound memory use on very long runs."
        )

        self.btn_acq_toggle = QPushButton("Start Acquisition")
        self.btn_acq_toggle.setFixedHeight(30)
        self.btn_acq_toggle.setStyleSheet(primary_button_style())

        acq_layout.addWidget(QLabel("Sample Rate:"))
        acq_layout.addWidget(self.spin_rate)
        acq_layout.addWidget(QLabel("History Limit:"))
        acq_layout.addWidget(self.spin_window)
        acq_layout.addWidget(self.btn_acq_toggle)

        right.addWidget(acq_box)

        # --- ADC channel visibility + color -----------------------------
        toggle_layout = QHBoxLayout()
        self.chan_checks = []
        self.curve_colors = {}  # {"ch0"...: hex, (loop,"meas")...: hex}

        for ch in range(6):
            chk = QCheckBox(f"CH{ch}")
            chk.setChecked(True)
            self.chan_checks.append(chk)
            toggle_layout.addWidget(chk)
            toggle_layout.addWidget(self._make_color_button(f"ch{ch}", CHANNEL_COLORS[ch]))

        self.btn_clear_graph = QPushButton("Clear Plot")
        toggle_layout.addWidget(self.btn_clear_graph)

        self.btn_auto_range = QPushButton("Auto Range")
        self.btn_auto_range.setToolTip(
            "Fit the view to everything currently plotted. Nothing you "
            "drag/zoom away from is ever deleted - this just resets what's "
            "in view (use it after panning around to inspect history)."
        )
        self.btn_auto_range.setStyleSheet(primary_button_style())
        toggle_layout.addWidget(self.btn_auto_range)

        right.addLayout(toggle_layout)

        # --- PID live-percentage curve visibility + color ----------------
        pid_curve_box = QGroupBox("PID Loop % Curves (plotted while that loop's PID is ON)")
        pid_curve_layout = QGridLayout()
        self.pid_percent_checks = {0: {}, 1: {}}

        for loop in (0, 1):
            pid_curve_layout.addWidget(QLabel(f"Loop {PID_LOOP_LABELS[loop]}:"), loop, 0)
            col = 1
            for field in ("meas", "err", "out"):
                chk = QCheckBox(PID_FIELD_LABELS[field])
                chk.setChecked(True)
                self.pid_percent_checks[loop][field] = chk
                pid_curve_layout.addWidget(chk, loop, col)
                col += 1
                pid_curve_layout.addWidget(
                    self._make_color_button((loop, field), PID_PERCENT_DEFAULT_COLORS[(loop, field)]),
                    loop, col,
                )
                col += 1

        pid_curve_box.setLayout(pid_curve_layout)
        right.addWidget(pid_curve_box)

        pg.setConfigOption("background", "#181c22")
        pg.setConfigOption("foreground", "#e6e8eb")

        self.plot_widget = pg.PlotWidget(title="Real-Time Telemetry (ADC engineering units + PID %)")
        self.plot_widget.setLabel("left", "Signal Magnitude")
        self.plot_widget.setLabel("bottom", "Time", units="s")
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget.addLegend(offset=(10, 10))

        self.curves = []
        for ch in range(6):
            pen = pg.mkPen(color=CHANNEL_COLORS[ch], width=2)
            curve = self.plot_widget.plot(
                name=f"CH{ch}", pen=pen, autoDownsample=True, clipToView=True
            )
            self.curves.append(curve)

        # PID live-percentage curves - hidden (empty) until that loop's
        # PID goes ON and starts feeding samples (see
        # PIDTabMixin.handle_pid_data / append_pid_percent_sample).
        self.pid_percent_curves = {0: {}, 1: {}}
        for loop in (0, 1):
            for field in ("meas", "err", "out"):
                color = PID_PERCENT_DEFAULT_COLORS[(loop, field)]
                pen = pg.mkPen(color=color, width=2, style=Qt.PenStyle.DotLine)
                curve = self.plot_widget.plot(
                    name=f"Loop {PID_LOOP_LABELS[loop]} {PID_FIELD_LABELS[field]} %",
                    pen=pen, autoDownsample=True, clipToView=True,
                )
                self.pid_percent_curves[loop][field] = curve

        # Horizontal dashed lines marking each PID loop's active setpoint,
        # color-matched to the loop's driven channel (loop 0 -> CH0, loop
        # 1 -> CH1). Hidden until a setpoint is actually set from the PID
        # tab - see update_setpoint_line().
        self.setpoint_lines = {}
        for loop in (0, 1):
            color = CHANNEL_COLORS[loop]
            line = pg.InfiniteLine(
                angle=0,
                movable=False,
                pen=pg.mkPen(color=color, width=1.5, style=Qt.PenStyle.DashLine),
                label=f"Loop {PID_LOOP_LABELS[loop]} SP: {{value:0.3f}}",
                labelOpts={"color": color, "position": 0.95 - (loop * 0.08), "fill": (20, 20, 20, 180)},
            )
            line.setVisible(False)
            self.plot_widget.addItem(line)
            self.setpoint_lines[loop] = line

        right.addWidget(self.plot_widget, 1)
        root.addLayout(right, 2)

        # Signals
        self.btn_reapply.clicked.connect(self.apply_all_configurations)
        self.btn_acq_toggle.clicked.connect(self.toggle_acquisition)
        self.spin_rate.valueChanged.connect(self.update_sampling_rate)
        self.spin_window.valueChanged.connect(self.update_window_depth)
        self.btn_clear_graph.clicked.connect(self.clear_buffers)
        self.btn_auto_range.clicked.connect(self.auto_range_plot)

        return tab

    # -------------------------------------------------------------------
    # PER-CURVE COLOR PICKER
    # -------------------------------------------------------------------
    def _make_color_button(self, curve_key, default_hex: str) -> QPushButton:
        """
        Small square swatch button. curve_key is either "ch0".."ch5" (an
        ADC channel) or a (loop, field) tuple (a PID % curve) - it's just
        used to look up which pg curve to re-pen when a new color is
        picked, and to remember the choice in self.curve_colors.
        """
        btn = QPushButton()
        btn.setFixedSize(20, 20)
        btn.setToolTip("Choose plot color")
        btn.setStyleSheet(f"background-color: {default_hex}; border: 1px solid {BORDER}; border-radius: 4px;")
        self.curve_colors[curve_key] = default_hex
        btn.clicked.connect(lambda: self.pick_curve_color(curve_key, btn))
        return btn

    def pick_curve_color(self, curve_key, button: QPushButton):
        current_hex = self.curve_colors.get(curve_key, "#ffffff")
        chosen = QColorDialog.getColor(QColor(current_hex), self, "Choose Plot Color")
        if not chosen.isValid():
            return

        hex_color = chosen.name()
        self.curve_colors[curve_key] = hex_color
        button.setStyleSheet(f"background-color: {hex_color}; border: 1px solid {BORDER}; border-radius: 4px;")

        if isinstance(curve_key, str) and curve_key.startswith("ch"):
            ch = int(curve_key[2:])
            self.curves[ch].setPen(pg.mkPen(color=hex_color, width=2))
        else:
            loop, field = curve_key
            self.pid_percent_curves[loop][field].setPen(
                pg.mkPen(color=hex_color, width=2, style=Qt.PenStyle.DotLine)
            )

    # -------------------------------------------------------------------
    # ADC CONFIGURATION
    # -------------------------------------------------------------------
    def sync_adc_channel_mode(self, ch: int, is_current: bool, apply_to_hardware: bool = True):
        """
        Pushes CONF:VOLT/CONF:CURR for a channel to the hardware (and to
        the running ADCWorker, if any) and keeps self.channel_modes in
        sync. CH0/CH1 are always called with is_current=True (from
        PIDTabMixin.update_pid) since they're permanently dedicated to the
        two PID loops; this still goes through the same settle-cycle
        bookkeeping as any other mode change.
        """
        self.channel_modes[ch] = is_current

        response = None
        if apply_to_hardware and self.scpi.is_connected():
            response = self.scpi.config_adc_channel(ch, is_current)
            # Front-end just switched mode on this channel - ignore the
            # next few poll cycles' readings for it so a mid-settle
            # transient can't briefly show up on the graph as a glitch.
            self.adc_mode_settle_cycles[ch] = self.ADC_MODE_SETTLE_CYCLES

        if self.adc_worker:
            self.adc_worker.update_modes(self.channel_modes)

        if ch in self.mode_combos:
            combo = self.mode_combos[ch]
            combo.blockSignals(True)
            combo.setCurrentIndex(1 if is_current else 0)
            combo.blockSignals(False)

        return response

    def percent_to_engineering(self, is_current: bool, percent: float) -> float:
        """
        Converts a 0-100 Percentage/Manual-mode PID setpoint into the
        equivalent engineering-unit value (V or mA) using the channel's
        fixed CONF:VOLT (0-5V) / CONF:CURR (4-20mA) range, so a setpoint
        can still be drawn as a line on the graph's V/mA axis.
        """
        lo, hi = ADC_ENGINEERING_RANGE[is_current]
        percent = max(0.0, min(100.0, percent))
        return lo + (percent / 100.0) * (hi - lo)

    def update_setpoint_line(self, loop: int, value):
        """
        Show/move/hide the dashed horizontal setpoint marker for a PID
        loop on the live graph. Called from PIDTabMixin.update_pid() so
        pressing UPDATE with a setpoint gives an immediate visual
        reference against that loop's channel trace. Pass value=None to
        hide the line.
        """
        line = self.setpoint_lines.get(loop)
        if line is None:
            return

        if value is None:
            line.setVisible(False)
            return

        line.setPos(value)
        line.setVisible(True)

    def apply_all_configurations(self):
        if not self.scpi.is_connected():
            return

        for ch in range(6):
            is_current = self.channel_modes.get(ch, False)
            self.scpi.config_adc_channel(ch, is_current)
            time.sleep(0.02)

        self.status_bar.showMessage("All 6 ADC channels configured.")

    # -------------------------------------------------------------------
    # ADC ACQUISITION (worker thread)
    # -------------------------------------------------------------------
    def toggle_acquisition(self):
        if not self.scpi.is_connected():
            QMessageBox.warning(self, "Warning", "Connect to hardware first.")
            return

        if self.adc_worker and self.adc_worker.isRunning():
            self.stop_adc_worker()
            self.btn_acq_toggle.setText("Start Acquisition")
            self.btn_acq_toggle.setStyleSheet(primary_button_style())
        else:
            # Resume the plot's time axis from where it left off (rather
            # than jumping back to t=0) whenever there's already data in
            # the buffers - i.e. Stop then Start Acquisition continues the
            # same trace. Clear Plot (clear_buffers) empties time_buffer,
            # so the next start after that correctly begins at t=0 again.
            resume_offset = self.time_buffer[-1] if self.time_buffer else 0.0

            self.adc_worker = ADCWorker(self.scpi, self.channel_modes, start_offset=resume_offset)
            self.adc_worker.set_sampling_rate(self.spin_rate.value())
            self.adc_worker.data_received.connect(self.handle_telemetry)
            self.adc_worker.start()

            # Same start-time reference the ADCWorker itself uses, so the
            # PID % curves (fed on their own async cadence in
            # PIDTabMixin.handle_pid_data) land on the same elapsed-seconds
            # x-axis as the ADC traces above.
            self.acq_start_wall_time = time.time() - resume_offset

            self.btn_acq_toggle.setText("Stop Acquisition")
            self.btn_acq_toggle.setStyleSheet(danger_button_style())

    def stop_adc_worker(self):
        if self.adc_worker:
            self.adc_worker.stop()
            self.adc_worker.wait()
            self.adc_worker = None

    def update_sampling_rate(self, value: int):
        if self.adc_worker:
            self.adc_worker.set_sampling_rate(value)

    def update_window_depth(self, value: int):
        # 0 means "Unlimited" (see spin_window.setSpecialValueText) - no
        # trimming is applied, so every sample collected this session stays
        # in the buffers and dragging/zooming the plot always has history
        # to show. A positive value caps each buffer to that many most
        # recent samples, for anyone who wants to bound memory on very
        # long runs.
        self.max_pts = value

    def clear_buffers(self):
        self.time_buffer.clear()
        for buffer in self.adc_buffers:
            buffer.clear()
        for curve in self.curves:
            curve.setData([], [])

        for loop in (0, 1):
            for field in ("meas", "err", "out"):
                self.pid_percent_buffers[loop][field].clear()
                self.pid_percent_curves[loop][field].setData([], [])
            self.pid_percent_buffers[loop]["t"].clear()

    def auto_range_plot(self):
        """
        Fits the view to everything currently plotted and resumes
        automatic fitting going forward. Dragging or zooming the plot only
        changes what's in view - it never deletes data - so this is always
        safe to click to get back to seeing the full trace after scrolling
        around to inspect an earlier section.
        """
        view_box = self.plot_widget.getViewBox()
        view_box.enableAutoRange(x=True, y=True)
        self.plot_widget.autoRange()

    def handle_telemetry(self, timestamp: float, readings: dict):
        self.time_buffer.append(timestamp)

        if self.max_pts > 0 and len(self.time_buffer) > self.max_pts:
            self.time_buffer.pop(0)

        for ch in range(6):
            settling = self.adc_mode_settle_cycles.get(ch, 0) > 0

            if ch in readings and not settling:
                value, is_current = readings[ch]
                self.last_adc_values[ch] = value
                self.last_adc_is_current[ch] = is_current
            else:
                # No fresh (verified) reading this cycle, or the channel
                # just switched mode and is still settling - hold the last
                # known value instead of dropping the display/graph to
                # zero or showing a mid-switch transient.
                value = self.last_adc_values[ch]
                is_current = self.last_adc_is_current[ch]

                if settling and ch in readings:
                    # A reading did come back during the settle window -
                    # count the cycle down but don't trust the value yet.
                    self.adc_mode_settle_cycles[ch] -= 1

            unit = "mA" if is_current else "V"
            text = f"{value:.4f} {unit}"

            self.lbl_readouts[ch].setText(text)

            self.adc_buffers[ch].append(value)
            if self.max_pts > 0 and len(self.adc_buffers[ch]) > self.max_pts:
                self.adc_buffers[ch].pop(0)

            if self.chan_checks[ch].isChecked():
                self.curves[ch].setData(self.time_buffer, self.adc_buffers[ch])
            else:
                self.curves[ch].setData([], [])

        if self.is_logging:
            self.write_log_row(timestamp)

    # -------------------------------------------------------------------
    # PID LIVE-PERCENTAGE CURVES (fed from PIDTabMixin.handle_pid_data)
    # -------------------------------------------------------------------
    def append_pid_percent_sample(self, loop: int, meas_pct, err_pct, out_pct):
        """
        Appends one point to loop's Measured/Error/Output % curves and
        redraws them, but only while acquisition is running - PID data
        arrives on its own ~2 Hz cadence independent of the ADC worker, so
        this uses the same wall-clock reference (self.acq_start_wall_time)
        established in toggle_acquisition() to keep it on the same x-axis
        as the ADC traces.

        Called from PIDTabMixin.handle_pid_data only when the loop's PID
        is actually ON and all three percentages resolved to real numbers
        this cycle; pass a sample only when all three are available so
        every per-field buffer always stays the same length as buf["t"]
        (no partial/misaligned points).
        """
        if meas_pct is None or err_pct is None or out_pct is None:
            return
        if not (self.adc_worker and self.adc_worker.isRunning()) or self.acq_start_wall_time is None:
            return

        elapsed = time.time() - self.acq_start_wall_time
        buf = self.pid_percent_buffers[loop]

        buf["t"].append(elapsed)
        buf["meas"].append(meas_pct)
        buf["err"].append(err_pct)
        buf["out"].append(out_pct)

        if self.max_pts > 0:
            for key in ("t", "meas", "err", "out"):
                while len(buf[key]) > self.max_pts:
                    buf[key].pop(0)

        for field in ("meas", "err", "out"):
            if self.pid_percent_checks[loop][field].isChecked():
                self.pid_percent_curves[loop][field].setData(buf["t"], buf[field])
            else:
                self.pid_percent_curves[loop][field].setData([], [])
