"""
vdas.ui.tabs.connection
=========================

Builds the "Hardware Connection" panel at the top of the main window and
handles connect/disconnect.

New in this version: on connect, the app sends *IDN? and parses the
echo-verified reply

    *IDN?->(VIMICRO,VDAS,01,3.3)

into Company / Product / Model / Firmware fields, which are shown in this
panel, preceded by the system logo. If the identification query fails or
doesn't parse (bad echo, wrong field count), the fields show "---" rather
than a guess, and the mismatch is logged to Diagnostics via the existing
SCPILogger. Every other behavior here (ports, baud rate, the
safe-shutdown-on-disconnect sequence) is unchanged from the original
application.
"""

import os

import serial.tools.list_ports
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QIntValidator, QPixmap
from PyQt6.QtWidgets import (
    QComboBox, QFrame, QGroupBox, QHBoxLayout, QLabel, QLineEdit,
    QMessageBox, QPushButton, QStackedWidget, QVBoxLayout, QWidget,
)

from vdas.ui.style import GREEN, RED, TEXT_DIM

# Logo shown at the start of the identification row. Drop a replacement
# PNG/SVG-exported-as-PNG in this location (vdas/ui/assets/logo.png) to
# rebrand the app - no code changes needed as long as the filename matches.
_LOGO_PATH = os.path.join(os.path.dirname(os.path.dirname(__file__)), "assets", "logo.png")


