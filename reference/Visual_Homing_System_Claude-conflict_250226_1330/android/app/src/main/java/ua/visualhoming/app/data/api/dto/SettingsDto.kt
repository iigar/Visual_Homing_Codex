package ua.visualhoming.app.data.api.dto

import com.google.gson.annotations.SerializedName

data class SettingsDto(
    @SerializedName("camera_type") val cameraType: String = "usb_capture",
    @SerializedName("camera_device") val cameraDevice: String = "/dev/video0",
    @SerializedName("camera_resolution_w") val cameraResolutionW: Int = 640,
    @SerializedName("camera_resolution_h") val cameraResolutionH: Int = 480,
    @SerializedName("camera_fps") val cameraFps: Int = 30,
    @SerializedName("mavlink_port") val mavlinkPort: String = "/dev/serial0",
    @SerializedName("mavlink_baud") val mavlinkBaud: Int = 115200,
    @SerializedName("flow_enabled") val flowEnabled: Boolean = true,
    @SerializedName("lidar_enabled") val lidarEnabled: Boolean = true,
    @SerializedName("rtl_high_alt") val rtlHighAlt: Double = 50.0,
    @SerializedName("rtl_precision_alt") val rtlPrecisionAlt: Double = 5.0,
    @SerializedName("web_port") val webPort: Int = 5000,
    @SerializedName("stream_enabled") val streamEnabled: Boolean = true
)
