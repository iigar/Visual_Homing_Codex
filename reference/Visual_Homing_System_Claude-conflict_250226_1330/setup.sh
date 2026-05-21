#!/bin/bash
# ============================================================================
# Visual Homing System — Автоматичне налаштування Raspberry Pi
# ============================================================================
# Використання:
#   git clone https://github.com/iigar/Visual_Homing_System_Claude.git ~/visual-homing
#   cd ~/visual-homing
#   chmod +x setup.sh
#   ./setup.sh
#
# Підтримувані платформи:
#   - Raspberry Pi Zero 2 W
#   - Raspberry Pi 4B / 400
#   - Raspberry Pi 5
#
# Вимоги:
#   - Raspberry Pi OS Lite (64-bit, Bookworm+)
#   - Інтернет (для apt install)
# ============================================================================

set -e

# ─── Кольори ────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

# ─── Змінні ─────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VH_DIR="$SCRIPT_DIR"
FIRMWARE_DIR="$VH_DIR/firmware/python"
BACKEND_DIR="$VH_DIR/backend"
VENV_DIR="$VH_DIR/venv"
LOG_FILE="$VH_DIR/setup.log"
NEEDS_REBOOT=false
STEP=0
TOTAL_STEPS=8

# ─── Функції ────────────────────────────────────────────────
step() {
    STEP=$((STEP + 1))
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}  [$STEP/$TOTAL_STEPS] $1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

ok()   { echo -e "  ${GREEN}[OK]${NC} $1"; }
warn() { echo -e "  ${YELLOW}[!]${NC} $1"; }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; exit 1; }
skip() { echo -e "  ${CYAN}[SKIP]${NC} $1"; }
info() { echo -e "  $1"; }

# ─── Перевірка архітектури ──────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}"
echo "  ╔══════════════════════════════════════════════════╗"
echo "  ║       Visual Homing Setup Script v1.0            ║"
echo "  ║    Автоматичне налаштування Raspberry Pi          ║"
echo "  ╚══════════════════════════════════════════════════╝"
echo -e "${NC}"

ARCH=$(uname -m)
if [[ "$ARCH" != "aarch64" && "$ARCH" != "armv7l" ]]; then
    fail "Цей скрипт для Raspberry Pi (aarch64/armv7l). Поточна: $ARCH"
fi

if [ ! -d "$FIRMWARE_DIR" ]; then
    fail "firmware/python не знайдено. Запускай з кореня репо: cd ~/visual-homing && ./setup.sh"
fi

# Визначити модель Pi
PI_MODEL="Unknown"
[ -f /proc/device-tree/model ] && PI_MODEL=$(cat /proc/device-tree/model | tr -d '\0')

# Визначити тип для специфічних налаштувань
PI_TYPE="unknown"
if echo "$PI_MODEL" | grep -qi "Zero 2"; then
    PI_TYPE="pizero2"
elif echo "$PI_MODEL" | grep -qi "Pi 5"; then
    PI_TYPE="pi5"
elif echo "$PI_MODEL" | grep -qi "Pi 4"; then
    PI_TYPE="pi4"
fi

CURRENT_USER=$(whoami)
USER_HOME=$(eval echo ~$CURRENT_USER)

info "Платформа:  ${BOLD}$PI_MODEL${NC}"
info "Тип:        ${BOLD}$PI_TYPE${NC}"
info "Юзер:       ${BOLD}$CURRENT_USER${NC}"
info "Директорія: ${BOLD}$VH_DIR${NC}"
info "Лог:        ${BOLD}$LOG_FILE${NC}"

exec > >(tee -a "$LOG_FILE") 2>&1

# ════════════════════════════════════════════════════════════
# КРОК 1: Системні пакети
# ════════════════════════════════════════════════════════════
step "Встановлення системних пакетів"

PACKAGES=(
    python3-dev
    python3-pip
    python3-venv
    python3-opencv
    libopencv-dev
    v4l-utils
    git
    i2c-tools
    ffmpeg
)

