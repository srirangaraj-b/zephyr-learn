"""
vdas.ui.style
==============

Application-wide QSS. Purely cosmetic - no widget behavior, signal, or SCPI
command is affected by anything in this module. Applied once, globally, via
app.setStyleSheet(STYLESHEET) in main.py, so individual widgets no longer
need to carry one-off inline setStyleSheet() calls for basic look and feel
(state-dependent colors like "PID ACTIVE" vs "OFF" are still set at runtime
where the state is known).
"""

ACCENT = "#2f8fff"
ACCENT_HOVER = "#4aa0ff"
ACCENT_PRESSED = "#1f74d6"
BG_WINDOW = "#15171c"
BG_PANEL = "#1c1f26"
BG_INPUT = "#20242c"
BORDER = "#2a2e37"
TEXT = "#e6e8eb"
TEXT_DIM = "#8b93a1"
GREEN = "#2fbf71"
RED = "#e5484d"

STYLESHEET = f"""
* {{
    outline: none;
}}

QWidget {{
    background-color: {BG_WINDOW};
    color: {TEXT};
    font-family: "Segoe UI", "Inter", "Arial", sans-serif;
    font-size: 13px;
}}

QMainWindow {{
    background-color: {BG_WINDOW};
}}

QGroupBox {{
    background-color: {BG_PANEL};
    border: 1px solid {BORDER};
    border-radius: 10px;
    margin-top: 16px;
    padding: 14px 10px 10px 10px;
    font-weight: 600;
}}

QGroupBox::title {{
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    top: 2px;
    padding: 0 6px;
    color: {TEXT};
    background-color: transparent;
}}

QLabel {{
    background: transparent;
}}

QTabWidget::pane {{
    border: 1px solid {BORDER};
    border-radius: 10px;
    top: -1px;
    background-color: {BG_WINDOW};
}}

QTabBar::tab {{
    background-color: {BG_PANEL};
    color: {TEXT_DIM};
    border: 1px solid {BORDER};
    border-bottom: none;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    padding: 8px 18px;
    margin-right: 2px;
    font-weight: 600;
}}

QTabBar::tab:selected {{
    background-color: {BG_WINDOW};
    color: {TEXT};
    border-color: {ACCENT};
}}

QTabBar::tab:hover:!selected {{
    color: {TEXT};
}}

QPushButton {{
    background-color: {BG_INPUT};
    color: {TEXT};
    border: 1px solid {BORDER};
    border-radius: 6px;
    padding: 6px 14px;
    font-weight: 600;
}}

QPushButton:hover {{
    border-color: {ACCENT};
}}

QPushButton:pressed {{
    background-color: {BORDER};
}}

QPushButton:disabled {{
    color: {TEXT_DIM};
    border-color: {BORDER};
}}

QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox, QTextEdit {{
    background-color: {BG_INPUT};
    border: 1px solid {BORDER};
    border-radius: 6px;
    padding: 4px 8px;
    selection-background-color: {ACCENT};
}}

QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus {{
    border-color: {ACCENT};
}}

QLineEdit:read-only {{
    color: {TEXT_DIM};
}}

QComboBox::drop-down {{
    border: none;
    width: 22px;
}}

QComboBox QAbstractItemView {{
    background-color: {BG_INPUT};
    border: 1px solid {BORDER};
    selection-background-color: {ACCENT};
    outline: none;
}}

QCheckBox {{
    spacing: 6px;
}}

QCheckBox::indicator {{
    width: 15px;
    height: 15px;
    border-radius: 3px;
    border: 1px solid {BORDER};
    background-color: {BG_INPUT};
}}

QCheckBox::indicator:checked {{
    background-color: {ACCENT};
    border-color: {ACCENT};
}}

QSlider::groove:horizontal {{
    height: 4px;
    background: {BORDER};
    border-radius: 2px;
}}

QSlider::handle:horizontal {{
    background: {ACCENT};
    width: 14px;
    height: 14px;
    margin: -6px 0;
    border-radius: 7px;
}}

QSlider::sub-page:horizontal {{
    background: {ACCENT};
    border-radius: 2px;
}}

QFrame[frameShape="4"], QFrame[frameShape="5"] {{
    color: {BORDER};
}}

QLabel[valueLabel="true"] {{
    background-color: {BG_INPUT};
    border: 1px solid {BORDER};
    border-radius: 5px;
    padding: 3px 6px;
    font-family: "Consolas", "Courier New", monospace;
}}

QStatusBar {{
    background-color: {BG_PANEL};
    color: {TEXT_DIM};
    border-top: 1px solid {BORDER};
}}

QTextEdit {{
    font-family: "Consolas", "Courier New", monospace;
}}

QScrollBar:vertical {{
    background: {BG_WINDOW};
    width: 12px;
    margin: 0;
}}

QScrollBar::handle:vertical {{
    background: {BORDER};
    min-height: 24px;
    border-radius: 5px;
}}

QScrollBar::handle:vertical:hover {{
    background: {ACCENT};
}}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0;
}}
"""


def primary_button_style() -> str:
    return f"background-color: {ACCENT}; color: white; font-weight: 700; border: none;"


def success_button_style() -> str:
    return f"background-color: {GREEN}; color: white; font-weight: 700; border: none;"


def danger_button_style() -> str:
    return f"background-color: {RED}; color: white; font-weight: 700; border: none;"
