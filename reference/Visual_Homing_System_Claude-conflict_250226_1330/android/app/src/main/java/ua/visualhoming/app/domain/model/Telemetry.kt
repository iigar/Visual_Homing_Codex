package ua.visualhoming.app.domain.model

data class DronePosition(
    val x: Double = 0.0,
    val y: Double = 0.0,
    val z: Double = 0.0,
    val yaw: Double = 0.0,
    val pitch: Double = 0.0,
    val roll: Double = 0.0,
    val speed: Double = 0.0,
    val mode: String = "IDLE"
)

data class SensorStatus(
    val opticalFlowConnected: Boolean = false,
    val opticalFlowQuality: Int = 0,
    val flowX: Double = 0.0,
    val flowY: Double = 0.0,
    val lidarConnected: Boolean = false,
    val lidarDistanceM: Double = 0.0,
    val lidarSignal: Int = 0
)

data class SmartRtlStatus(
    val active: Boolean = false,
    val phase: String = "idle",
    val currentAltitude: Double = 0.0,
    val homeDistance: Double = 0.0,
    val returnProgress: Double = 0.0,
    val navSource: String = "none",
    val targetAltitude: Double = 0.0
)

data class Telemetry(
    val position: DronePosition = DronePosition(),
    val sensors: SensorStatus = SensorStatus(),
    val smartRtl: SmartRtlStatus = SmartRtlStatus(),
    val keyframeCount: Int = 0,
    val mavlinkConnected: Boolean = false,
    val visOdomHealthy: Boolean = false,
    val timestamp: String = ""
)
