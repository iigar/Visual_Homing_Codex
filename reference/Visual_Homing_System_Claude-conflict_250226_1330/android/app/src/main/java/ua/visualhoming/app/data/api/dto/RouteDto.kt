package ua.visualhoming.app.data.api.dto

import com.google.gson.annotations.SerializedName

data class RoutePointDto(
    val x: Double = 0.0,
    val y: Double = 0.0,
    val z: Double = 0.0,
    val yaw: Double = 0.0,
    val timestamp: Double = 0.0,
    @SerializedName("is_keyframe") val isKeyframe: Boolean = false
)

data class RouteDto(
    val id: String = "",
    val name: String = "",
    val points: List<RoutePointDto> = emptyList(),
    val keyframes: List<RoutePointDto> = emptyList(),
    @SerializedName("total_distance") val totalDistance: Double = 0.0,
    @SerializedName("created_at") val createdAt: String = ""
)
