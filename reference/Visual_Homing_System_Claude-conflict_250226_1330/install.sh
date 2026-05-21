#!/bin/bash
# =============================================================================
# Visual Homing System — Bootstrap Installer
# =============================================================================
# Запуск (без git, з нуля):
#
#   curl -fsSL https://raw.githubusercontent.com/iigar/Visual_Homing_System_Claude/main/install.sh | bash
#
# або:
#
#   wget -qO- https://raw.githubusercontent.com/iigar/Visual_Homing_System_Claude/main/install.sh | bash
#
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

ok()   { echo -e "  ${GREEN}[OK]${NC} $1"; }
warn() { echo -e "  ${YELLOW}[!]${NC} $1"; }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; exit 1; }
info() { echo -e "  $1"; }

REPO_URL="https://github.com/iigar/Visual_Homing_System_Claude.git"
INSTALL_DIR="$HOME/visual-homing"

echo ""
echo -e "${BOLD}${CYAN}"
echo "  ╔══════════════════════════════════════════════════╗"
echo "  ║       Visual Homing — Bootstrap Installer        ║"
echo "  ║     Автоматичне встановлення з нуля              ║"
echo "  ╚══════════════════════════════════════════════════╝"
echo -e "${NC}"

# Перевірка архітектури
ARCH=$(uname -m)
if [[ "$ARCH" != "aarch64" && "$ARCH" != "armv7l" ]]; then
    fail "Цей скрипт для Raspberry Pi (aarch64/armv7l). Поточна: $ARCH"
fi

# ─── КРОК 1: Встановити git і unzip ─────────────────────────
echo ""
echo -e "${CYAN}━━━ [1/3] Bootstrap: git + unzip ━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

NEED_APT=false
MISSING_PKG=()

command -v git   &>/dev/null || { MISSING_PKG+=(git);   NEED_APT=true; }
command -v unzip &>/dev/null || { MISSING_PKG+=(unzip); NEED_APT=true; }

if $NEED_APT; then
    info "Встановлення: ${MISSING_PKG[*]}"
    sudo apt-get update -qq
    sudo apt-get install -y "${MISSING_PKG[@]}"
    ok "Встановлено: ${MISSING_PKG[*]}"
else
    ok "git та unzip вже є"
fi

# ─── КРОК 2: Клонувати репозиторій ──────────────────────────
echo ""
echo -e "${CYAN}━━━ [2/3] Клонування репозиторію ━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if [ -d "$INSTALL_DIR/.git" ]; then
    info "Директорія вже існує — оновлення..."
    git -C "$INSTALL_DIR" pull --ff-only
    ok "Репозиторій оновлено: $INSTALL_DIR"
else
    if [ -d "$INSTALL_DIR" ]; then
        warn "$INSTALL_DIR існує але не є git репо — видалення..."
        rm -rf "$INSTALL_DIR"
    fi
    info "Клонування → $INSTALL_DIR"
    git clone "$REPO_URL" "$INSTALL_DIR"
    ok "Репозиторій клоновано"
fi

# ─── КРОК 3: Запустити setup.sh ─────────────────────────────
echo ""
echo -e "${CYAN}━━━ [3/3] Запуск setup.sh ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

chmod +x "$INSTALL_DIR/setup.sh"
exec bash "$INSTALL_DIR/setup.sh"
