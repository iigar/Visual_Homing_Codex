package ua.visualhoming.app.network

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import ua.visualhoming.app.data.preferences.AppPreferences
import ua.visualhoming.app.domain.model.ConnectionState
import ua.visualhoming.app.domain.model.PiStatus

class PiDiscoveryManager(
    private val context: Context,
    private val preferences: AppPreferences
) {
    private val _piStatus = MutableStateFlow(PiStatus())
    val piStatus: StateFlow<PiStatus> = _piStatus

    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private var discoveryListener: NsdManager.DiscoveryListener? = null
    private val scope = CoroutineScope(Dispatchers.IO)

    fun startDiscovery() {
        _piStatus.value = PiStatus(connectionState = ConnectionState.SEARCHING)

        discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(regType: String) {
                Log.d("NSD", "Discovery started: $regType")
            }

            override fun onServiceFound(service: NsdServiceInfo) {
                if (service.serviceName.contains("visual-homing", ignoreCase = true) ||
                    service.serviceName.contains("raspberry", ignoreCase = true)
                ) {
                    nsdManager.resolveService(service, createResolveListener())
                }
            }

            override fun onServiceLost(service: NsdServiceInfo) {
                Log.d("NSD", "Service lost: ${service.serviceName}")
            }

            override fun onDiscoveryStopped(serviceType: String) {}
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                _piStatus.value = PiStatus(connectionState = ConnectionState.OFFLINE)
            }
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {}
        }

        try {
            nsdManager.discoverServices("_http._tcp", NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        } catch (e: Exception) {
            Log.e("NSD", "Discovery failed: ${e.message}")
        }
    }

    fun stopDiscovery() {
        discoveryListener?.let {
            try { nsdManager.stopServiceDiscovery(it) } catch (e: Exception) { }
        }
        discoveryListener = null
    }

    fun setManualIp(ip: String, port: Int = 5000) {
        val url = "http://$ip:$port"
        scope.launch {
            preferences.setPiUrl(url)
            preferences.setLastKnownIp(ip)
        }
        _piStatus.value = PiStatus(
            connectionState = ConnectionState.CONNECTED,
            ipAddress = ip,
            piUrl = url
        )
    }

    fun setConnected(ip: String, port: Int) {
        val url = "http://$ip:$port"
        scope.launch {
            preferences.setPiUrl(url)
            preferences.setLastKnownIp(ip)
        }
        _piStatus.value = PiStatus(
            connectionState = ConnectionState.CONNECTED,
            ipAddress = ip,
            piUrl = url
        )
    }

    fun setOffline() {
        _piStatus.value = PiStatus(connectionState = ConnectionState.OFFLINE)
    }

    private fun createResolveListener() = object : NsdManager.ResolveListener {
        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
            Log.e("NSD", "Resolve failed: $errorCode")
        }

        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
            val ip = serviceInfo.host?.hostAddress ?: return
            val port = serviceInfo.port
            Log.d("NSD", "Pi found at $ip:$port")
            setConnected(ip, port)
        }
    }
}
