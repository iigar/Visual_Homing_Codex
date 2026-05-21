package ua.visualhoming.app.ui.screens.dashboard

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import ua.visualhoming.app.data.repository.PiRepository
import ua.visualhoming.app.domain.model.Telemetry
import ua.visualhoming.app.network.TelemetryWebSocket

data class DashboardUiState(
    val telemetry: Telemetry = Telemetry(),
    val wsConnected: Boolean = false,
    val recording: Boolean = false,
    val returning: Boolean = false,
    val statusMessage: String = ""
)

class DashboardViewModel(
    private val piRepository: PiRepository,
    private val webSocket: TelemetryWebSocket
) : ViewModel() {

    private val _uiState = MutableStateFlow(DashboardUiState())
    val uiState: StateFlow<DashboardUiState> = _uiState

    val videoStreamUrl: String get() = piRepository.getVideoStreamUrl()

    init {
        webSocket.connect()
        viewModelScope.launch {
            webSocket.telemetry.collect { telemetry ->
                _uiState.update { it.copy(telemetry = telemetry) }
            }
        }
        viewModelScope.launch {
            webSocket.connected.collect { connected ->
                _uiState.update { it.copy(wsConnected = connected) }
            }
        }
    }

    fun startRecording() {
        viewModelScope.launch {
            piRepository.startRecording()
                .onSuccess { _uiState.update { it.copy(recording = true, statusMessage = "Recording started") } }
                .onFailure { err -> _uiState.update { it.copy(statusMessage = "Failed: ${err.message}") } }
        }
    }

    fun stop() {
        viewModelScope.launch {
            piRepository.stop()
                .onSuccess { _uiState.update { it.copy(recording = false, returning = false, statusMessage = "Stopped") } }
                .onFailure { err -> _uiState.update { it.copy(statusMessage = "Failed: ${err.message}") } }
        }
    }

    fun returnHome() {
        viewModelScope.launch {
            piRepository.startReturn()
                .onSuccess { _uiState.update { it.copy(returning = true, statusMessage = "Return to home started") } }
                .onFailure { err -> _uiState.update { it.copy(statusMessage = "Failed: ${err.message}") } }
        }
    }

    override fun onCleared() {
        webSocket.disconnect()
    }
}
