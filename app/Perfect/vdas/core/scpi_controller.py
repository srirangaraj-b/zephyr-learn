"""
vdas.core.scpi_controller
==========================

Owns the serial connection to the VDAS hardware and provides every SCPI
command used by the GUI (ADC config/measurement, DAC control, PID control,
and identification).

This module is a direct extraction of the SCPIController /  SCPILogger /
extract_value logic from the original single-file application. No SCPI
command, parsing rule, or timing behavior has been changed.

Command/response integrity
---------------------------
Every SCPI reply is echoed back in the form:

    <command sent>->(<data>)

e.g.  MEAS:VOLT:ALL?->(2.1001,0.0000,0.0000,0.0000,0.0000,0.0000)
      PID:MEAS? (@0)->(3.498)
      *IDN?->(VIMICRO,VDAS,01,3.3)

extract_value() and SCPIController.parse_scpi_array() REQUIRE the expected
command (and, for PID, the exact (@loop) index) to be present in the
echoed portion of the response before the payload after "->" is trusted.
If the echo doesn't match what was actually sent, the value is discarded
(empty list / "---") and logged, rather than being silently written into
the wrong ADC channel or the wrong PID loop's box. This matters
specifically because ADCWorker and PIDWorker share one mutex-guarded
serial line - a stale or misrouted reply is discarded instead of
corrupting a live display.
"""

import socket
import time

import serial
from PyQt6.QtCore import QMutex, QMutexLocker, QObject, pyqtSignal


# =============================================================================
# TCP TRANSPORT ADAPTER
# =============================================================================
class _TCPAdapter:
    """
    Thin wrapper around a TCP socket that mimics the small subset of
    pyserial's Serial API that SCPIController.send_cmd() / disconnect() /
    is_connected() actually use: .write(), .readline(), .reset_input_buffer(),
    .is_open, and .close().

    Because SCPIController only ever talks to self.ser through that subset,
    wrapping a socket in this adapter lets the exact same send_cmd() /
    parsing / echo-verification logic in this file run unmodified over
    TCP (e.g. an ESP32/STM32 exposing the SCPI server on a socket, or a
    serial-to-Ethernet bridge) - no protocol or command changes needed.
    """

    def __init__(self, host: str, port: int, timeout: float = 0.5):
        self._timeout = timeout
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.settimeout(timeout)
        self._rx_buf = b""
        self.is_open = True

    def write(self, data: bytes):
        self._sock.sendall(data)

    def readline(self) -> bytes:
        # Mirrors pyserial's readline(timeout=...) contract that send_cmd()
        # relies on: return whatever's been accumulated (line, possibly
        # incomplete/empty) once a '\n' arrives or the socket times out,
        # rather than blocking forever or raising.
        while b"\n" not in self._rx_buf:
            try:
                chunk = self._sock.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            self._rx_buf += chunk

        if b"\n" in self._rx_buf:
            line, self._rx_buf = self._rx_buf.split(b"\n", 1)
            return line + b"\n"

        line, self._rx_buf = self._rx_buf, b""
        return line

    def reset_input_buffer(self):
        self._rx_buf = b""
        # Also drain anything already sitting in the OS socket buffer,
        # matching pyserial's reset_input_buffer() behavior.
        self._sock.setblocking(False)
        try:
            while self._sock.recv(4096):
                pass
        except (BlockingIOError, OSError):
            pass
        finally:
            self._sock.setblocking(True)
            self._sock.settimeout(self._timeout)

    def close(self):
        try:
            self._sock.close()
        finally:
            self.is_open = False


