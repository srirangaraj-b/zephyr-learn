"""
vdas.ui.main_window
======================

VDASApplication is the single QMainWindow that ties everything together:
the connection panel, the SAFE SHUTDOWN button, and the tabs
(Graph, PID, Diagnostics).

Dashboard and DAC are no longer standalone tabs: CH0/CH1 are permanently
configured as Current (4-20mA) input channels dedicated to the two PID
loops, and DAC channels 0/1 are driven directly from the PID tab (Manual
mode) instead of through a separate DAC panel.

All the actual tab-building and event-handling code lives in the mixins
under vdas.ui.tabs / vdas.ui.widgets - this file only owns the shared
instance state (buffers, caches, registries) and wires the top-level
layout together.
"""

from PyQt6.QtWidgets import (
    QMainWindow, QPushButton, QStatusBar, QTabWidget, QVBoxLayout, QWidget,
)

from vdas.core.scpi_controller import SCPIController
from vdas.ui.style import danger_button_style
from vdas.ui.tabs.connection import ConnectionMixin
from vdas.ui.tabs.diagnostics import DiagnosticsTabMixin
from vdas.ui.tabs.graph import GraphTabMixin
from vdas.ui.tabs.logging_panel import DataLoggingMixin
from vdas.ui.tabs.pid import PIDTabMixin
from vdas.ui.tabs.safety import SafetyMixin


class VDASApplication(
    QMainWindow,
    ConnectionMixin,
    GraphTabMixin,
    DataLoggingMixin,
    PIDTabMixin,
    DiagnosticsTabMixin,
    SafetyMixin,
):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("VDAS Unified Control Application")
        self.resize(1450, 860)

        self.scpi = SCPIController()
        self.scpi.logger.message.connect(self.log_message)

        self.adc_worker = None
        self.pid_worker = None

        # ADC state. CH0 and CH1 are permanently dedicated to the two PID
        # loops and are always in Current (4-20mA) mode - there is no
        # user-facing way to switch them back to Voltage, since the PID
        # loops assume a 4-20mA process input. CH2-CH5 remain plain
        # Voltage monitoring channels.
        self.channel_modes = {0: True, 1: True, 2: False, 3: False, 4: False, 5: False}
        # 0 = unlimited (see GraphTabMixin.update_window_depth) - by default
        # every sample collected this session is retained, so dragging or
        # zooming the plot always has history to show instead of it having
        # been discarded.
        self.max_pts = 0
        self.time_buffer = []
        self.adc_buffers = [[] for _ in range(6)]

        # Wall-clock time acquisition last (re)started at, used to give
        # the PID percentage curves (see GraphTabMixin) an x-axis that
        # lines up with the ADC curves' elapsed-seconds axis even though
        # PID samples arrive on their own, slower, asynchronous cadence.
        self.acq_start_wall_time = None

        # Last known-good ADC reading per channel, held over whenever a poll
        # cycle returns no data for that channel (see fetch_all_telemetry).
        self.last_adc_values = [0.0] * 6
        self.last_adc_is_current = [False] * 6

        # Number of upcoming poll cycles to hold-and-ignore for a channel
        # right after its CONF:VOLT/CONF:CURR mode is switched. The ADC
        # front-end needs a moment to settle after a mode change; without
        # this, the first sample or two read in the new mode can be a
        # transient (e.g. a stale/mid-switch value), which briefly reads
        # like a glitch on the graph right at the switch point.
        self.adc_mode_settle_cycles = {ch: 0 for ch in range(6)}
        self.ADC_MODE_SETTLE_CYCLES = 3

        # Widget registries (filled per-tab, referenced by shared update handlers)
        self.mode_combos = {}
        self.lbl_readouts = {}          # Graph tab live values
        self.pid_monitor_labels = {}    # PID tab monitor labels {loop: {field: label}}
        self.pid_inputs = {}            # PID tab input widgets {loop: {mode, setpoint, kp, ki, kd, unit_label, on_button, off_button}}

        # Live PID percentage buffers, fed by handle_pid_data and plotted
        # on the Graph tab whenever that loop's PID is ON (see
        # GraphTabMixin.append_pid_percent_sample). Everything here is
        # already expressed as 0-100%, having been converted from the raw
        # engineering-unit reply on the wire (4-20mA for measured/error,
        # 0-5V DAC drive for output).
        self.pid_percent_buffers = {
            0: {"t": [], "meas": [], "err": [], "out": []},
            1: {"t": [], "meas": [], "err": [], "out": []},
        }

        # --------------------------------------------------------------
        # CSV DATA LOGGING STATE
        # --------------------------------------------------------------
        # Last known PID snapshot per loop, already resolved to percentage
        # strings - updated in handle_pid_data and used both to drive the
        # Live PID Monitor labels and to fill each logged CSV row, so a
        # row isn't blocked on the PID worker's slower poll cadence and
        # always matches what's on screen.
        self.last_pid_snapshot = {
            0: {"status": "---", "meas": "---", "err": "---", "out": "---"},
            1: {"status": "---", "meas": "---", "err": "---", "out": "---"},
        }

        # Holds the last successfully-read raw value per field/loop, used
        # to bridge over a transient poll miss (comm error / echo
        # mismatch) so the display doesn't flash to "---" just because one
        # read on the wire failed - it only shows "---" when the loop is
        # actually OFF. None means "never received a good value yet".
        self.pid_hold_cache = {
            0: {"status": None, "meas": None, "err": None, "out": None},
            1: {"status": None, "meas": None, "err": None, "out": None},
        }

        self.is_logging = False
        self.log_file_handle = None
        self.log_csv_writer = None
        self.log_file_path = ""
        self.log_row_count = 0
        self.log_start_time = None

        self.init_ui()

    # -------------------------------------------------------------------
    # TOP-LEVEL UI
    # -------------------------------------------------------------------
    def init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(10)

        # Status bar is created before the connection panel because
        # create_connection_panel() calls populate_ports(), which writes
        # its first message to self.status_bar.
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Disconnected. Select a port and connect.")

        # Global connection panel (built by ConnectionMixin; includes the
        # logo and the *IDN? identification fields)
        root.addWidget(self.create_connection_panel())

        # Tabs
        self.tabs = QTabWidget()
        self.tabs.addTab(self.create_graph_tab(), "Graph (ADC Scope)")
        self.tabs.addTab(self.create_pid_tab(), "PID")
        self.tabs.addTab(self.create_diagnostics_tab(), "Diagnostics")
        root.addWidget(self.tabs, 1)

        # Global safety button
        self.btn_shutdown_all = QPushButton("SAFE SHUTDOWN: DISABLE ALL PID LOOPS + DAC OUTPUTS")
        self.btn_shutdown_all.setFixedHeight(42)
        self.btn_shutdown_all.setStyleSheet(danger_button_style() + " font-size: 14px;")
        root.addWidget(self.btn_shutdown_all)

        self.btn_shutdown_all.clicked.connect(self.shutdown_all)
