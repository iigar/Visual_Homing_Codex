# Visual Homing Android App

Нативний Android додаток для керування системою Visual Homing на Raspberry Pi.

## Відкрити в Android Studio

Відкрити папку `android/` як Android проєкт (File → Open → вибрати цю папку).

## Вимоги
- Android Studio Ladybug (2024.2) або новіше
- JDK 17
- Android SDK API 35
- Android 8.0+ на пристрої (minSdk 26)

## Налаштування

1. Запустіть мобільну точку доступу на телефоні
2. Pi підключається до неї автоматично (якщо налаштовано `nmcli`)
3. Додаток шукає Pi через mDNS (`visual-homing.local:5000`)
4. Або вкажіть IP вручну: Головний екран → "Enter IP manually"

## Архітектура

```
MVVM + Clean Architecture
├── domain/model/       — доменні моделі
├── data/
│   ├── api/dto/        — Retrofit DTOs
│   ├── local/          — Room DB
│   ├── preferences/    — DataStore
│   └── repository/     — репозиторії
├── network/
│   ├── ApiClient       — Retrofit/OkHttp клієнт
│   ├── PiDiscoveryManager — mDNS (NSD)
│   └── TelemetryWebSocket — WS /ws/telemetry
└── ui/
    ├── screens/        — 7 екранів (MVVM)
    └── components/     — MjpegView, TelemetryCard...
```

## API (Pi Flask server, port 5000)

| Method | Endpoint | Опис |
|--------|----------|------|
| GET | `/video_feed` | MJPEG стрім |
| GET | `/api/status` | Статус системи |
| POST | `/api/recording/start` | Почати запис |
| POST | `/api/stop` | Зупинити |
| POST | `/api/return/start` | Smart RTL |
| GET | `/api/routes` | Список маршрутів |
| WS | `/ws/telemetry` | Телеметрія real-time |

## Збірка APK

```
./gradlew assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
```
