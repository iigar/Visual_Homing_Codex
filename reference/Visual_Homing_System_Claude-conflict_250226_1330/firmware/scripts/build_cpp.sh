#!/bin/bash
# Build C++ version of Visual Homing

set -e

echo "=== Збірка C++ версії ==="

# Install dependencies
echo "[Перевірка залежностей]"
sudo apt install -y build-essential cmake libopencv-dev

# Download MAVLink headers
if [ ! -d "cpp/include/mavlink" ]; then
    echo "[Завантаження MAVLink headers]"
    git clone --depth 1 https://github.com/mavlink/c_library_v2.git cpp/include/mavlink
fi

# Create build directory
echo "[Збірка]"
mkdir -p cpp/build
cd cpp/build

# Build
cmake ..
make -j$(nproc)

echo ""
echo "=== Збірка завершена! ==="
echo "Запуск: ./cpp/build/visual_homing"
