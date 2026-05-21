#!/bin/bash
# Setup autostart for Visual Homing

set -e

echo "=== Налаштування автозапуску ==="

# Create systemd service file
cat << 'EOF' | sudo tee /etc/systemd/system/visual-homing.service
[Unit]
Description=Visual Homing Navigation System
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/visual_homing
Environment=PATH=/home/pi/venv/bin:/usr/local/bin:/usr/bin:/bin
ExecStart=/home/pi/venv/bin/python main.py --autostart --web
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

# Wait for camera and UART
ExecStartPre=/bin/sleep 10

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd
sudo systemctl daemon-reload

# Enable autostart
sudo systemctl enable visual-homing.service

echo ""
echo "Автозапуск налаштовано!"
echo ""
echo "Команди:"
echo "  Запустити:   sudo systemctl start visual-homing"
echo "  Зупинити:    sudo systemctl stop visual-homing"
echo "  Статус:      sudo systemctl status visual-homing"
echo "  Логи:        journalctl -u visual-homing -f"
echo ""
echo "Перезавантажте для активації: sudo reboot"
