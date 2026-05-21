package ua.visualhoming.app.ui.screens.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import ua.visualhoming.app.data.api.dto.SettingsDto
import ua.visualhoming.app.data.preferences.AppPreferences
import ua.visualhoming.app.data.repository.PiRepository
import ua.visualhoming.app.network.PiDiscoveryManager

data class SettingsUiState(
    val piUrl: String = "http://visual-homing.local:5000",
    val remoteSettings: SettingsDto? = null,
    val loading: Boolean = false,
    val message: String = ""
)

class SettingsViewModel(
    private val piRepository: PiRepository,
    private val preferences: AppPreferences,
    private val discoveryManager: PiDiscoveryManager
) : ViewModel() {

    private val _uiState = MutableStateFlow(SettingsUiState())
    val uiState: StateFlow<SettingsUiState> = _uiState

    init {
        viewModelScope.launch {
            preferences.piUrl.collect { url ->
                _uiState.update { it.copy(piUrl = url) }
            }
        }
    }

    fun loadRemoteSettings() {
        viewModelScope.launch {
            _uiState.update { it.copy(loading = true) }
            piRepository.getSettings()
                .onSuccess { settings -> _uiState.update { it.copy(remoteSettings = settings, loading = false) } }
                .onFailure { _uiState.update { it.copy(loading = false, message = "Load failed: ${it.message}") } }
        }
    }

    fun savePiUrl(url: String) {
        viewModelScope.launch {
            preferences.setPiUrl(url)
            _uiState.update { it.copy(message = "URL saved. Reconnecting...") }
            discoveryManager.setManualIp(url.removePrefix("http://").removePrefix("https://").substringBefore(":"))
        }
    }

    fun saveRemoteSettings(settings: SettingsDto) {
        viewModelScope.launch {
            piRepository.saveSettings(settings)
                .onSuccess { _uiState.update { it.copy(message = "Settings saved on Pi") } }
                .onFailure { _uiState.update { it.copy(message = "Save failed: ${it.message}") } }
        }
    }
}
