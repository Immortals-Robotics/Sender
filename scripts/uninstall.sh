#!/bin/bash
set -e

sudo systemctl stop sender.service || true
sudo systemctl disable sender.service || true
sudo rm -f /etc/systemd/system/sender.service
sudo rm -f /usr/local/bin/sender
sudo systemctl daemon-reload

echo "sender service removed"
