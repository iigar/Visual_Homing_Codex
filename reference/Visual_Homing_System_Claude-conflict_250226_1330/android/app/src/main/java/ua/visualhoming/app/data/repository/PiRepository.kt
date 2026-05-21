package ua.visualhoming.app.data.repository

import ua.visualhoming.app.data.api.dto.PiStatusDto
import ua.visualhoming.app.data.api.dto.SettingsDto
import ua.visualhoming.app.data.preferences.AppPreferences
import ua.visualhoming.app.network.ApiClient

class PiRepository(
    private val apiClient: ApiClient,
    private val preferences: AppPreferences
) {
    suspend fun getStatus(): Result<PiStatusDto> = runCatching {
        apiClient.getPiService().getStatus()
    }

    suspend fun startRecording(): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().startRecording()
    }

    suspend fun stop(): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().stop()
    }

    suspend fun startReturn(): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().startReturn()
    }

    suspend fun stopReturn(): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().stopReturn()
    }

    suspend fun getSettings(): Result<SettingsDto> = runCatching {
        apiClient.getPiService().getSettings()
    }

    suspend fun saveSettings(settings: SettingsDto): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().saveSettings(settings)
    }

    fun getVideoStreamUrl() = apiClient.getVideoStreamUrl()
}
