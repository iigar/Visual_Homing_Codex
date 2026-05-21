# Python реалізація Visual Homing + Smart RTL

## Архітектура системи

```
+-----------------------------------------------------------+
|                   Visual Homing System v2                   |
|                       (Python)                              |
+-----------------------------------------------------------+
         |              |               |            |
         v              v               v            v
+-------------+ +-------------+ +-------------+ +----------+
|  Camera     | |  Feature    | |  MAVLink    | | Sensors  |
|  Module     | |  Tracker    | |  Interface  | | Module   |
+-------------+ +-------------+ +-------------+ +----------+
     |              |               |            |    |
     v              v               v            v    v
+---------------------------------------------+ +--------+
|              Навігація                       | | Smart  |
|   (keyframes, features, positions)           | | RTL    |
+---------------------------------------------+ +--------+
```

## Структура файлів

```
visual_homing/
|-- main.py                 # Головний запуск
|-- config.py               # Конфігурація (оновлено: сенсори, Smart RTL)
|-- camera/
|   |-- __init__.py
|   |-- usb_capture.py      # USB аналогова камера
|   +-- pi_camera.py        # Pi Camera CSI
|-- vision/
|   |-- __init__.py
|   |-- feature_detector.py # ORB/SIFT детектор
|   |-- matcher.py          # Feature matching
|   +-- visual_odometry.py  # Візуальна одометрія
|-- navigation/
|   |-- __init__.py
|   |-- route_recorder.py   # Запис маршруту
|   |-- route_follower.py   # Повернення по маршруту
|   +-- smart_rtl.py        # **НОВИЙ** Smart RTL логіка
|-- sensors/                # **НОВИЙ** Модулі сенсорів
|   |-- __init__.py
|   |-- optical_flow.py     # MATEK 3901-L0X (Optical Flow + VL53L0X)
|   +-- lidar.py            # Benewake TF-Luna LiDAR
|-- mavlink/
|   |-- __init__.py
|   +-- ardupilot.py        # MAVLink комунікація
+-- web/
    |-- __init__.py
    +-- server.py           # Flask веб-сервер
```

## Файли імплементації

Дивіться код у директорії `/firmware/python/`
