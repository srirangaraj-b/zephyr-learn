"""
VDAS Unified Control Application - entry point.

Run with:
    python main.py

See README.md for setup, usage, and instructions on building a
standalone .exe with PyInstaller.
"""

import sys

from PyQt6.QtWidgets import QApplication

from vdas.ui.main_window import VDASApplication
from vdas.ui.style import STYLESHEET


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("VDAS Unified Control Application")
    app.setStyleSheet(STYLESHEET)

    window = VDASApplication()
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
