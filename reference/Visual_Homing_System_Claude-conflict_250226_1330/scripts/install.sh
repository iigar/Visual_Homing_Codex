#!/bin/bash
# =============================================================================
# Visual Homing System - Unified Installer v2.2
# Підтримка: Raspberry Pi Zero 2 W, Pi 4B, Pi 5
# =============================================================================
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

INSTALL_DIR="/home/pi/visual_homing"
VENV_DIR="/home/pi/venv"
LOG_FILE="/tmp/visual_homing_install.log"

log() { echo -e "${CYAN}[$(date '+%H:%M:%S')]${NC} $1"; }
ok() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err() { echo -e "${RED}[ERROR]${NC} $1"; }

# --- Detect Raspberry Pi Model ---
detect_pi_model() {
    local model=$(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')
    if [[ "$model" == *"Raspberry Pi 5"* ]]; then
        echo "pi5"
    elif [[ "$model" == *"Raspberry Pi 4"* ]]; then
        echo "pi4"
    elif [[ "$model" == *"Raspberry Pi Zero 2"* ]]; then
        echo "pizero2"
    elif [[ "$model" == *"Raspberry Pi Zero"* ]]; then
        echo "pizero"
    else
        echo "unknown"
    fi
}

PI_MODEL=$(detect_pi_model)

echo ""
echo "=============================================="
echo "  Visual Homing System - Installer v2.2"
echo "  Raspberry Pi Zero 2 W / Pi 4B / Pi 5"
echo "=============================================="
echo ""
echo "Виявлено модель: $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0' || echo 'Unknown')"
echo "Тип: $PI_MODEL"
echo ""

# --- 1. System Update ---
log "Оновлення системи..."
sudo apt update -qq && sudo apt upgrade -y -qq >> "$LOG_FILE" 2>&1
ok "Систему оновлено"

# --- 2. System Dependencies ---
log "Встановлення системних пакетів..."

# Core packages (all Pi models)
CORE_PACKAGES="python3-pip python3-venv python3-dev python3-opencv python3-numpy libopencv-dev git cmake build-essential v4l-utils ffmpeg libjpeg-dev libpng-dev libtiff-dev libxml2-dev libxslt1-dev screen htop"

# Math libraries - try modern first, fallback to legacy
MATH_PACKAGES=""
if apt-cache show liblapack-dev > /dev/null 2>&1; then
    MATH_PACKAGES="liblapack-dev libblas-dev libhdf5-dev"
elif apt-cache show libatlas-base-dev > /dev/null 2>&1; then
    MATH_PACKAGES="libatlas-base-dev libhdf5-dev"
else
    warn "Математичні бібліотеки не знайдено, продовжуємо без них"
fi

# Pi 5 specific packages
PI5_PACKAGES=""
if [ "$PI_MODEL" = "pi5" ]; then
    PI5_PACKAGES="rpicam-apps libcamera-dev"
fi

sudo apt install -y -qq $CORE_PACKAGES $MATH_PACKAGES $PI5_PACKAGES >> "$LOG_FILE" 2>&1
ok "Системні пакети встановлено"

# --- 3. Python Virtual Environment ---
log "Налаштування Python venv..."
if [ ! -d "$VENV_DIR" ]; then
  python3 -m venv "$VENV_DIR" --system-site-packages
  echo 'source ~/venv/bin/activate' >> ~/.bashrc
fi
source "$VENV_DIR/bin/activate"
ok "Python venv готовий"

# --- 4. Python Dependencies ---
log "Встановлення Python бібліотек..."
pip install --upgrade pip setuptools wheel >> "$LOG_FILE" 2>&1
pip install pymavlink pyserial flask flask-cors flask-socketio opencv-python-headless numpy requests >> "$LOG_FILE" 2>&1

# Install picamera2 if on Pi (for native camera support)
if [ "$PI_MODE" != "unknown" ]; then
    sudo apt install -y -qq python3-picamera2 >> "$LOG_FILE" 2>&1 || true
fi

ok "Python бібліотеки встановлено"

# --- 5. Create Project Directory ---
log "Створення директорій проекту..."
mkdir -p "$INSTALL_DIR"/{camera,vision,navigation,sensors,mavlink,web,data,routes,logs}
ok "Директорії створено"

# --- 6. Copy/Download Firmware Files ---
log "Копіювання файлів прошивки..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# If running from the project directory, copy files
if [ -f "$SCRIPT_DIR/firmware/python/main.py" ]; then
  cp -r "$SCRIPT_DIR/firmware/python/"* "$INSTALL_DIR/"
  if [ -d "$SCRIPT_DIR/firmware/config" ]; then
    cp -r "$SCRIPT_DIR/firmware/config" "$INSTALL_DIR/config"
  fi
  ok "Файли скопійовано з локальної директорії"
else
  # Download from API
  log "Завантаження прошивки з сервера..."
  FIRMWARE_URL="https://github.com/iigar/Visual_Homing_System_Claude/archive/refs/heads/main.zip"

  cd /tmp
  if wget -q "$FIRMWARE_URL" -O firmware.zip 2>/dev/null; then
    unzip -o -q firmware.zip -d "$INSTALL_DIR/"
    rm -f firmware.zip
    ok "Прошивку завантажено та розпаковано"
  else
    warn "Не вдалося завантажити прошивку. Завантажте вручну:"
    warn "  wget $FIRMWARE_URL -O firmware.zip"
    warn "  unzip -o firmware.zip -d $INSTALL_DIR/"
  fi
fi

# --- 7. Configure UART ---
log "Налаштування UART..."
CONFIG_FILE="/boot/firmware/config.txt"
if [ ! -f "$CONFIG_FILE" ]; then
  CONFIG_FILE="/boot/config.txt"
fi

# Check if UART already configured
if ! grep -q "enable_uart=1" "$CONFIG_FILE" 2>/dev/null; then
  # Model-specific UART configuration
  if [ "$PI_MODEL" = "pi5" ]; then
    sudo tee -a "$CONFIG_FILE" > /dev/null << 'UARTEOF'

# === Visual Homing UART Configuration (Pi 5) ===
enable_uart=1
# Pi 5 використовує PL011 UART
dtparam=uart0=on
# Додаткові UART на GPIO
dtoverlay=uart2-pi5
dtoverlay=uart3-pi5
# Камера (Pi 5 libcamera)
gpu_mem=256
dtparam=i2c_arm=on
UARTEOF
  else
    sudo tee -a "$CONFIG_FILE" > /dev/null << 'UARTEOF'

# === Visual Homing UART Configuration (Pi Zero 2 W / Pi 4B) ===
enable_uart=1
dtoverlay=disable-bt
dtoverlay=uart2
dtoverlay=uart3
gpu_mem=128
start_x=1
dtparam=i2c_arm=on
UARTEOF
  fi
  ok "UART налаштовано в $CONFIG_FILE (модель: $PI_MODEL)"
else
  ok "UART вже налаштовано"
fi

# --- 8. User Permissions ---
log "Налаштування дозволів..."
sudo usermod -a -G dialout,video,i2c pi 2>/dev/null || true
ok "Дозволи налаштовано (dialout, video, i2c)"

# --- 9. Swap Configuration ---
log "Налаштування swap..."

# Swap size based on Pi model (Pi 4/5 have more RAM, need less swap)
if [ "$PI_MODEL" = "pi5" ] || [ "$PI_MODEL" = "pi4" ]; then
    SWAP_SIZE=512
else
    SWAP_SIZE=1024
fi

if [ -f /etc/dphys-swapfile ]; then
    CURRENT_SWAP=$(cat /etc/dphys-swapfile | grep 'CONF_SWAPSIZE=' | cut -d'=' -f2 || echo "0")
    if [ "$CURRENT_SWAP" -lt "$SWAP_SIZE" ] 2>/dev/null; then
        sudo dphys-swapfile swapoff 2>/dev/null || true
        sudo sed -i "s/CONF_SWAPSIZE=.*/CONF_SWAPSIZE=$SWAP_SIZE/" /etc/dphys-swapfile
        sudo dphys-swapfile setup >> "$LOG_FILE" 2>&1
        sudo dphys-swapfile swapon
        ok "Swap налаштовано: ${SWAP_SIZE}MB"
    else
        ok "Swap вже налаштовано: ${CURRENT_SWAP}MB"
    fi
else
    warn "dphys-swapfile не знайдено, пропускаємо налаштування swap"
fi

# --- 10. Disable Unnecessary Services ---
log "Оптимізація системи..."
sudo systemctl disable bluetooth 2>/dev/null || true
sudo systemctl disable hciuart 2>/dev/null || true
ok "Непотрібні сервіси вимкнено"

# --- 11. Create Systemd Service ---
log "Створення systemd сервісу..."
sudo tee /etc/systemd/system/visual-homing.service > /dev/null << EOF
[Unit]
Description=Visual Homing Navigation System
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=$INSTALL_DIR
Environment=PATH=$VENV_DIR/bin:/usr/bin:/bin
ExecStart=$VENV_DIR/bin/python3 main.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable visual-homing
ok "Systemd сервіс створено та увімкнено"

# --- 12. Verify Installation ---
echo ""
echo "=============================================="
echo "  Перевірка встановлення"
echo "=============================================="
echo ""

ERRORS=0

# Python
python3 -c "import cv2; print(f'  OpenCV: {cv2.__version__}')" 2>/dev/null || { err "OpenCV не знайдено"; ERRORS=$((ERRORS+1)); }
python3 -c "from pymavlink import mavutil; print('  pymavlink: OK')" 2>/dev/null || { err "pymavlink не знайдено"; ERRORS=$((ERRORS+1)); }
python3 -c "import serial; print('  pyserial: OK')" 2>/dev/null || { err "pyserial не знайдено"; ERRORS=$((ERRORS+1)); }
python3 -c "import flask; print(f'  Flask: {flask.__version__}')" 2>/dev/null || { err "Flask не знайдено"; ERRORS=$((ERRORS+1)); }

# UART
if [ -e /dev/serial0 ]; then
  ok "UART serial0 доступний"
else
  warn "UART serial0 не знайдено (потрібен reboot)"
fi

# Camera
if [ -e /dev/video0 ]; then
  ok "Камера /dev/video0 доступна"
else
  warn "Камера не знайдена (підключити після reboot)"
fi

echo ""
if [ $ERRORS -eq 0 ]; then
  echo -e "${GREEN}=============================================="
  echo "  Встановлення завершено успішно!"
  echo "==============================================${NC}"
else
  echo -e "${RED}=============================================="
  echo "  Встановлення завершено з $ERRORS помилками"
  echo "==============================================${NC}"
fi

echo ""
echo "=============================================="
echo "  Наступні кроки"
echo "=============================================="
echo ""
echo "  1. sudo reboot   (для застосування UART)"
echo "  2. Підключити сенсори (MATEK 3901-L0X, TF-Luna)"
echo "  3. Підключити камеру"
echo "  4. Завантажити параметри ArduPilot: $INSTALL_DIR/config/visual_homing.param"
echo "  5. Запустити: sudo systemctl start visual-homing"
echo ""
echo "Корисні команди:"
echo "  Статус:  sudo systemctl status visual-homing"
echo "  Логи:    journalctl -u visual-homing -f"
echo "  Стоп:    sudo systemctl stop visual-homing"
echo "  Рестарт: sudo systemctl restart visual-homing"
echo ""
echo "Повний лог встановлення: $LOG_FILE"
echo ""

# Model-specific notes
if [ "$PI_MODEL" = "pi5" ]; then
    echo "Примітки для Raspberry Pi 5:"
    echo "  - Використовуйте 'rpicam-hello' замість 'libcamera-hello' для тесту камери"
    echo "  - UART доступні через /dev/ttyAMA0, /dev/ttyAMA2, /dev/ttyAMA3"
    echo ""
elif [ "$PI_MODEL" = "pi4" ]; then
    echo "Примітки для Raspberry Pi 4B:"
    echo "  - Більше RAM дозволяє швидшу обробку зображень"
    echo "  - Рекомендовано активне охолодження при тривалій роботі"
    echo ""
elif [ "$PI_MODEL" = "pizero2" ]; then
    echo "Примітки для Raspberry Pi Zero 2 W:"
    echo "  - Обмежена RAM, використовуйте opencv-headless"
    echo "  - Рекомендовано пасивне охолодження"
    echo ""
fi