# =============================================================================
# SHARED HELPERS
# =============================================================================
def extract_value(response: str, expected_cmd: str = None) -> str:
    """
    Converts responses such as:
        PID:MEAS? (@0)->(3.498)
    into:
        3.498
    Also handles "(3.498)" and plain "3.498".

    If expected_cmd is given, the echoed command portion (everything before
    "->") must contain it, or the value is rejected as untrustworthy. This
    is what stops a stale/misrouted response from being written into the
    wrong box - e.g. a Loop B reply landing in Loop A's "Measured" label
    because both loops share the same serial line.
    """
    if not response:
        return "---"

    value = response.strip()

    if "->" in value:
        echoed, value = value.split("->", 1)
        echoed = echoed.strip()
        if expected_cmd and expected_cmd.strip() not in echoed:
            return "---"
    else:
        if expected_cmd:
            return "---"

    value = value.replace("(", "").replace(")", "")

    return value.strip() or "---"


# =============================================================================
# SCPI CONTROLLER
# Single serial owner shared by ADC + DAC + PID panels
# =============================================================================
class SCPILogger(QObject):
    """Lightweight signal source so worker threads can log to the GUI safely."""
    message = pyqtSignal(str)


class SCPIController:
    def __init__(self):
        self.ser = None
        self.transport_kind = None  # "serial" or "tcp", set on successful connect
        self.mutex = QMutex()
        self.logger = SCPILogger()

    def _log(self, text: str):
        self.logger.message.emit(text)

    def connect(self, port: str, baudrate: int = 115200) -> bool:
        with QMutexLocker(self.mutex):
            try:
                self.ser = serial.Serial(port, baudrate, timeout=0.5, write_timeout=0.5)
                time.sleep(0.1)
                self.ser.reset_input_buffer()
                self.transport_kind = "serial"
                return True
            except Exception as e:
                self._log(f"! Connection Error: {e}")
                self.ser = None
                return False

    def connect_tcp(self, host: str, port: int) -> bool:
        """
        TCP/IP equivalent of connect(). Uses the exact same send_cmd(),
        echo-verification, and SCPI command set - only the transport
        underneath self.ser changes (see _TCPAdapter above), so ADC/DAC/PID
        behavior is identical whether hardware is reached over a serial
        port or over the network.
        """
        with QMutexLocker(self.mutex):
            try:
                self.ser = _TCPAdapter(host, port, timeout=0.5)
                self.ser.reset_input_buffer()
                self.transport_kind = "tcp"
                return True
            except Exception as e:
                self._log(f"! TCP Connection Error: {e}")
                self.ser = None
                return False

    def disconnect(self):
        with QMutexLocker(self.mutex):
            if self.ser and self.ser.is_open:
                self.ser.close()
            self.ser = None
            self.transport_kind = None

    def is_connected(self) -> bool:
        with QMutexLocker(self.mutex):
            return self.ser is not None and self.ser.is_open

    def send_cmd(self, cmd: str) -> str:
        with QMutexLocker(self.mutex):
            if not self.ser or not self.ser.is_open:
                return ""
            try:
                self.ser.reset_input_buffer()
                full_cmd = cmd.strip() + "\r\n"
                self.ser.write(full_cmd.encode("utf-8"))
                self._log(f"> {cmd}")

                response = self.ser.readline().decode("utf-8", errors="ignore").strip()

                if response:
                    self._log(f"< {response}")

                return response

            except Exception as e:
                self._log(f"! TX/RX Error ({cmd}): {e}")
                return ""

    # ---------------- IDENTIFICATION ----------------

    def query_idn(self) -> dict:

        idn_cmd = "*IDN?"
        response = self.send_cmd(idn_cmd)

        if not response:
            return {}

        payload = extract_value(response, expected_cmd=idn_cmd)
        if payload == "---":
            self._log(f"! *IDN? response failed echo verification: '{response}'")
            return {}

        fields = [f.strip() for f in payload.split(",")]
        if len(fields) != 4:
            self._log(f"! *IDN? response has unexpected field count: '{response}'")
            return {}

        company, product, model, firmware = fields
        return {
            "company": company,
            "product": product,
            "model": model,
            "firmware": firmware,
            "raw": response,
        }

    # ---------------- ADC ----------------

    def config_adc_channel(self, channel: int, is_current: bool) -> str:
        cmd = f"CONF:CURR (@{channel})" if is_current else f"CONF:VOLT (@{channel})"
        return self.send_cmd(cmd)

    def parse_scpi_array(self, raw_str: str, expected_cmd: str = None) -> list:

        if not raw_str:
            return []
        try:
            working = raw_str

            if "->" in working:
                echoed, working = working.split("->", 1)
                echoed = echoed.strip()
                if expected_cmd and expected_cmd.strip() not in echoed:
                    self._log(
                        f"! Command mismatch: sent '{expected_cmd}', "
                        f"echo was '{echoed}' - discarding response"
                    )
                    return []
            else:
                if expected_cmd:
                    self._log(f"! No command echo found for '{expected_cmd}' in: '{raw_str}'")
                    return []

            if "(" in working and ")" in working:
                working = working[working.find("(") + 1: working.rfind(")")]

            tokens = [x.strip() for x in working.split(",") if x.strip()]
            return [float(x) for x in tokens]
        except (ValueError, IndexError) as e:
            self._log(f"! Parsing Error: '{raw_str}': {e}")
            return []

    INTER_QUERY_SETTLE_S = 0.05

    def fetch_all_telemetry(self, channel_modes: dict) -> dict:

        readings = {}

        current_channels = {ch for ch in (0, 1) if channel_modes.get(ch, False)}
        voltage_channels = [ch for ch in range(6) if ch not in current_channels]

        if voltage_channels:
            volt_cmd = "MEAS:VOLT:ALL?"
            volt_resp = self.send_cmd(volt_cmd)
            volt_values = self.parse_scpi_array(volt_resp, expected_cmd=volt_cmd)

            for ch in voltage_channels:
                if ch < len(volt_values):
                    readings[ch] = (volt_values[ch], False)

        if current_channels:

            if voltage_channels:
                time.sleep(self.INTER_QUERY_SETTLE_S)

            curr_cmd = "MEAS:CURR:ALL?"
            curr_resp = self.send_cmd(curr_cmd)
            curr_values = self.parse_scpi_array(curr_resp, expected_cmd=curr_cmd)

            active_current_channels = [ch for ch in (0, 1) if ch in current_channels]

            for idx, ch in enumerate(active_current_channels):
                if idx < len(curr_values):
                    readings[ch] = (curr_values[idx], True)

        return readings

    # ---------------- DAC ----------------

    def set_dac_voltage(self, channel: int, voltage: float) -> str:
        return self.send_cmd(f"SOUR:VOLT (@{channel}) {voltage:.4f}")

    def set_dac_output(self, channel: int, state: bool) -> str:
        state_str = "ON" if state else "OFF"
        return self.send_cmd(f"OUTP (@{channel}) {state_str}")

    # ---------------- PID ----------------

    def fetch_pid_snapshot(self, loop: int) -> dict:
        stat_cmd = f"PID:STAT? (@{loop})"
        meas_cmd = f"PID:MEAS? (@{loop})"
        err_cmd = f"PID:ERR? (@{loop})"
        out_cmd = f"PID:OUT? (@{loop})"

        status = extract_value(self.send_cmd(stat_cmd), expected_cmd=stat_cmd)
        meas = extract_value(self.send_cmd(meas_cmd), expected_cmd=meas_cmd)
        err = extract_value(self.send_cmd(err_cmd), expected_cmd=err_cmd)
        out = extract_value(self.send_cmd(out_cmd), expected_cmd=out_cmd)

        return {"status": status, "meas": meas, "err": err, "out": out}

    def pid_on(self, loop: int) -> str:
        return self.send_cmd(f"PID:ON (@{loop})")

    def pid_off(self, loop: int) -> str:
        return self.send_cmd(f"PID:OFF (@{loop})")
