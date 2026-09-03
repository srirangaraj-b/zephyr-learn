"""
vdas.ui.tabs.logging_panel
=============================

The "Data Logging (CSV)" panel shown inside the Graph tab, plus the
start/stop/write-row logic behind it. Extracted unchanged from the
original application.
"""

import csv
import os
import time
from datetime import datetime

from PyQt6.QtWidgets import (
    QFileDialog, QGroupBox, QHBoxLayout, QLabel, QLineEdit, QMessageBox,
    QPushButton, QVBoxLayout,
)

from vdas.ui.style import TEXT_DIM, danger_button_style, success_button_style


class DataLoggingMixin:
    def create_logging_box(self):
        box = QGroupBox("Data Logging (CSV)")
        layout = QVBoxLayout()

        # Where to store
        file_row = QHBoxLayout()
        self.log_path_edit = QLineEdit()
        self.log_path_edit.setPlaceholderText("Choose a .csv file to log to...")
        self.log_path_edit.setReadOnly(True)

        self.btn_log_browse = QPushButton("Browse...")
        self.btn_log_browse.setFixedWidth(90)

        file_row.addWidget(QLabel("Log File:"))
        file_row.addWidget(self.log_path_edit, 1)
        file_row.addWidget(self.btn_log_browse)
        layout.addLayout(file_row)

        # When to start / when to end
        button_row = QHBoxLayout()
        self.btn_log_start = QPushButton("Start Logging")
        self.btn_log_start.setFixedHeight(32)
        self.btn_log_start.setStyleSheet(success_button_style())

        self.btn_log_stop = QPushButton("Stop Logging")
        self.btn_log_stop.setFixedHeight(32)
        self.btn_log_stop.setStyleSheet(danger_button_style())
        self.btn_log_stop.setEnabled(False)

        button_row.addWidget(self.btn_log_start)
        button_row.addWidget(self.btn_log_stop)
        layout.addLayout(button_row)

        # Status
        self.lbl_log_status = QLabel("Not logging.")
        self.lbl_log_status.setStyleSheet(f"color: {TEXT_DIM};")
        layout.addWidget(self.lbl_log_status)

        info = QLabel(
            "Logs a row per ADC sample: all 6 channel values, plus both PID "
            "loops' mode/setpoint%/Kp/Ki/Kd and live status/measured%/error%/output%."
        )
        info.setWordWrap(True)
        info.setStyleSheet(f"color: {TEXT_DIM}; font-size: 11px;")
        layout.addWidget(info)

        box.setLayout(layout)

        self.btn_log_browse.clicked.connect(self.browse_log_file)
        self.btn_log_start.clicked.connect(self.start_logging)
        self.btn_log_stop.clicked.connect(self.stop_logging)

        return box

    def browse_log_file(self):
        default_name = time.strftime("vdas_log_%Y%m%d_%H%M%S.csv")
        path, _ = QFileDialog.getSaveFileName(
            self, "Choose CSV Log File", default_name, "CSV Files (*.csv)"
        )
        if path:
            if not path.lower().endswith(".csv"):
                path += ".csv"
            self.log_path_edit.setText(path)

    def start_logging(self):
        if self.is_logging:
            return

        path = self.log_path_edit.text().strip()
        if not path:
            QMessageBox.warning(self, "No File Selected", "Choose a CSV file to log to first.")
            return

        try:
            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            self.log_file_handle = open(path, mode="w", newline="", encoding="utf-8")
            self.log_csv_writer = csv.writer(self.log_file_handle)
            self.log_csv_writer.writerow([
                "Timestamp",
                "CH0_Value", "CH0_Unit",
                "CH1_Value", "CH1_Unit",
                "CH2_Value", "CH2_Unit",
                "CH3_Value", "CH3_Unit",
                "CH4_Value", "CH4_Unit",
                "CH5_Value", "CH5_Unit",
                "PID_A_Status", "PID_A_Mode", "PID_A_Setpoint_Pct",
                "PID_A_Kp", "PID_A_Ki", "PID_A_Kd",
                "PID_A_Measured_Pct", "PID_A_Error_Pct", "PID_A_Output_Pct",
                "PID_B_Status", "PID_B_Mode", "PID_B_Setpoint_Pct",
                "PID_B_Kp", "PID_B_Ki", "PID_B_Kd",
                "PID_B_Measured_Pct", "PID_B_Error_Pct", "PID_B_Output_Pct",
            ])
            self.log_file_handle.flush()
        except OSError as e:
            QMessageBox.critical(self, "Log File Error", f"Could not open log file:\n{e}")
            self.log_file_handle = None
            self.log_csv_writer = None
            return

        self.log_file_path = path
        self.log_row_count = 0
        self.log_start_time = time.time()
        self.is_logging = True

        self.btn_log_start.setEnabled(False)
        self.btn_log_stop.setEnabled(True)
        self.btn_log_browse.setEnabled(False)
        self.lbl_log_status.setStyleSheet("color: #2fbf71; font-weight: bold;")
        self.lbl_log_status.setText(f"Logging to {os.path.basename(path)} — 0 rows")

        self.log_message(f"Data logging started -> {path}")

    def stop_logging(self):
        if not self.is_logging:
            return

        self.is_logging = False

        if self.log_file_handle:
            try:
                self.log_file_handle.flush()
                self.log_file_handle.close()
            except OSError:
                pass

        self.log_file_handle = None
        self.log_csv_writer = None

        self.btn_log_start.setEnabled(True)
        self.btn_log_stop.setEnabled(False)
        self.btn_log_browse.setEnabled(True)
        self.lbl_log_status.setStyleSheet(f"color: {TEXT_DIM};")
        self.lbl_log_status.setText(f"Stopped. {self.log_row_count} rows written to {os.path.basename(self.log_file_path)}")

        self.log_message(f"Data logging stopped ({self.log_row_count} rows written).")

    def write_log_row(self, timestamp: float):
        """
        Writes one CSV row using the values currently held/displayed for
        the 6 ADC channels (self.last_adc_values / self.last_adc_is_current
        - the same numbers on screen, including hold-last-good values) and
        the most recently known PID data for both loops. Configured PID
        variables (mode/setpoint/Kp/Ki/Kd) are read live from the PID tab's
        input widgets so the log reflects what's actually configured right
        now, not just the last value pushed to the device.

        The logged timestamp is the PC's wall-clock/system time at the
        moment the row is written (not the acquisition-elapsed seconds
        used for the graph's x-axis), so log rows carry an absolute,
        real-world time that's directly comparable across sessions and
        across separate log files. The `timestamp` parameter (acquisition-
        elapsed seconds, passed in from handle_telemetry) is accepted for
        call-signature compatibility but intentionally unused here.
        """
        if not self.is_logging or not self.log_csv_writer:
            return

        system_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        row = [system_time]

        for ch in range(6):
            value = self.last_adc_values[ch]
            unit = "mA" if self.last_adc_is_current[ch] else "V"
            row.extend([f"{value:.4f}", unit])

        for loop in (0, 1):
            snapshot = self.last_pid_snapshot.get(loop, {})
            inputs = self.pid_inputs.get(loop)

            if inputs:
                mode = inputs["mode"].currentText()
                setpoint = inputs["setpoint"].text()
                kp = inputs["kp"].text()
                ki = inputs["ki"].text()
                kd = inputs["kd"].text()
            else:
                mode = setpoint = kp = ki = kd = "---"

            row.extend([
                snapshot.get("status", "---"),
                mode,
                setpoint,
                kp, ki, kd,
                snapshot.get("meas", "---"),
                snapshot.get("err", "---"),
                snapshot.get("out", "---"),
            ])

        try:
            self.log_csv_writer.writerow(row)
            self.log_row_count += 1

            # Flush periodically rather than every row, to keep 10+ Hz
            # logging from hammering the disk on every single sample.
            if self.log_row_count % 10 == 0:
                self.log_file_handle.flush()

            self.lbl_log_status.setText(
                f"Logging to {os.path.basename(self.log_file_path)} — {self.log_row_count} rows"
            )
        except OSError as e:
            self.log_message(f"! Log write error: {e}")
            self.stop_logging()
