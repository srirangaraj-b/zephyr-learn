"""
vdas.ui.tabs.dac
===================

The "DAC" tab: four independent DACChannelCard widgets. Unchanged from the
original application.
"""

from PyQt6.QtWidgets import QGridLayout, QLabel, QVBoxLayout, QWidget

from vdas.ui.widgets import DACChannelCard


class DACTabMixin:
    def create_dac_tab(self):
        tab = QWidget()
        layout = QVBoxLayout(tab)

        info = QLabel("Four-channel DAC voltage control. Output range: 0.0000 V to 5.0000 V.")
        info.setStyleSheet("font-weight: bold; padding: 5px;")
        layout.addWidget(info)

        grid = QGridLayout()
        self.channel_cards = []

        for ch in range(4):
            card = DACChannelCard(ch, self.scpi)
            self.channel_cards.append(card)
            row, col = divmod(ch, 2)
            grid.addWidget(card, row, col)

        layout.addLayout(grid)
        layout.addStretch()

        return tab
