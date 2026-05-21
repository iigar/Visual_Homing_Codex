#!/bin/bash
# ============================================================================
# Visual Homing System — Швидке оновлення
# ============================================================================
# Використання:  cd ~/visual-homing && ./update.sh
# ============================================================================

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

VH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$VH_DIR/firmware/python"
VENV_DIR="$VH_DIR/venv"

# ─── Платформа ──────────────────────────────────────────────
RAM_MB=$(free -m | awk '/Mem:/{print $2}')
PI_MODEL="Unknown"
[ -f /proc/device-tree/model ] && PI_MODEL=$(cat /proc/device-tree/model | tr -d '\0')

echo -e "${CYAN}${BOLD}Visual Homing Update${NC}"
echo -e "  Pi:  $PI_MODEL"
echo -e "  RAM: ${RAM_MB}MB"
echo ""

# ─── [1/4] Git pull ──────────────────────────────────────────
echo -e "${CYAN}[1/4]${NC} git pull..."
if [ -d "$VH_DIR/.git" ]; then
    cd "$VH_DIR"
    git pull || echo -e "  ${YELLOW}git pull не вдався, продовжуємо...${NC}"
else
    echo -e "  ${YELLOW}Немає .git — пропуск${NC}"
fi

# ─── [2/4] Python залежності ─────────────────────────────────
echo -e "${CYAN}[2/4]${NC} Python залежності..."

if [ ! -d "$VENV_DIR" ]; then
    echo -e "  ${YELLOW}venv не знайдено — запусти ./setup.sh спочатку${NC}"
    exit 1
fi

source "$VENV_DIR/bin/activate"

# Оновити тільки якщо є changes у pip (через --upgrade)
pip install --upgrade --quiet \
    numpy \
    opencv-python-headless \
    pymavlink \
    pyserial \
    flask \
    flask-cors \
    flask-socketio \
    eventlet \
    requests \
    2>&1 | grep -E "Successfully|already" | head -5 || true

echo -e "  ${GREEN}OK${NC}"
deactivate

# ─── [3/4] Перевірка конфігурації ───────────────────────────
echo -e "${CYAN}[3/4]${NC} Перевірка..."

# Перевірити UART
[ -e /dev/serial0 ]  && echo -e "  ${GREEN}UART (MAVLink) /dev/serial0 — OK${NC}"  || echo -e "  ${YELLOW}UART serial0 не знайдено (reboot?)${NC}"
[ -e /dev/ttyAMA2 ] && echo -e "  ${GREEN}UART2 (Flow)   /dev/ttyAMA2 — OK${NC}"   || echo -e "  ${YELLOW}UART2 (Flow) не знайдено${NC}"
[ -e /dev/ttyAMA3 ] && echo -e "  ${GREEN}UART3 (LiDAR)  /dev/ttyAMA3 — OK${NC}"   || echo -e "  ${YELLOW}UART3 (LiDAR) не знайдено${NC}"
[ -e /dev/video0 ]  && echo -e "  ${GREEN}Камера /dev/video0 — OK${NC}"             || echo -e "  ${YELLOW}Камера не знайдена${NC}"

# ─── [4/4] Перезапуск сервісу ────────────────────────────────
echo -e "${CYAN}[4/4]${NC} Перезапуск visual-homing..."

if systemctl is-enabled --quiet visual-homing 2>/dev/null; then
    sudo systemctl restart visual-homing
    sleep 5

    if systemctl is-active --quiet visual-homing; then
        echo -e "  ${GREEN}${BOLD}visual-homing запущено${NC}"
        echo ""

        # Статус через веб-інтерфейс Pi
        if curl -s --max-time 3 http://localhost:5000/api/status > /dev/null 2>&1; then
            curl -s http://localhost:5000/api/status | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    print(f\"  State:      {d.get('state', '?')}\")
    print(f\"  Recording:  {d.get('recording', False)}\")
    print(f\"  Following:  {d.get('following', False)}\")
    print(f\"  Keyframes:  {d.get('keyframes_count', 0)}\")
    cam = d.get('camera_active', False)
    mav = d.get('mavlink_connected', False)
    print(f\"  Camera:     {'OK' if cam else 'NOT CONNECTED'}\")
    print(f\"  MAVLink:    {'CONNECTED' if mav else 'NOT CONNECTED'}\")
except Exception as e:
    print(f'  Status parse error: {e}')
" 2>/dev/null || echo -e "  ${YELLOW}Status API недоступний${NC}"
        else
            echo -e "  ${YELLOW}Web API ще стартує — зачекайте 10с або:${NC}"
            echo -e "  ${YELLOW}  curl http://localhost:5000/api/status${NC}"
        fi
    else
        echo -e "  ${RED}Сервіс не запустився!${NC}"
        echo -e "  Перевір: journalctl -u visual-homing -n 30"
        exit 1
    fi
else
    echo -e "  ${YELLOW}visual-homing.service не знайдено — запусти ./setup.sh${NC}"
fi

echo ""
echo -e "${GREEN}${BOLD}Оновлення завершено!${NC}"
echo ""
echo -e "  ${BOLD}Команди:${NC}"
echo -e "    journalctl -u visual-homing -f          — логи"
echo -e "    sudo systemctl restart visual-homing     — перезапуск"
echo -e "    curl http://localhost:5000/api/status    — статус"
echo ""
