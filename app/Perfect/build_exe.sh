#!/usr/bin/env bash
# Builds a single-file VDAS binary on Linux/macOS using PyInstaller.
# Note: PyInstaller is NOT a cross-compiler - running this on Linux/macOS
# produces a Linux/macOS binary, not a Windows .exe. To build VDAS.exe you
# need to run build_exe.bat on an actual Windows machine (or a Windows VM).
set -e

echo "Installing/updating build dependencies..."
python3 -m pip install --upgrade pip
python3 -m pip install -r requirements.txt
python3 -m pip install pyinstaller

echo
echo "Building VDAS binary..."
pyinstaller --noconfirm --clean vdas.spec

echo
echo "Done. Find the executable in the dist/ folder."