class ConnectionMixin:
    """Mixed into VDASApplication. Expects self.scpi, self.log_message,
    self.status_bar, and (once created) self.tabs / worker start-stop
    helpers from the other mixins."""

    def create_connection_panel(self) -> QGroupBox:
        conn_box = QGroupBox("Hardware Connection")
        outer = QVBoxLayout(conn_box)
        outer.setSpacing(10)

        # --- Row 1: interface / port-or-host / connect button / status ---
        conn_layout = QHBoxLayout()

        self.transport_combo = QComboBox()
        self.transport_combo.addItems(["Serial", "TCP/IP"])

        # --- Serial page ------------------------------------------------
        serial_page = QWidget()
        serial_layout = QHBoxLayout(serial_page)
        serial_layout.setContentsMargins(0, 0, 0, 0)

        self.combo_ports = QComboBox()
        self.btn_refresh = QPushButton("Refresh Ports")

        self.baud_combo = QComboBox()
        self.baud_combo.addItems(
            ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
        )
        self.baud_combo.setCurrentText("115200")

        serial_layout.addWidget(QLabel("Port:"))
        serial_layout.addWidget(self.combo_ports, 1)
        serial_layout.addWidget(self.btn_refresh)
        serial_layout.addWidget(QLabel("Baud:"))
        serial_layout.addWidget(self.baud_combo)

        # --- TCP/IP page --------------------------------------------------
        tcp_page = QWidget()
        tcp_layout = QHBoxLayout(tcp_page)
        tcp_layout.setContentsMargins(0, 0, 0, 0)

        self.edit_tcp_host = QLineEdit()
        self.edit_tcp_host.setPlaceholderText("e.g. 192.168.1.50")

        self.edit_tcp_port = QLineEdit("5025")
        self.edit_tcp_port.setValidator(QIntValidator(1, 65535))
        self.edit_tcp_port.setFixedWidth(70)

        tcp_layout.addWidget(QLabel("Host / IP:"))
        tcp_layout.addWidget(self.edit_tcp_host, 1)
        tcp_layout.addWidget(QLabel("Port:"))
        tcp_layout.addWidget(self.edit_tcp_port)

        self.transport_stack = QStackedWidget()
        self.transport_stack.addWidget(serial_page)
        self.transport_stack.addWidget(tcp_page)
        self.transport_combo.currentIndexChanged.connect(self.transport_stack.setCurrentIndex)

        self.btn_connect = QPushButton("Connect Hardware")
        self.btn_connect.setStyleSheet("font-weight: bold;")

        self.connection_status = QLabel("● Disconnected")
        self.connection_status.setStyleSheet(f"color: {RED}; font-weight: bold;")

        conn_layout.addWidget(QLabel("Interface:"))
        conn_layout.addWidget(self.transport_combo)
        conn_layout.addWidget(self.transport_stack, 1)
        conn_layout.addWidget(self.btn_connect)
        conn_layout.addWidget(self.connection_status)

        outer.addLayout(conn_layout)

        # --- Divider ----------------------------------------------------
        divider = QFrame()
        divider.setFrameShape(QFrame.Shape.HLine)
        outer.addWidget(divider)

        # --- Row 2: logo + device identification (from *IDN?) ------------
        idn_layout = QHBoxLayout()
        idn_layout.setSpacing(18)

        idn_layout.addWidget(self._create_logo_label())

        logo_divider = QFrame()
        logo_divider.setFrameShape(QFrame.Shape.VLine)
        idn_layout.addWidget(logo_divider)

        idn_layout.addWidget(self._idn_field_label("Company:"))
        self.lbl_idn_company = self._idn_value_label()
        idn_layout.addWidget(self.lbl_idn_company)

        idn_layout.addWidget(self._idn_field_label("Product:"))
        self.lbl_idn_product = self._idn_value_label()
        idn_layout.addWidget(self.lbl_idn_product)

        idn_layout.addWidget(self._idn_field_label("Model:"))
        self.lbl_idn_model = self._idn_value_label()
        idn_layout.addWidget(self.lbl_idn_model)

        idn_layout.addWidget(self._idn_field_label("Firmware:"))
        self.lbl_idn_firmware = self._idn_value_label()
        idn_layout.addWidget(self.lbl_idn_firmware)

        idn_layout.addStretch()
        outer.addLayout(idn_layout)

        self.btn_refresh.clicked.connect(self.populate_ports)
        self.btn_connect.clicked.connect(self.toggle_connection)

        self.populate_ports()
        self.clear_identity_display()

        return conn_box

    @staticmethod
    def _create_logo_label() -> QLabel:
        """Loads vdas/ui/assets/logo.png and returns it scaled to a fixed
        panel height. Falls back to a plain text mark if the asset is
        missing so a deleted/renamed logo file never crashes the app."""
        lbl = QLabel()
        pixmap = QPixmap(_LOGO_PATH)
        if not pixmap.isNull():
            lbl.setPixmap(
                pixmap.scaledToHeight(40, Qt.TransformationMode.SmoothTransformation)
            )
        else:
            lbl.setText("VDAS")
            lbl.setStyleSheet("font-size: 18px; font-weight: 800; color: #2f8fff;")
        return lbl

    @staticmethod
    def _idn_field_label(text: str) -> QLabel:
        lbl = QLabel(text)
        lbl.setStyleSheet(f"color: {TEXT_DIM}; font-weight: 600;")
        return lbl

    @staticmethod
    def _idn_value_label() -> QLabel:
        lbl = QLabel("---")
        lbl.setProperty("valueLabel", True)
        lbl.setMinimumWidth(90)
        return lbl

    # -------------------------------------------------------------------
    # IDENTIFICATION
    # -------------------------------------------------------------------
    def clear_identity_display(self):
        for lbl in (
            self.lbl_idn_company,
            self.lbl_idn_product,
            self.lbl_idn_model,
            self.lbl_idn_firmware,
        ):
            lbl.setText("---")

    def refresh_identity_display(self):
        """Sends *IDN? and updates the Company/Product/Model/Firmware
        fields. Called once right after a successful connection."""
        idn = self.scpi.query_idn()

        if not idn:
            self.clear_identity_display()
            self.log_message("! Could not read device identification (*IDN?).")
            return

        self.lbl_idn_company.setText(idn["company"])
        self.lbl_idn_product.setText(idn["product"])
        self.lbl_idn_model.setText(idn["model"])
        self.lbl_idn_firmware.setText(idn["firmware"])
        self.log_message(f"Device identified: {idn['raw']}")

    # -------------------------------------------------------------------
    # SERIAL PORT / CONNECTION
    # -------------------------------------------------------------------
    def populate_ports(self):
        self.combo_ports.clear()
        ports = [p.device for p in serial.tools.list_ports.comports()]

        if ports:
            self.combo_ports.addItems(ports)
            self.status_bar.showMessage(f"Found {len(ports)} serial port(s).")
        else:
            self.status_bar.showMessage("No active serial ports found.")

    def toggle_connection(self):
        if self.scpi.is_connected():
            self.stop_adc_worker()
            self.stop_pid_worker()
            self.shutdown_all()

            if self.is_logging:
                self.stop_logging()

            self.scpi.disconnect()

            self.btn_connect.setText("Connect Hardware")
            self.transport_combo.setEnabled(True)
            self.combo_ports.setEnabled(True)
            self.baud_combo.setEnabled(True)
            self.edit_tcp_host.setEnabled(True)
            self.edit_tcp_port.setEnabled(True)
            self.set_connected_state(False)
            self.clear_identity_display()

            self.status_bar.showMessage("Hardware disconnected.")
            return

        use_tcp = self.transport_combo.currentIndex() == 1

        if use_tcp:
            host = self.edit_tcp_host.text().strip()
            port_text = self.edit_tcp_port.text().strip()

            if not host or not port_text:
                QMessageBox.warning(self, "TCP Error", "Enter a host/IP and port.")
                return

            try:
                port_num = int(port_text)
            except ValueError:
                QMessageBox.warning(self, "TCP Error", "Port must be numeric.")
                return

            connected = self.scpi.connect_tcp(host, port_num)
            target_desc = f"{host}:{port_num} (TCP)"
        else:
            port = self.combo_ports.currentText()

            if not port:
                QMessageBox.warning(self, "Port Error", "No serial port selected.")
                return

            baud = int(self.baud_combo.currentText())
            connected = self.scpi.connect(port, baud)
            target_desc = f"{port} @ {baud} baud"

        if connected:
            self.btn_connect.setText("Disconnect Hardware")
            self.transport_combo.setEnabled(False)
            self.combo_ports.setEnabled(False)
            self.baud_combo.setEnabled(False)
            self.edit_tcp_host.setEnabled(False)
            self.edit_tcp_port.setEnabled(False)
            self.set_connected_state(True)

            self.status_bar.showMessage(f"Connected to {target_desc}.")

            self.refresh_identity_display()

            self.apply_all_configurations()
            self.start_pid_worker()
        else:
            QMessageBox.critical(self, "Connection Error", f"Failed to connect to {target_desc}.")

    def set_connected_state(self, connected):
        if connected:
            self.connection_status.setText("● Connected")
            self.connection_status.setStyleSheet(f"color: {GREEN}; font-weight: bold;")
        else:
            self.connection_status.setText("● Disconnected")
            self.connection_status.setStyleSheet(f"color: {RED}; font-weight: bold;")
