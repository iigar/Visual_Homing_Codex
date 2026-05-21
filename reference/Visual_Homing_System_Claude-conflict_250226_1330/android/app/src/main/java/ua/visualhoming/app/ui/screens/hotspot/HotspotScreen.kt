package ua.visualhoming.app.ui.screens.hotspot

import android.content.Intent
import android.provider.Settings
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.navigation.NavController

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HotspotScreen(navController: NavController) {
    val context = LocalContext.current
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Hotspot Setup") },
                navigationIcon = { IconButton(onClick = { navController.popBackStack() }) { Icon(Icons.Default.ArrowBack, null) } }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier.fillMaxSize().padding(padding).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Card {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Step 1: Enable Mobile Hotspot", style = MaterialTheme.typography.titleLarge)
                    Text("Enable your phone's hotspot so Pi can connect.")
                    Button(onClick = {
                        context.startActivity(Intent(Settings.ACTION_WIRELESS_SETTINGS).apply { addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) })
                    }, modifier = Modifier.fillMaxWidth()) {
                        Icon(Icons.Default.Wifi, null); Spacer(Modifier.width(8.dp)); Text("Open Hotspot Settings")
                    }
                }
            }
            Card {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Step 2: Configure Pi", style = MaterialTheme.typography.titleLarge)
                    Text("SSH into Pi and run:")
                    Surface(color = MaterialTheme.colorScheme.surfaceVariant, shape = MaterialTheme.shapes.small) {
                        Text(
                            "nmcli device wifi connect \"SSID\" password \"PASS\"",
                            modifier = Modifier.padding(12.dp),
                            style = MaterialTheme.typography.bodySmall,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
            Card {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Step 3: Find Pi", style = MaterialTheme.typography.titleLarge)
                    Text("• Android hotspot: 192.168.43.x")
                    Text("• mDNS: visual-homing.local")
                    Text("• Pi web server: port 5000")
                }
            }
        }
    }
}

