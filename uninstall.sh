#!/usr/bin/env bash
set -e

echo "=========================================================="
echo " Uninstalling Conway's Game of Life"
echo "=========================================================="

BIN_FILE="${HOME}/.local/bin/game_of_life"
APP_FILE="${HOME}/.local/share/applications/game-of-life.desktop"
ICON_FILE="${HOME}/.local/share/icons/hicolor/scalable/apps/game-of-life.svg"
DESKTOP_FILE="${HOME}/Desktop/game-of-life.desktop"

rm -f "$BIN_FILE"
rm -f "$APP_FILE"
rm -f "$ICON_FILE"
rm -f "$DESKTOP_FILE"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
fi

echo "[SUCCESS] Conway's Game of Life has been uninstalled."
