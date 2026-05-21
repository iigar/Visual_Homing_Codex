# Visual Homing Android App

## Концепція

Android додаток для керування Visual Homing системою. Працює як мобільна точка доступу та оптимізований інтерфейс управління.

## Основні функції

### 1. Mobile Hotspot Management
- Автоматичний запуск мобільної точки доступу
- Raspberry Pi та інші пристрої підключаються до цієї точки
- Відображення списку підключених пристроїв
- Автоматичне виявлення Pi в мережі (mDNS)

### 2. Оптимізований інтерфейс
- Native Android UI (не WebView)
- Offline-first архітектура
- Оптимізовано для польових умов (яскраве сонце, рукавички)

### 3. Основні екрани

```
┌─────────────────────────────────────┐
│  Visual Homing                   ≡  │
├─────────────────────────────────────┤
│  ┌─────────────────────────────┐    │
│  │                             │    │
│  │      3D Route Map           │    │
│  │      (OpenGL ES)            │    │
│  │                             │    │
│  └─────────────────────────────┘    │
│                                     │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐   │
│  │ ALT │ │ SPD │ │ BAT │ │ SAT │   │
│  │ 45m │ │12m/s│ │ 78% │ │  12 │   │
│  └─────┘ └─────┘ └─────┘ └─────┘   │
│                                     │
│  ┌───────────┐  ┌───────────────┐   │
│  │   START   │  │   SMART RTL   │   │
│  │ RECORDING │  │    ▶ START    │   │
│  └───────────┘  └───────────────┘   │
│                                     │
│  [Map] [Telemetry] [Camera] [⚙️]    │
└─────────────────────────────────────┘
```

## Архітектура

```
┌─────────────────────────────────────────────────────────────┐
│                     Android Device                           │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Visual Homing App                       │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐             │    │
│  │  │   UI    │  │ Hotspot │  │   API   │             │    │
│  │  │ Layer   │  │ Manager │  │ Client  │             │    │
│  │  └────┬────┘  └────┬────┘  └────┬────┘             │    │
│  │       │            │            │                   │    │
│  │       └────────────┴────────────┘                   │    │
│  │                    │                                │    │
│  │              Local Database                         │    │
│  │              (Room/SQLite)                          │    │
│  └─────────────────────────────────────────────────────┘    │
│                         │                                    │
│               WiFi Hotspot (192.168.43.x)                    │
└─────────────────────────┬───────────────────────────────────┘
                          │
          ┌───────────────┴───────────────┐
          │                               │
    ┌─────┴─────┐                   ┌─────┴─────┐
    │ Raspberry │                   │    PC     │
    │    Pi     │                   │ (Browser) │
    │           │                   │           │
    │ visual-   │                   │           │
    │ homing.   │                   │           │
    │ local     │                   │           │
    └───────────┘                   └───────────┘
```

## Технічний стек

### Frontend (Android)
- **Kotlin** - основна мова
- **Jetpack Compose** - UI framework
- **Material 3** - дизайн система
- **OpenGL ES 3.0** - 3D візуалізація карти

### Backend Integration
- **Retrofit** - HTTP клієнт
- **OkHttp** - WebSocket для real-time
- **Kotlin Coroutines** - асинхронність

### Local Storage
- **Room** - локальна база даних
- **DataStore** - налаштування

### Hotspot Management
- **WifiManager** - керування WiFi
- **TetheringManager** (API 30+) - програмний hotspot

## Ключові модулі

### 1. HotspotManager
```kotlin
class HotspotManager(context: Context) {
    
    // Запуск точки доступу
    suspend fun startHotspot(
        ssid: String = "VisualHoming",
        password: String = "drone12345"
    ): Result<HotspotConfig>
    
    // Зупинка точки доступу
    suspend fun stopHotspot()
    
    // Список підключених пристроїв
    fun getConnectedDevices(): Flow<List<ConnectedDevice>>
    
    // Автоматичне виявлення Pi
    suspend fun discoverPi(): PiDevice?
}
```

### 2. VisualHomingApi
```kotlin
interface VisualHomingApi {
    
    @GET("/api/health")
    suspend fun healthCheck(): HealthResponse
    
    @GET("/api/smart-rtl/status")
    suspend fun getSmartRtlStatus(): SmartRtlStatus
    
    @POST("/api/return/start")
    suspend fun startReturn(): ApiResponse
    
    @POST("/api/return/stop")
    suspend fun stopReturn(): ApiResponse
    
    @GET("/api/routes")
    suspend fun getRoutes(): List<Route>
    
    @GET("/api/telemetry")
    suspend fun getTelemetry(): TelemetryData
}
```

### 3. WebSocketClient
```kotlin
class VisualHomingWebSocket(
    private val baseUrl: String
) {
    // Real-time оновлення
    fun connect(): Flow<TelemetryUpdate>
    
    // Отримання подій RTL
    fun rtlEvents(): Flow<RtlEvent>
}
```

## Екрани додатку

