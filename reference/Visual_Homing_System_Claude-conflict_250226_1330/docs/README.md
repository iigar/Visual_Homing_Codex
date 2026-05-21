# Visual Homing System для Raspberry Pi + ArduPilot

## Огляд проекту

Система візуальної навігації (Visual Homing) для мультикоптерного дрона на базі ArduPilot.
Принцип роботи подібний до оптичної комп'ютерної мишки - система записує візуальні орієнтири
під час польоту і використовує їх для автономного повернення на точку зльоту.

## Підтримувані моделі Raspberry Pi

| Модель | RAM | Рекомендовано для |
|--------|-----|-------------------|
| **Pi Zero 2 W** | 512MB | Легкі дрони, базова навігація |
| **Pi 4B** | 2-8GB | Середні дрони, швидка обробка |
| **Pi 5** | 4-8GB | Максимальна продуктивність |

## Компоненти системи

| Компонент | Модель | Примітка |
|-----------|--------|----------|
| Комп'ютер | Raspberry Pi Zero 2 W / 4B / 5 | Залежить від дрона |
| **Камера (основна)** | **Pi Camera Module v2/v3** | **CSI інтерфейс, picamera2** |
| Термокамера | Caddx Thermal 256 (IRC-256) | Аналоговий PAL вихід |
| USB Capture | EasyCap (UTV007) | Аналог → USB |
| Політний контролер | Matek H743-Slim V3 | ArduCopter 4.5+ |
| Сенсори | MATEK 3901-L0X, TF-Luna | Optical Flow + LiDAR |

## Особливості

- **Без GPS залежності** - навігація по візуальних орієнтирах
- **Smart RTL** - інтелектуальне повернення з переключенням режимів за висотою
- **Teach & Repeat** - запис маршруту → повернення по записаному шляху
- **Веб-інтерфейс** - 3D карта, телеметрія, налаштування
- **Android підтримка** - мобільний інтерфейс для керування

## Структура документації

1. [Налаштування Raspberry Pi OS](./01_raspberry_pi_setup.md)
2. [Схеми підключення](./02_wiring_diagrams.md)
3. [Конфігурація ArduPilot](./03_ardupilot_config.md)
4. [Python версія](./04_python_implementation.md)
5. [C++ версія](./05_cpp_implementation.md)
6. [Веб-інтерфейс](./06_web_interface.md)
7. [Автозапуск системи](./07_autostart.md)
8. [Тестування та калібрування](./08_testing.md)
9. [Тестування в SITL](./09_sitl_testing.md)
10. [Android додаток](./10_android_app.md)
11. [Розширення радіусу дії](./11_long_range.md) ⭐ NEW

## Швидкий старт

### Варіант 1: Автоматична інсталяція (рекомендовано)

```bash
# На Raspberry Pi (через SSH):
cd ~
git clone https://github.com/iigar/Visual_Homing_System_Claude.git ~/visual-homing && cd ~/visual-homing && ./setup.sh
unzip -o firmware.zip -d visual_homing
```

### Варіант 2: Завантаження прошивки ZIP

```bash
cd ~
git clone https://github.com/iigar/Visual_Homing_System_Claude.git ~/visual-homing && cd ~/visual-homing && ./setup.sh
unzip firmware.zip -d visual_homing
cd visual_homing
```

### Запуск системи

```bash
cd ~/visual_homing
source ~/venv/bin/activate

# З Pi Camera (рекомендовано):
python main.py --web --camera picamera

# З USB камерою (EasyCap):
python main.py --web --camera usb

# Тестовий режим (без MAVLink):
python main.py --web --camera picamera --test-mode
```

### Доступ до веб-інтерфейсу

```
http://visual-homing.local:5000
або
http://<IP_адреса_Pi>:5000
```

## Preview URL (актуальний)

- **Веб-інтерфейс:** https://github.com/iigar/Visual_Homing_System_Claude
- **API:** https://github.com/iigar/Visual_Homing_System_Claude/api

## Ліцензія

MIT License - вільне використання для будь-яких цілей.
