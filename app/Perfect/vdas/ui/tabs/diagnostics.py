"""
vdas.ui.tabs.diagnostics
===========================

The "Diagnostics" tab: the live SCPI TX/RX log, plus a manual *IDN? /
custom-command sender. Unchanged from the original application.
"""

import time

from PyQt6.QtGui import QFont
from PyQt6.QtWidgets import (
    QGroupBox, QHBoxLayout, QLabel, QLineEdit, QMessageBox, QPushButton,
    QTextEdit, QVBoxLayout, QWidget,
)


class DiagnosticsTabMixin:
    def create_diagnostics_tab(self):
        tab = QWidget()
        root = QVBoxLayout(tab)

        log_box = QGroupBox("SCPI Communication Log")
        log_layout = QVBoxLayout()

        self.log = QTextEdit()
        self.log.setReadOnly(True)
        self.log.setFont(QFont("Consolas", 9))
        log_layout.addWidget(self.log)

        clear_log_button = QPushButton("Clear Log")
        clear_log_button.clicked.connect(self.log.clear)
        log_layout.addWidget(clear_log_button)

        log_box.setLayout(log_layout)
        root.addWidget(log_box, 1)

        tools_box = QGroupBox("Diagnostic Tools")
        tools_layout = QHBoxLayout()

        btn_idn = QPushButton("Query *IDN?")
        btn_idn.clicked.connect(lambda: self.send_diagnostic_command("*IDN?"))
        tools_layout.addWidget(btn_idn)

        tools_layout.addWidget(QLabel("Custom command:"))
        self.custom_cmd_edit = QLineEdit()
        self.custom_cmd_edit.setPlaceholderText("e.g. MEAS:VOLT:ALL?")
        tools_layout.addWidget(self.custom_cmd_edit, 1)

        btn_send_custom = QPushButton("Send")
        btn_send_custom.clicked.connect(
            lambda: self.send_diagnostic_command(self.custom_cmd_edit.text())
        )
        tools_layout.addWidget(btn_send_custom)

        tools_box.setLayout(tools_layout)
        root.addWidget(tools_box)

        return tab

    def send_diagnostic_command(self, command):
        if not command:
            return
        if not self.scpi.is_connected():
            QMessageBox.warning(self, "Not Connected", "Connect to the VDAS first.")
            return
        self.scpi.send_cmd(command)

    # -------------------------------------------------------------------
    # LOGGING
    # -------------------------------------------------------------------
    def log_message(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.log.append(f"[{timestamp}] {message}")
        scrollbar = self.log.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())
