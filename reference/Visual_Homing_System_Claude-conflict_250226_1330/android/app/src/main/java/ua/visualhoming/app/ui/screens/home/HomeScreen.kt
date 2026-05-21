package ua.visualhoming.app.ui.screens.home

import android.content.Intent
import android.provider.Settings
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.navigation.NavController
import ua.visualhoming.app.domain.model.ConnectionState
import ua.visualhoming.app.ui.components.ConnectionStatusBadge
import ua.visualhoming.app.ui.navigation.Screen

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(vm: HomeViewModel, navController: NavController) {
    val context = LocalContext.current
    val piStatus by vm.piStatus.collectAsState()
    var showManualDialog by remember { mutableStateOf(false) }
    var manualIp by remember { mutableStateOf("192.168.43.") }

    LaunchedEffect(Unit) { vm.startDiscovery() }
    DisposableEffect(Unit) { onDispose { vm.stopDiscovery() } }

    Scaffold(
        topBar = { TopAppBar(title = { Text("Visual Homing") }) }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Spacer(Modifier.height(16.dp))
            ConnectionStatusBadge(piStatus = piStatus)
            Spacer(Modifier.height(24.dp))

            when (piStatus.connectionState) {
                ConnectionState.CONNECTED -> {
                    Icon(Icons.Default.CheckCircle, null, Modifier.size(64.dp), tint = MaterialTheme.colorScheme.secondary)
                    Text("Raspberry Pi connected", style = MaterialTheme.typography.titleLarge)
                    Text(piStatus.piUrl, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Spacer(Modifier.height(16.dp))
                    Button(onClick = { navController.navigate(Screen.Dashboard.route) }, modifier = Modifier.fillMaxWidth()) {
                        Icon(Icons.Default.Dashboard, null)
                        Spacer(Modifier.width(8.dp))
                        Text("Open Dashboard")
                    }
                }
                ConnectionState.SEARCHING -> {
                    CircularProgressIndicator(color = MaterialTheme.colorScheme.secondary)
                    Text("Searching for Pi...", style = MaterialTheme.typography.titleLarge)
                    Text("Make sure Pi is connected to hotspot", color = MaterialTheme.colorScheme.onSurfaceVariant, textAlign = TextAlign.Center)
                }
                ConnectionState.OFFLINE -> {
                    Icon(Icons.Default.WifiOff, null, Modifier.size(64.dp), tint = MaterialTheme.colorScheme.error)
                    Text("Not connected", style = MaterialTheme.typography.titleLarge)
                }
            }

            Spacer(Modifier.weight(1f))

            OutlinedButton(onClick = {
                context.startActivity(Intent(Settings.ACTION_WIRELESS_SETTINGS).apply { addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) })
            }, modifier = Modifier.fillMaxWidth()) {
                Icon(Icons.Default.Wifi, null); Spacer(Modifier.width(8.dp)); Text("Hotspot Settings")
            }
            OutlinedButton(onClick = { showManualDialog = true }, modifier = Modifier.fillMaxWidth()) {
                Icon(Icons.Default.Edit, null); Spacer(Modifier.width(8.dp)); Text("Enter IP manually")
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { navController.navigate(Screen.Docs.route) }, modifier = Modifier.weight(1f)) {
                    Icon(Icons.Default.MenuBook, null); Spacer(Modifier.width(4.dp)); Text("Docs")
                }
                OutlinedButton(onClick = { navController.navigate(Screen.Settings.route) }, modifier = Modifier.weight(1f)) {
                    Icon(Icons.Default.Settings, null); Spacer(Modifier.width(4.dp)); Text("Settings")
                }
            }
        }
    }

    if (showManualDialog) {
        AlertDialog(
            onDismissRequest = { showManualDialog = false },
            title = { Text("Enter Pi IP address") },
            text = {
                OutlinedTextField(value = manualIp, onValueChange = { manualIp = it }, label = { Text("IP address") }, singleLine = true)
            },
            confirmButton = {
                TextButton(onClick = { vm.connectManually(manualIp.trim()); showManualDialog = false }) { Text("Connect") }
            },
            dismissButton = { TextButton(onClick = { showManualDialog = false }) { Text("Cancel") } }
        )
    }
}

