package ua.visualhoming.app.ui.screens.settings

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.navigation.NavController
import ua.visualhoming.app.data.api.dto.SettingsDto

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(vm: SettingsViewModel, navController: NavController) {
    val state by vm.uiState.collectAsState()
    var editUrl by remember(state.piUrl) { mutableStateOf(state.piUrl) }

    LaunchedEffect(Unit) { vm.loadRemoteSettings() }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings") },
                navigationIcon = { IconButton(onClick = { navController.popBackStack() }) { Icon(Icons.Default.ArrowBack, null) } }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            if (state.message.isNotEmpty()) {
                Text(state.message, color = MaterialTheme.colorScheme.secondary)
            }

            // Connection settings
            Card {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Connection", style = MaterialTheme.typography.titleLarge)
                    OutlinedTextField(
                        value = editUrl,
                        onValueChange = { editUrl = it },
                        label = { Text("Pi URL") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                    Button(
                        onClick = { vm.savePiUrl(editUrl.trim()) },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Save & Connect")
                    }
                }
            }

            // Remote settings
            state.remoteSettings?.let { settings ->
                Card {
                    Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Pi Settings", style = MaterialTheme.typography.titleLarge)

                        var cameraDevice by remember { mutableStateOf(settings.cameraDevice) }
                        var mavlinkPort by remember { mutableStateOf(settings.mavlinkPort) }
                        var rtlHighAlt by remember { mutableStateOf(settings.rtlHighAlt.toString()) }
                        var rtlPrecisionAlt by remember { mutableStateOf(settings.rtlPrecisionAlt.toString()) }

                        OutlinedTextField(value = cameraDevice, onValueChange = { cameraDevice = it }, label = { Text("Camera device") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                        OutlinedTextField(value = mavlinkPort, onValueChange = { mavlinkPort = it }, label = { Text("MAVLink port") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                        OutlinedTextField(value = rtlHighAlt, onValueChange = { rtlHighAlt = it }, label = { Text("RTL high alt (m)") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                        OutlinedTextField(value = rtlPrecisionAlt, onValueChange = { rtlPrecisionAlt = it }, label = { Text("Precision land alt (m)") }, modifier = Modifier.fillMaxWidth(), singleLine = true)

                        Button(
                            onClick = {
                                vm.saveRemoteSettings(settings.copy(
                                    cameraDevice = cameraDevice,
                                    mavlinkPort = mavlinkPort,
                                    rtlHighAlt = rtlHighAlt.toDoubleOrNull() ?: settings.rtlHighAlt,
                                    rtlPrecisionAlt = rtlPrecisionAlt.toDoubleOrNull() ?: settings.rtlPrecisionAlt
                                ))
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) { Text("Save to Pi") }
                    }
                }
            }

            if (state.loading) {
                CircularProgressIndicator()
            }
        }
    }
}