### 1. Головний екран (Dashboard)
- 3D карта маршруту (OpenGL)
- Основна телеметрія (висота, швидкість, батарея)
- Кнопки управління (Start Recording, Smart RTL)
- Статус підключення до Pi

### 2. Налаштування Hotspot
- Увімкнення/вимкнення точки доступу
- SSID та пароль
- Список підключених пристроїв
- QR код для швидкого підключення

### 3. Телеметрія
- Детальна інформація про політ
- Графіки висоти, швидкості
- Статус сенсорів (Optical Flow, LiDAR)
- MAVLink з'єднання

### 4. Камера
- Live stream з Pi Camera
- Оверлей з feature points
- Запис відео

### 5. Маршрути
- Список збережених маршрутів
- Експорт (JSON, KML)
- Перегляд на карті

### 6. Налаштування
- Підключення до Pi (IP/hostname)
- Параметри камери
- Smart RTL конфігурація

## Особливості для польових умов

### 1. High Contrast Mode
```kotlin
@Composable
fun FieldModeTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = highContrastDarkColorScheme(),
        typography = largeTypography(), // Збільшений шрифт
    ) {
        content()
    }
}
```

### 2. Великі кнопки
- Мінімальний розмір touch target: 56dp
- Haptic feedback на всі дії
- Голосове підтвердження критичних команд

### 3. Offline режим
- Кешування маршрутів локально
- Робота без інтернету (тільки локальна мережа)
- Синхронізація при підключенні

## Permissions (AndroidManifest.xml)

```xml
<!-- WiFi та Hotspot -->
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_STATE" />
<uses-permission android:name="android.permission.WRITE_SETTINGS" />

<!-- Tethering (Android 10+) -->
<uses-permission android:name="android.permission.TETHER_PRIVILEGED" />

<!-- Network -->
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />

<!-- Location (для WiFi scanning) -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />

<!-- Foreground service -->
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
```

## Структура проекту

```
android/
├── app/
│   ├── src/main/
│   │   ├── java/com/visualhoming/
│   │   │   ├── MainActivity.kt
│   │   │   ├── ui/
│   │   │   │   ├── screens/
│   │   │   │   │   ├── DashboardScreen.kt
│   │   │   │   │   ├── HotspotScreen.kt
│   │   │   │   │   ├── TelemetryScreen.kt
│   │   │   │   │   ├── CameraScreen.kt
│   │   │   │   │   ├── RoutesScreen.kt
│   │   │   │   │   └── SettingsScreen.kt
│   │   │   │   ├── components/
│   │   │   │   │   ├── RouteMap3D.kt
│   │   │   │   │   ├── TelemetryCard.kt
│   │   │   │   │   └── ControlButtons.kt
│   │   │   │   └── theme/
│   │   │   │       ├── Theme.kt
│   │   │   │       └── FieldModeTheme.kt
│   │   │   ├── data/
│   │   │   │   ├── api/
│   │   │   │   │   ├── VisualHomingApi.kt
│   │   │   │   │   └── WebSocketClient.kt
│   │   │   │   ├── repository/
│   │   │   │   │   ├── TelemetryRepository.kt
│   │   │   │   │   └── RouteRepository.kt
│   │   │   │   └── local/
│   │   │   │       ├── AppDatabase.kt
│   │   │   │       └── RouteDao.kt
│   │   │   ├── hotspot/
│   │   │   │   ├── HotspotManager.kt
│   │   │   │   └── DeviceDiscovery.kt
│   │   │   └── service/
│   │   │       └── HotspotService.kt
│   │   └── res/
│   │       └── ...
│   └── build.gradle.kts
├── gradle/
└── build.gradle.kts
```

## Етапи розробки

### Phase 1: MVP (2-3 тижні)
1. Базовий UI з Jetpack Compose
2. API клієнт для Visual Homing
3. Dashboard з телеметрією
4. Кнопки управління (Start/Stop RTL)

### Phase 2: Hotspot (1-2 тижні)
1. HotspotManager реалізація
2. Автоматичне виявлення Pi
3. UI для керування точкою доступу

### Phase 3: 3D Map (2-3 тижні)
1. OpenGL ES renderer
2. Відображення маршруту
3. Анімація Smart RTL

### Phase 4: Polish (1-2 тижні)
1. Field Mode тема
2. Offline підтримка
3. Тестування на різних пристроях

## Мінімальні вимоги

- **Android:** 8.0+ (API 26+)
- **RAM:** 2GB+
- **Display:** 5" або більше
- **WiFi:** 802.11n або новіше

## Альтернативи

### Progressive Web App (PWA)
Якщо немає ресурсів на native Android:
- Оптимізувати поточний веб-інтерфейс для mobile
- Додати Service Worker для offline
- Встановлення як PWA на Android

### React Native / Flutter
- Кросплатформенність (iOS + Android)
- Швидша розробка
- Менша продуктивність для 3D

## Посилання

- [Jetpack Compose](https://developer.android.com/jetpack/compose)
- [Material 3 Android](https://m3.material.io/develop/android)
- [Android Tethering API](https://developer.android.com/reference/android/net/TetheringManager)