# Математичні бібліотеки — нові Debian Trixie/Bookworm
if apt-cache show liblapack-dev &>/dev/null; then
    PACKAGES+=(liblapack-dev libblas-dev)
elif apt-cache show libatlas-base-dev &>/dev/null; then
    PACKAGES+=(libatlas-base-dev)
fi

# Pi 5 — libcamera замість legacy
if [ "$PI_TYPE" = "pi5" ]; then
    PACKAGES+=(rpicam-apps libcamera-dev)
fi

MISSING=()
for pkg in "${PACKAGES[@]}"; do
    dpkg -l "$pkg" &>/dev/null || MISSING+=("$pkg")
done

if [ ${#MISSING[@]} -eq 0 ]; then
    ok "Всі пакети вже встановлені"
else
    info "Встановлення: ${MISSING[*]}"
    sudo apt update -qq >> "$LOG_FILE" 2>&1
    sudo apt install -y "${MISSING[@]}" >> "$LOG_FILE" 2>&1
    ok "Встановлено ${#MISSING[@]} пакетів"
fi

# ════════════════════════════════════════════════════════════
# КРОК 2: Налаштування апаратних інтерфейсів
# ════════════════════════════════════════════════════════════
step "Налаштування UART, I2C, SPI, Camera"

CONFIG_FILE=""
[ -f /boot/firmware/config.txt ] && CONFIG_FILE="/boot/firmware/config.txt"
[ -z "$CONFIG_FILE" ] && [ -f /boot/config.txt ] && CONFIG_FILE="/boot/config.txt"

if [ -n "$CONFIG_FILE" ]; then
    info "Config: $CONFIG_FILE"

    add_once() {
        grep -q "^$1" "$CONFIG_FILE" 2>/dev/null || { echo "$1" | sudo tee -a "$CONFIG_FILE" > /dev/null; NEEDS_REBOOT=true; ok "$2"; return; }
        skip "$2 вже є"
    }

    add_once "dtparam=i2c_arm=on"         "I2C"
    add_once "dtparam=spi=on"             "SPI"
    add_once "camera_auto_detect=1"        "Camera auto-detect"
    add_once "enable_uart=1"              "Hardware UART (MAVLink)"
    add_once "dtparam=watchdog=on"        "Hardware watchdog"

    # UART2 та UART3 для сенсорів (Optical Flow + LiDAR)
    if [ "$PI_TYPE" = "pi5" ]; then
        add_once "dtoverlay=uart2-pi5"    "UART2 (Optical Flow, /dev/ttyAMA2)"
        add_once "dtoverlay=uart3-pi5"    "UART3 (LiDAR, /dev/ttyAMA3)"
    else
        add_once "dtoverlay=disable-bt"   "Disable Bluetooth (звільнити UART0)"
        add_once "dtoverlay=uart2"        "UART2 (Optical Flow, /dev/ttyAMA2)"
        add_once "dtoverlay=uart3"        "UART3 (LiDAR, /dev/ttyAMA3)"
    fi

    # GPU пам'ять
    if ! grep -q "^gpu_mem=" "$CONFIG_FILE" 2>/dev/null; then
        [ "$PI_TYPE" = "pi5" ] && GPU=256 || GPU=128
        echo "gpu_mem=$GPU" | sudo tee -a "$CONFIG_FILE" > /dev/null
        ok "gpu_mem=$GPU"
    fi

    # Вимкнути serial console (звільнити UART для MAVLink)
    for cmdline in /boot/firmware/cmdline.txt /boot/cmdline.txt; do
        if grep -q "console=serial0" "$cmdline" 2>/dev/null; then
            sudo sed -i 's/console=serial0,[0-9]* //g' "$cmdline"
            ok "Serial console вимкнено — UART вільний для MAVLink"
            NEEDS_REBOOT=true
        fi
    done
else
    warn "config.txt не знайдено — пропуск налаштування інтерфейсів"
fi

# Watchdog daemon
if ! command -v watchdog &>/dev/null; then
    sudo apt-get install -y watchdog >> "$LOG_FILE" 2>&1
    sudo tee /etc/watchdog.conf > /dev/null << 'WDEOF'
watchdog-device = /dev/watchdog
watchdog-timeout = 15
interval = 5
WDEOF
    sudo systemctl enable watchdog >> "$LOG_FILE" 2>&1
    ok "Watchdog daemon встановлено"
else
    skip "Watchdog daemon вже є"
fi

# Swap для Pi Zero 2W
RAM_MB=$(free -m | awk '/Mem:/{print $2}')
if [ "$RAM_MB" -lt 600 ]; then
    if [ -f /etc/dphys-swapfile ]; then
        CURRENT_SWAP=$(grep 'CONF_SWAPSIZE=' /etc/dphys-swapfile | cut -d= -f2 || echo 0)
        if [ "$CURRENT_SWAP" -lt 1024 ] 2>/dev/null; then
            sudo dphys-swapfile swapoff 2>/dev/null || true
            sudo sed -i 's/CONF_SWAPSIZE=.*/CONF_SWAPSIZE=1024/' /etc/dphys-swapfile
            sudo dphys-swapfile setup >> "$LOG_FILE" 2>&1
            sudo dphys-swapfile swapon
            ok "Swap збільшено до 1024MB (Pi Zero 2W)"
        else
            skip "Swap вже $CURRENT_SWAP MB"
        fi
    fi
fi

# ════════════════════════════════════════════════════════════
# КРОК 3: Python venv + залежності
# ════════════════════════════════════════════════════════════
step "Python venv + залежності прошивки"

if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR" --system-site-packages
    ok "Створено venv: $VENV_DIR"
else
    skip "venv вже існує"
fi

source "$VENV_DIR/bin/activate"

pip install --upgrade pip >> "$LOG_FILE" 2>&1

# Firmware deps (мінімальний набір для Pi)
pip install \
    numpy \
    opencv-python-headless \
    pymavlink \
    pyserial \
    flask \
    flask-cors \
    flask-socketio \
    eventlet \
    requests \
    >> "$LOG_FILE" 2>&1

ok "Python залежності встановлені"

# Перевірка
python3 -c "import cv2; print(f'  OpenCV: {cv2.__version__}')" 2>/dev/null && ok "OpenCV OK" || warn "OpenCV недоступний"
python3 -c "from pymavlink import mavutil; print('OK')" 2>/dev/null && ok "pymavlink OK" || warn "pymavlink недоступний"

deactivate

# ════════════════════════════════════════════════════════════
# КРОК 4: Systemd сервіс (firmware)
# ════════════════════════════════════════════════════════════
step "Systemd сервіс visual-homing.service"

SERVICE_FILE="/etc/systemd/system/visual-homing.service"

sudo tee "$SERVICE_FILE" > /dev/null << SVCEOF
[Unit]
Description=Visual Homing Navigation System
After=network.target
StartLimitIntervalSec=60
StartLimitBurst=5

[Service]
Type=simple
User=$CURRENT_USER
WorkingDirectory=$FIRMWARE_DIR
Environment=PYTHONPATH=$VH_DIR
ExecStart=$VENV_DIR/bin/python3 $FIRMWARE_DIR/main.py --web
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SVCEOF

sudo systemctl daemon-reload
sudo systemctl enable visual-homing >> "$LOG_FILE" 2>&1
ok "visual-homing.service створено та увімкнено"

# ════════════════════════════════════════════════════════════
# КРОК 5: Права доступу
# ════════════════════════════════════════════════════════════
step "Права доступу до UART, I2C, SPI, Camera"

GROUPS_TO_ADD=(dialout i2c spi video gpio)
for grp in "${GROUPS_TO_ADD[@]}"; do
    if getent group "$grp" &>/dev/null; then
        if ! groups "$CURRENT_USER" | grep -qw "$grp"; then
            sudo usermod -aG "$grp" "$CURRENT_USER"
            ok "Додано в групу: $grp"
        else
            skip "Вже в групі: $grp"
        fi
    fi
done

# ════════════════════════════════════════════════════════════
# КРОК 6: Вимкнути непотрібні сервіси
# ════════════════════════════════════════════════════════════
step "Оптимізація системи"

for svc in bluetooth hciuart avahi-daemon triggerhappy; do
    systemctl is-enabled "$svc" &>/dev/null && {
        sudo systemctl disable "$svc" >> "$LOG_FILE" 2>&1
        ok "Вимкнено: $svc"
    } || skip "$svc вже вимкнено"
done

# ════════════════════════════════════════════════════════════
# КРОК 7: Мережа
# ════════════════════════════════════════════════════════════
step "Мережева інформація"

IP_ADDR=$(hostname -I 2>/dev/null | awk '{print $1}')
HOSTNAME=$(hostname)

if [ -n "$IP_ADDR" ]; then
    ok "IP адреса: $IP_ADDR"
    ok "Веб-інтерфейс Pi: http://$IP_ADDR:5000"
    ok "Або: http://$HOSTNAME.local:5000"
else
    warn "IP не визначено (Wi-Fi підключено?)"
fi

# ════════════════════════════════════════════════════════════
# КРОК 8: Фінальна перевірка
# ════════════════════════════════════════════════════════════
step "Завершення"

echo ""
echo -e "${GREEN}${BOLD}"
echo "  ╔══════════════════════════════════════════════════╗"
echo "  ║       Visual Homing встановлено успішно!          ║"
echo "  ╚══════════════════════════════════════════════════╝"
echo -e "${NC}"

echo -e "  ${BOLD}Зведення:${NC}"
echo -e "    Платформа:    $PI_MODEL"
echo -e "    Python venv:  ${GREEN}$VENV_DIR${NC}"
echo -e "    Firmware:     ${GREEN}$FIRMWARE_DIR${NC}"
echo -e "    Systemd:      ${GREEN}visual-homing.service${NC}"
echo -e "    UART (MAVLink): ${GREEN}/dev/serial0${NC}"
echo -e "    UART2 (Flow): ${GREEN}/dev/ttyAMA2${NC}"
echo -e "    UART3 (LiDAR): ${GREEN}/dev/ttyAMA3${NC}"

echo ""
echo -e "  ${BOLD}ArduPilot параметри:${NC}"
echo -e "    firmware/config/visual_homing.param"

echo ""
echo -e "  ${BOLD}Корисні команди:${NC}"
echo -e "    sudo systemctl status visual-homing"
echo -e "    sudo systemctl restart visual-homing"
echo -e "    journalctl -u visual-homing -f"
echo -e "    ./update.sh   (оновлення з GitHub)"

if $NEEDS_REBOOT; then
    echo ""
    echo -e "  ${YELLOW}${BOLD}ПОТРІБНЕ ПЕРЕЗАВАНТАЖЕННЯ!${NC}"
    echo -e "  ${YELLOW}Змінено UART/I2C/SPI в config.txt${NC}"
    echo ""
    read -p "  Перезавантажити зараз? (y/n): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "  ${CYAN}Перезавантаження через 3с...${NC}"
        sleep 3
        sudo reboot
    else
        echo -e "  ${YELLOW}Після reboot: sudo systemctl start visual-homing${NC}"
    fi
else
    echo ""
    info "Запуск visual-homing..."
    sudo systemctl restart visual-homing
    sleep 5
    systemctl is-active --quiet visual-homing \
        && ok "Visual Homing працює!" \
        || warn "Сервіс не запустився. Перевір: journalctl -u visual-homing -n 30"
fi

echo ""
echo -e "${CYAN}Лог: $LOG_FILE${NC}"
echo ""
