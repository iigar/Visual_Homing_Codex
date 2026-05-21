package ua.visualhoming.app.domain.model

data class RoutePoint(
    val x: Double = 0.0,
    val y: Double = 0.0,
    val z: Double = 0.0,
    val yaw: Double = 0.0,
    val timestamp: Double = 0.0,
    val isKeyframe: Boolean = false
)

data class Route(
    val id: String,
    val name: String,
    val points: List<RoutePoint> = emptyList(),
    val keyframes: List<RoutePoint> = emptyList(),
    val totalDistance: Double = 0.0,
    val createdAt: String = "",
    val isSynced: Boolean = true
)
