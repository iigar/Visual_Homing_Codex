package ua.visualhoming.app.ui.screens.routes

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import ua.visualhoming.app.data.api.dto.RouteDto
import ua.visualhoming.app.data.repository.RouteRepository
import ua.visualhoming.app.domain.model.Route

data class RoutesUiState(
    val localRoutes: List<Route> = emptyList(),
    val remoteRoutes: List<RouteDto> = emptyList(),
    val loading: Boolean = false,
    val message: String = ""
)

class RoutesViewModel(private val repository: RouteRepository) : ViewModel() {

    private val _uiState = MutableStateFlow(RoutesUiState())
    val uiState: StateFlow<RoutesUiState> = _uiState

    init {
        viewModelScope.launch {
            repository.localRoutes.collect { routes ->
                _uiState.update { it.copy(localRoutes = routes) }
            }
        }
    }

    fun syncFromPi() {
        viewModelScope.launch {
            _uiState.update { it.copy(loading = true) }
            repository.fetchRemoteRoutes()
                .onSuccess { routes ->
                    routes.forEach { repository.syncRoute(it) }
                    _uiState.update { it.copy(loading = false, message = "Synced ${routes.size} routes") }
                }
                .onFailure {
                    _uiState.update { it.copy(loading = false, message = "Sync failed: ${it.message}") }
                }
        }
    }

    fun deleteLocal(id: String) {
        viewModelScope.launch { repository.deleteLocalRoute(id) }
    }

    fun uploadToPi(route: Route) {
        viewModelScope.launch {
            repository.uploadRoute(route)
                .onSuccess { _uiState.update { it.copy(message = "Uploaded: ${route.name}") } }
                .onFailure { _uiState.update { it.copy(message = "Upload failed: ${it.message}") } }
        }
    }
}
