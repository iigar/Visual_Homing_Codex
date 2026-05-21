#!/bin/bash
# Install dependencies for Visual Homing on Raspberry Pi
# Підтримка: Pi Zero 2 W, Pi 4B, Pi 5
# Сумісність: Debian Bookworm, Trixie (13)

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

ok() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err() { echo -e "${RED}[ERROR]${NC} $1"; }

# Detect Pi model
detect_pi_model() {
    local model=$(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')
    if [[ "$model" == *"Raspberry Pi 5"* ]]; then
        echo "pi5"
    elif [[ "$model" == *"Raspberry Pi 4"* ]]; then
        echo "pi4"
    elif [[ "$model" == *"Raspberry Pi Zero 2"* ]]; then
        echo "pizero2"
    else
        echo "unknown"
    fi
}

PI_MODEL=$(detect_pi_model)

echo "=== Visual Homing Installation ==="
echo "Модель Pi: $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0' || echo 'Unknown')"
echo "Тип: $PI_MODEL"
echo ""

# Update system
echo "[1/6] Оновлення системи..."
sudo apt update && sudo apt upgrade -y
ok "Систему оновлено"

# Install system packages
echo "[2/6] Встановлення системних пакетів..."

# Core packages
CORE_PACKAGES="build-essential cmake git pkg-config v4l-utils python3-pip python3-dev python3-venv libopencv-dev python3-opencv libv4l-dev libjpeg-dev libpng-dev libtiff-dev"

# Math libraries - try liblapack first (newer), fallback to libatlas (older)
MATH_PACKAGES=""
if apt-cache show liblapack-dev > /dev/null 2>&1; then
    MATH_PACKAGES="liblapack-dev libblas-dev"
    echo "  -> Використовуємо liblapack-dev (Debian Trixie+)"
elif apt-cache show libatlas-base-dev > /dev/null 2>&1; then
    MATH_PACKAGES="libatlas-base-dev"
    echo "  -> Використовуємо libatlas-base-dev (Debian Bookworm)"
else
    warn "Математичні бібліотеки не знайдено"
fi

# Serial library - try libserial first, it might not exist in newer versions
SERIAL_PACKAGES=""
if apt-cache show libserial-dev > /dev/null 2>&1; then
    SERIAL_PACKAGES="libserial-dev"
fi

# Pi 5 specific
PI5_PACKAGES=""
if [ "$PI_MODEL" = "pi5" ]; then
    PI5_PACKAGES="rpicam-apps libcamera-dev"
fi

sudo apt install -y $CORE_PACKAGES $MATH_PACKAGES $SERIAL_PACKAGES $PI5_PACKAGES || {
    warn "Деякі пакети не встановлені, продовжуємо..."
}
ok "Системні пакети встановлено"

# Create virtual environment
echo "[3/6] Створення Python середовища..."
cd ~
if [ ! -d "venv" ]; then
    python3 -m venv venv --system-site-packages
fi
source ~/venv/bin/activate
ok "Python venv створено"

# Install Python packages
echo "[4/6] Встановлення Python бібліотек..."
pip install --upgrade pip
pip install \
    numpy \
    opencv-python-headless \
    pymavlink \
    pyserial \
    flask \
    flask-cors \
    flask-socketio \
    eventlet \
    requests
ok "Python бібліотеки встановлено"

# Verify critical packages
echo ""
echo "Перевірка встановлення..."
python3 -c "import cv2; print(f'  OpenCV: {cv2.__version__}')" 2>/dev/null || err "OpenCV не знайдено"
python3 -c "from pymavlink import mavutil; print('  pymavlink: OK')" 2>/dev/null || err "pymavlink не знайдено"
python3 -c "import serial; print('  pyserial: OK')" 2>/dev/null || err "pyserial не знайдено"
python3 -c "import flask; print(f'  Flask: {flask.__version__}')" 2>/dev/null || err "Flask не знайдено"

# Create directories
echo ""
echo "[5/6] Створення директорій..."
mkdir -p ~/visual_homing/data
mkdir -p ~/visual_homing/routes
mkdir -p ~/visual_homing/logs
mkdir -p ~/visual_homing/config
ok "Директорії створено"

# Copy files (assuming they are in current directory)
echo "[6/6] Копіювання файлів..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -d "$SCRIPT_DIR/../python" ]; then
    cp -r "$SCRIPT_DIR/../python/"* ~/visual_homing/
    ok "Файли скопійовано"
else
    warn "Файли прошивки не знайдено в $SCRIPT_DIR/../python"
fi

echo ""
echo -e "${GREEN}=== Встановлення завершено! ===${NC}"
echo ""
echo "Для запуску:"
echo "  cd ~/visual_homing"
echo "  source ~/venv/bin/activate"
echo "  python main.py --web"
echo ""
echo "Для налаштування автозапуску:"
echo "  ./scripts/setup_autostart.sh"
echo ""

# Model-specific notes
if [ "$PI_MODEL" = "pi5" ]; then
    echo "Примітки для Pi 5:"
    echo "  - Тест камери: rpicam-hello --timeout 2000"
    echo ""
elif [ "$PI_MODEL" = "pi4" ]; then
    echo "Примітки для Pi 4B:"
    echo "  - Рекомендовано активне охолодження"
    echo ""
fi
