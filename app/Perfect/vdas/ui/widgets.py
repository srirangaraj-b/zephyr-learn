"""
vdas.ui.widgets
=================

Small reusable widgets shared across tabs:

- create_value_label(): the boxed read-only value label used all over the
  Graph/PID tabs.

The standalone DAC tab (and its DACChannelCard widget) has been removed:
DAC channels 0/1 are now driven directly from the PID tab's Manual
setpoint mode (see vdas.ui.tabs.pid), using the same underlying
SCPIController.set_dac_voltage() / set_dac_output() calls the old DAC
card used.
"""

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import QFrame, QLabel


def create_value_label() -> QLabel:
    label = QLabel("---")
    label.setFrameShape(QFrame.Shape.Box)
    label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    label.setMinimumHeight(25)
    label.setProperty("valueLabel", True)
    return label
