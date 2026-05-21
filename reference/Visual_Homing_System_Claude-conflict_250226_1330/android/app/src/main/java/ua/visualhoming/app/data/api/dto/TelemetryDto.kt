package ua.visualhoming.app.data.api.dto

import com.google.gson.annotations.SerializedName

data class DronePositionDto(
    val x: Double = 0.0,
    val y: Double = 0.0,
    val z: Double = 0.0,
    val yaw: Double = 0.0,
    val pitch: Double = 0.0,
    val roll: Double = 0.0,
    val speed: Double = 0.0,
    val mode: String = "IDLE"
)

data class SensorStatusDto(
    @SerializedName("optical_flow_connected") val opticalFlowConnected: Boolean = false,
    @SerializedName("optical_flow_quality") val opticalFlowQuality: Int = 0,
    @SerializedName("flow_x") val flowX: Double = 0.0,
    @SerializedName("flow_y") val flowY: Double = 0.0,
    @SerializedName("lidar_connected") val lidarConnected: Boolean = false,
    @SerializedName("lidar_distance_m") val lidarDistanceM: Double = 0.0,
    @SerializedName("lidar_signal") val lidarSignal: Int = 0
)

data class SmartRtlStatusDto(
    val active: Boolean = false,
    val phase: String = "idle",
    @SerializedName("current_altitude") val currentAltitude: Double = 0.0,
    @SerializedName("home_distance") val homeDistance: Double = 0.0,
    @SerializedName("return_progress") val returnProgress: Double = 0.0,
    @SerializedName("nav_source") val navSource: String = "none",
    @SerializedName("target_altitude") val targetAltitude: Double = 0.0
)

data class TelemetryMessageDto(
    val type: String = "",
    val timestamp: String = "",
    val position: DronePositionDto? = null,
    val sensors: SensorStatusDto? = null,
    @SerializedName("smart_rtl") val smartRtl: SmartRtlStatusDto? = null
)

data class PiStatusDto(
    @SerializedName("mavlink_connected") val mavlinkConnected: Boolean = false,
    @SerializedName("vis_odom_healthy") val visOdomHealthy: Boolean = false,
    @SerializedName("keyframe_count") val keyframeCount: Int = 0,
    val recording: Boolean = false,
    @SerializedName("return_active") val returnActive: Boolean = false,
    val mode: String = "IDLE"
)
