@echo off
REM Builds a single-file VDAS.exe on Windows using PyInstaller.
REM Run this from the project root (the folder containing main.py).

setlocal

echo Installing/updating build dependencies...
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
python -m pip install pyinstaller

echo.
echo Building VDAS.exe ...
pyinstaller --noconfirm --clean vdas.spec

echo.
echo Done. Find the executable at: dist\VDAS.exe
pause
