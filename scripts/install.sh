#!/bin/bash
set -e

BINARY="sender"
INSTALL_BIN="/usr/local/bin/$BINARY"
SERVICE_FILE="sender.service"
SERVICE_DEST="/etc/systemd/system/$SERVICE_FILE"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

# Build
BUILD_DIR="$REPO_ROOT/out/release"
mkdir -p "$BUILD_DIR"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel

# Install binary
sudo install -m 755 "$BUILD_DIR/$BINARY" "$INSTALL_BIN"

# Install and enable service
sudo install -m 644 "$REPO_ROOT/$SERVICE_FILE" "$SERVICE_DEST"
sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_FILE"
sudo systemctl restart "$SERVICE_FILE"

echo "Installed and started sender.service"
echo "  Logs: journalctl -u sender -f"
echo "Status: systemctl status sender"
