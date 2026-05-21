# C++ реалізація Visual Homing

## Архітектура

```
┌─────────────────────────────────────────────────────────┐
│                 Visual Homing (C++)                       │
└─────────────────────────────────────────────────────────┘
                    │
         ┌──────────┼──────────┐
         ▼          ▼          ▼
    ┌───────┐  ┌────────┐  ┌────────┐
    │Camera │  │ Vision │  │MAVLink │
    │Module │  │ Engine │  │Comms   │
    └───────┘  └────────┘  └────────┘

Бібліотеки:
- OpenCV 4.x
- MAVLink C headers
- libserial
```

## Структура файлів

```
visual_homing_cpp/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── camera.cpp
│   ├── camera.hpp
│   ├── feature_tracker.cpp
│   ├── feature_tracker.hpp
│   ├── visual_odometry.cpp
│   ├── visual_odometry.hpp
│   ├── mavlink_interface.cpp
│   ├── mavlink_interface.hpp
│   ├── route_memory.cpp
│   └── route_memory.hpp
└── include/
    └── mavlink/
```

## Залежності

```bash
# На Raspberry Pi Zero 2 W
sudo apt install -y \
    build-essential \
    cmake \
    libopencv-dev \
    libserial-dev

# MAVLink headers
git clone https://github.com/mavlink/c_library_v2.git
mv c_library_v2 include/mavlink
```

## Файли імплементації

Дивіться код у директорії `/firmware/cpp/`
