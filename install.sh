#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================================="
echo " Installing Conway's Game of Life to Desktop"
echo "=========================================================="

# 1. Ensure binary is compiled
if [ ! -f "bin/game_of_life" ]; then
    echo ">> Compiling binary..."
    make
fi

# 2. Define installation directories (user-level, no root required)
BIN_DIR="${HOME}/.local/bin"
APP_DIR="${HOME}/.local/share/applications"
ICON_DIR="${HOME}/.local/share/icons/hicolor/scalable/apps"
DESKTOP_DIR="${HOME}/Desktop"

mkdir -p "$BIN_DIR"
mkdir -p "$APP_DIR"
mkdir -p "$ICON_DIR"

# 3. Install executable
echo ">> Installing executable to: ${BIN_DIR}/game_of_life"
cp -f "bin/game_of_life" "${BIN_DIR}/game_of_life"
chmod +x "${BIN_DIR}/game_of_life"

# 4. Install SVG icon
echo ">> Installing icon to: ${ICON_DIR}/game-of-life.svg"
cp -f "assets/game-of-life.svg" "${ICON_DIR}/game-of-life.svg"

# 5. Install Desktop Application Entry
echo ">> Installing launcher to: ${APP_DIR}/game-of-life.desktop"
cp -f "assets/game-of-life.desktop" "${APP_DIR}/game-of-life.desktop"
chmod +x "${APP_DIR}/game-of-life.desktop"

# 6. Place on Desktop if folder exists
if [ -d "$DESKTOP_DIR" ]; then
    echo ">> Adding Desktop shortcut to: ${DESKTOP_DIR}/game-of-life.desktop"
    cp -f "assets/game-of-life.desktop" "${DESKTOP_DIR}/game-of-life.desktop"
    chmod +x "${DESKTOP_DIR}/game-of-life.desktop"
    # Mark as trusted on GNOME / Fedora if gio is available
    if command -v gio >/dev/null 2>&1; then
        gio set "${DESKTOP_DIR}/game-of-life.desktop" metadata::trusted true 2>/dev/null || true
    fi
fi

# 7. Update desktop & icon caches
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APP_DIR}" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache "${HOME}/.local/share/icons/hicolor" 2>/dev/null || true
fi

echo "=========================================================="
echo " [SUCCESS] Conway's Game of Life installed successfully!"
echo " - Launch from your Applications Menu (search 'Game of Life')"
echo " - Or double-click the icon on your Desktop"
echo " - Or run 'game_of_life' from any terminal"
echo "=========================================================="
