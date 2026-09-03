"""
vdas.core.workers
===================

ADCWorker and PIDWorker are separate QThreads that poll the hardware in the
background and emit Qt signals back to the GUI thread. This fixes the
blocking-UI issue in the original pid_gui.py, where PID monitoring ran on
the GUI thread and could stall the interface for seconds at a time.

Logic is unchanged from the original single-file application - only the
location has moved.
"""

import time

from PyQt6.QtCore import QMutex, QMutexLocker, QThread, pyqtSignal

from vdas.core.scpi_controller import SCPIController


# =============================================================================
# ADC ACQUISITION WORKER (background thread)
# =============================================================================
class ADCWorker(QThread):
    data_received = pyqtSignal(float, dict)

    def __init__(self, scpi: SCPIController, channel_modes: dict, start_offset: float = 0.0):
        super().__init__()
        self.scpi = scpi
        self.channel_modes = channel_modes.copy()
        self.running = False
        self.interval = 0.1
        self.mutex = QMutex()
        # Elapsed-seconds value to resume the timeline from, so stopping
        # and restarting acquisition continues the plot's time axis
        # instead of jumping back to t=0 (see GraphTabMixin.toggle_acquisition).
        self.start_offset = start_offset

    def set_sampling_rate(self, hz: float):
        with QMutexLocker(self.mutex):
            self.interval = 1.0 / max(0.1, hz)

    def update_modes(self, new_modes: dict):
        with QMutexLocker(self.mutex):
            self.channel_modes = new_modes.copy()

    def stop(self):
        with QMutexLocker(self.mutex):
            self.running = False

    def run(self):
        self.running = True
        start_time = time.time() - self.start_offset

        while True:
            with QMutexLocker(self.mutex):
                if not self.running:
                    break
                sleep_time = self.interval
                modes = self.channel_modes.copy()

            if self.scpi.is_connected():
                timestamp = time.time() - start_time
                readings = self.scpi.fetch_all_telemetry(modes)
                self.data_received.emit(timestamp, readings)

            time.sleep(sleep_time)


# =============================================================================
# PID MONITOR WORKER (background thread)
# =============================================================================
class PIDWorker(QThread):
    data_received = pyqtSignal(dict)  # {loop: {"status", "meas", "err", "out"}}

    def __init__(self, scpi: SCPIController):
        super().__init__()
        self.scpi = scpi
        self.running = False
        self.interval = 0.5
        self.mutex = QMutex()

    def set_interval(self, seconds: float):
        with QMutexLocker(self.mutex):
            self.interval = max(0.1, seconds)

    def stop(self):
        with QMutexLocker(self.mutex):
            self.running = False

    def run(self):
        self.running = True

        while True:
            with QMutexLocker(self.mutex):
                if not self.running:
                    break
                sleep_time = self.interval

            if self.scpi.is_connected():
                data = {
                    0: self.scpi.fetch_pid_snapshot(0),
                    1: self.scpi.fetch_pid_snapshot(1),
                }
                self.data_received.emit(data)

            time.sleep(sleep_time)
