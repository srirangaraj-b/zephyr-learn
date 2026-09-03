"""
vdas.ui.tabs.safety
======================

The global "SAFE SHUTDOWN" button and the window closeEvent() confirmation
flow.
"""

from PyQt6.QtWidgets import QMessageBox


class SafetyMixin:
    def shutdown_all(self):
        if not self.scpi.is_connected():
            return

        commands = [
            "PID:OFF (@0)",
            "PID:OFF (@1)",
            "OUTP (@0) OFF",
            "OUTP (@1) OFF",
            "OUTP (@2) OFF",
            "OUTP (@3) OFF",
        ]

        for command in commands:
            self.scpi.send_cmd(command)

        self.status_bar.showMessage("SAFE SHUTDOWN complete: PID loops off, all DAC outputs disabled.")
        self.log_message("Safe shutdown executed.")

    def closeEvent(self, event):
        if self.scpi.is_connected():
            answer = QMessageBox.question(
                self,
                "Exit",
                "The VDAS is still connected.\n\nPerform safe shutdown before exiting?",
                QMessageBox.StandardButton.Yes
                | QMessageBox.StandardButton.No
                | QMessageBox.StandardButton.Cancel,
            )

            if answer == QMessageBox.StandardButton.Cancel:
                event.ignore()
                return

            if answer == QMessageBox.StandardButton.Yes:
                self.shutdown_all()

        self.stop_adc_worker()
        self.stop_pid_worker()

        if self.is_logging:
            self.stop_logging()

        if self.scpi.is_connected():
            self.scpi.disconnect()

        event.accept()
