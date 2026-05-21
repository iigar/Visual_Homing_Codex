package ua.visualhoming.app.ui.screens.dashboard

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.navigation.NavController
import ua.visualhoming.app.ui.components.MjpegView
import ua.visualhoming.app.ui.components.StatusCard
import ua.visualhoming.app.ui.components.TelemetryCard

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(vm: DashboardViewModel, navController: NavController) {
    val state by vm.uiState.collectAsState()
    val pos = state.telemetry.position
    val sensors = state.telemetry.sensors
    val rtl = state.telemetry.smartRtl

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Dashboard") },
                navigationIcon = {
                    IconButton(onClick = { navController.popBackStack() }) { Icon(Icons.Default.ArrowBack, null) }
                },
                actions = {
                    Icon(
                        if (state.wsConnected) Icons.Default.Wifi else Icons.Default.WifiOff,
                        null,
                        tint = if (state.wsConnected) Color(0xFF4CAF50) else MaterialTheme.colorScheme.error
                    )
                    Spacer(Modifier.width(8.dp))
                }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
        ) {
            // Video feed
            MjpegView(
                streamUrl = vm.videoStreamUrl,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(220.dp)
            )

            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                // Status message
                if (state.statusMessage.isNotEmpty()) {
                    Text(state.statusMessage, color = MaterialTheme.colorScheme.secondary)
                }

                // Control buttons
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Button(
                        onClick = { if (state.recording) vm.stop() else vm.startRecording() },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (state.recording) MaterialTheme.colorScheme.error else Color(0xFF4CAF50)
                        )
                    ) {
                        Icon(if (state.recording) Icons.Default.Stop else Icons.Default.FiberManualRecord, null)
                        Spacer(Modifier.width(4.dp))
                        Text(if (state.recording) "STOP" else "RECORD")
                    }
                    Button(
                        onClick = { vm.returnHome() },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary),
                        enabled = !state.returning
                    ) {
                        Icon(Icons.Default.Home, null)
                        Spacer(Modifier.width(4.dp))
                        Text("RETURN")
                    }
                }

                // RTL progress
                if (rtl.active) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text("RTL: ${rtl.phase.uppercase()} — nav: ${rtl.navSource}")
                        LinearProgressIndicator(
                            progress = { rtl.returnProgress.toFloat() },
                            modifier = Modifier.fillMaxWidth()
                        )
                        Text("${(rtl.returnProgress * 100).toInt()}% — alt: ${"%.1f".format(rtl.currentAltitude)}m — dist: ${"%.0f".format(rtl.homeDistance)}m")
                    }
                }

                // Position telemetry
                Text("Position", style = MaterialTheme.typography.titleLarge)
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TelemetryCard("X", "%.2f".format(pos.x), "m", Modifier.weight(1f))
                    TelemetryCard("Y", "%.2f".format(pos.y), "m", Modifier.weight(1f))
                    TelemetryCard("ALT", "%.1f".format(pos.z), "m", Modifier.weight(1f))
                }
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TelemetryCard("ROLL", "%.1f".format(pos.roll), "°", Modifier.weight(1f))
                    TelemetryCard("PITCH", "%.1f".format(pos.pitch), "°", Modifier.weight(1f))
                    TelemetryCard("YAW", "%.1f".format(pos.yaw), "°", Modifier.weight(1f))
                }

                // System status
                Text("System Status", style = MaterialTheme.typography.titleLarge)
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    StatusCard("MAVLink", state.telemetry.mavlinkConnected, Modifier.weight(1f))
                    StatusCard("VisOdom", state.telemetry.visOdomHealthy, Modifier.weight(1f))
                    StatusCard("Flow", sensors.opticalFlowConnected, Modifier.weight(1f))
                    StatusCard("LiDAR", sensors.lidarConnected, Modifier.weight(1f))
                }

                // Sensor values
                if (sensors.opticalFlowConnected) {
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        TelemetryCard("Flow Q", "${sensors.opticalFlowQuality}", "%", Modifier.weight(1f))
                        TelemetryCard("Flow X", "%.2f".format(sensors.flowX), "", Modifier.weight(1f))
                        TelemetryCard("Flow Y", "%.2f".format(sensors.flowY), "", Modifier.weight(1f))
                    }
                }
                if (sensors.lidarConnected) {
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        TelemetryCard("LiDAR", "%.2f".format(sensors.lidarDistanceM), "m", Modifier.weight(1f))
                        TelemetryCard("Signal", "${sensors.lidarSignal}", "", Modifier.weight(1f))
                        Spacer(Modifier.weight(1f))
                    }
                }

                // Mode chip
                Surface(
                    color = MaterialTheme.colorScheme.surfaceVariant,
                    shape = MaterialTheme.shapes.small
                ) {
                    Text("Mode: ${pos.mode}", modifier = Modifier.padding(horizontal = 12.dp, vertical = 6.dp))
                }
            }
        }
    }
}

