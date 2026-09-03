"""
vdas.ui.tabs.dashboard
=========================

The "Dashboard" tab: a live summary of all 6 ADC channels and both PID
loops. Logic and layout are unchanged from the original application.
"""

from PyQt6.QtGui import QFont
from PyQt6.QtWidgets import QGridLayout, QGroupBox, QHBoxLayout, QLabel, QWidget

from vdas.ui.widgets import create_value_label


class DashboardTabMixin:
    def create_dashboard_tab(self):
        tab = QWidget()
        root = QHBoxLayout(tab)

        # ADC summary
        adc_box = QGroupBox("ADC Channels — Live Values")
        adc_layout = QGridLayout()

        for ch in range(6):
            adc_layout.addWidget(QLabel(f"CH {ch}:"), ch, 0)
            lbl = create_value_label()
            self.adc_dash_labels[ch] = lbl
            adc_layout.addWidget(lbl, ch, 1)

        adc_box.setLayout(adc_layout)
        root.addWidget(adc_box, 1)

        # PID summary
        pid_box = QGroupBox("PID Loops — Live Status")
        pid_layout = QGridLayout()

        headers = [("", 0), ("PID Loop A", 1), ("PID Loop B", 2)]
        for text, col in headers:
            lbl = QLabel(text)
            lbl.setFont(QFont("Arial", 9, QFont.Weight.Bold))
            pid_layout.addWidget(lbl, 0, col)

        pid_layout.addWidget(QLabel("Status"), 1, 0)
        pid_layout.addWidget(QLabel("Measured"), 2, 0)
        pid_layout.addWidget(QLabel("Error"), 3, 0)
        pid_layout.addWidget(QLabel("Output"), 4, 0)

        for loop in (0, 1):
            col = loop + 1
            self.pid_dash_labels[loop] = {
                "status": create_value_label(),
                "meas": create_value_label(),
                "err": create_value_label(),
                "out": create_value_label(),
            }
            pid_layout.addWidget(self.pid_dash_labels[loop]["status"], 1, col)
            pid_layout.addWidget(self.pid_dash_labels[loop]["meas"], 2, col)
            pid_layout.addWidget(self.pid_dash_labels[loop]["err"], 3, col)
            pid_layout.addWidget(self.pid_dash_labels[loop]["out"], 4, col)

        pid_box.setLayout(pid_layout)
        root.addWidget(pid_box, 1)

        return tab
