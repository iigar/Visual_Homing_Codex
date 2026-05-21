package ua.visualhoming.app.ui.screens.docs

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import ua.visualhoming.app.data.api.dto.DocContentDto
import ua.visualhoming.app.data.api.dto.DocFileDto
import ua.visualhoming.app.data.repository.RouteRepository

data class DocsUiState(
    val docs: List<DocFileDto> = emptyList(),
    val selected: DocContentDto? = null,
    val loading: Boolean = false,
    val message: String = ""
)

class DocsViewModel(private val repository: RouteRepository) : ViewModel() {

    private val _uiState = MutableStateFlow(DocsUiState())
    val uiState: StateFlow<DocsUiState> = _uiState

    init {
        loadDocs()
    }

    fun loadDocs() {
        viewModelScope.launch {
            _uiState.update { it.copy(loading = true) }
            repository.getDocs()
                .onSuccess { docs -> _uiState.update { it.copy(docs = docs, loading = false) } }
                .onFailure { _uiState.update { it.copy(loading = false, message = "Load failed: ${it.message}") } }
        }
    }

    fun openDoc(filename: String) {
        viewModelScope.launch {
            _uiState.update { it.copy(loading = true) }
            repository.getDoc(filename)
                .onSuccess { content -> _uiState.update { it.copy(selected = content, loading = false) } }
                .onFailure { _uiState.update { it.copy(loading = false, message = "Load failed: ${it.message}") } }
        }
    }

    fun closeDoc() {
        _uiState.update { it.copy(selected = null) }
    }
}
