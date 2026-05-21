package ua.visualhoming.app.ui.screens.home

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import ua.visualhoming.app.data.preferences.AppPreferences
import ua.visualhoming.app.data.repository.PiRepository
import ua.visualhoming.app.domain.model.ConnectionState
import ua.visualhoming.app.domain.model.PiStatus
import ua.visualhoming.app.network.PiDiscoveryManager

class HomeViewModel(
    private val piRepository: PiRepository,
    private val preferences: AppPreferences,
    private val discoveryManager: PiDiscoveryManager
) : ViewModel() {

    val piStatus: StateFlow<PiStatus> = discoveryManager.piStatus
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), PiStatus())

    fun startDiscovery() = discoveryManager.startDiscovery()
    fun stopDiscovery() = discoveryManager.stopDiscovery()

    fun connectManually(ip: String) {
        discoveryManager.setManualIp(ip)
    }

    fun verifyConnection() {
        viewModelScope.launch {
            val result = piRepository.getStatus()
            if (result.isFailure) discoveryManager.setOffline()
        }
    }
}
