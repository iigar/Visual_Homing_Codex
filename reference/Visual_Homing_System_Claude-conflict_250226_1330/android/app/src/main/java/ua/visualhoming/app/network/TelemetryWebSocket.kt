package ua.visualhoming.app.network

import com.google.gson.Gson
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import ua.visualhoming.app.data.api.dto.TelemetryMessageDto
import ua.visualhoming.app.domain.model.DronePosition
import ua.visualhoming.app.domain.model.SensorStatus
import ua.visualhoming.app.domain.model.SmartRtlStatus
import ua.visualhoming.app.domain.model.Telemetry
import java.util.concurrent.TimeUnit

class TelemetryWebSocket(private val apiClient: ApiClient) {
    private val _telemetry = MutableStateFlow(Telemetry())
    val telemetry: StateFlow<Telemetry> = _telemetry

    private val _connected = MutableStateFlow(false)
    val connected: StateFlow<Boolean> = _connected

    private var webSocket: WebSocket? = null
    private val gson = Gson()

    private val client = OkHttpClient.Builder()
        .pingInterval(15, TimeUnit.SECONDS)
        .build()

    fun connect() {
        val url = apiClient.getWebSocketUrl()
        val request = Request.Builder().url(url).build()
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(ws: WebSocket, response: Response) {
                _connected.value = true
            }

            override fun onMessage(ws: WebSocket, text: String) {
                try {
                    val msg = gson.fromJson(text, TelemetryMessageDto::class.java)
                    _telemetry.value = Telemetry(
                        position = msg.position?.let { p ->
                            DronePosition(p.x, p.y, p.z, p.yaw, p.pitch, p.roll, p.speed, p.mode)
                        } ?: _telemetry.value.position,
                        sensors = msg.sensors?.let { s ->
                            SensorStatus(
                                s.opticalFlowConnected, s.opticalFlowQuality,
                                s.flowX, s.flowY, s.lidarConnected, s.lidarDistanceM, s.lidarSignal
                            )
                        } ?: _telemetry.value.sensors,
                        smartRtl = msg.smartRtl?.let { r ->
                            SmartRtlStatus(
                                r.active, r.phase, r.currentAltitude,
                                r.homeDistance, r.returnProgress, r.navSource, r.targetAltitude
                            )
                        } ?: _telemetry.value.smartRtl,
                        timestamp = msg.timestamp
                    )
                } catch (_: Exception) {}
            }

            override fun onClosed(ws: WebSocket, code: Int, reason: String) {
                _connected.value = false
            }

            override fun onFailure(ws: WebSocket, t: Throwable, response: Response?) {
                _connected.value = false
            }
        })
    }

    fun disconnect() {
        webSocket?.close(1000, "Disconnect")
        webSocket = null
        _connected.value = false
    }
}
