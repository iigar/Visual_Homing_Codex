package ua.visualhoming.app.domain.model

enum class ConnectionState { SEARCHING, CONNECTED, OFFLINE }

data class PiStatus(
    val connectionState: ConnectionState = ConnectionState.OFFLINE,
    val ipAddress: String = "",
    val piUrl: String = ""
)
