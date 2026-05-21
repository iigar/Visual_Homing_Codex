package ua.visualhoming.app

import android.app.Application
import ua.visualhoming.app.data.local.AppDatabase
import ua.visualhoming.app.data.preferences.AppPreferences
import ua.visualhoming.app.data.repository.PiRepository
import ua.visualhoming.app.data.repository.RouteRepository
import ua.visualhoming.app.network.ApiClient
import ua.visualhoming.app.network.PiDiscoveryManager
import ua.visualhoming.app.network.TelemetryWebSocket

class VisualHomingApp : Application() {
    val preferences by lazy { AppPreferences(this) }
    val database by lazy { AppDatabase.getInstance(this) }
    val apiClient by lazy { ApiClient(preferences) }
    val piRepository by lazy { PiRepository(apiClient, preferences) }
    val routeRepository by lazy { RouteRepository(apiClient, database.routeDao()) }
    val discoveryManager by lazy { PiDiscoveryManager(this, preferences) }
    val telemetryWebSocket by lazy { TelemetryWebSocket(apiClient) }
}
