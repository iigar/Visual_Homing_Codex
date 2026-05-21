package ua.visualhoming.app.ui.screens.splash

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.FlightTakeoff
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

@Composable
fun SplashScreen(onReady: () -> Unit) {
    LaunchedEffect(Unit) {
        delay(1200)
        onReady()
    }
    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Icon(Icons.Default.FlightTakeoff, null, Modifier.size(72.dp), tint = MaterialTheme.colorScheme.secondary)
            Spacer(Modifier.height(16.dp))
            Text("Visual Homing", style = MaterialTheme.typography.titleLarge, fontSize = 28.sp)
            Spacer(Modifier.height(8.dp))
            Text("Teach & Repeat Navigation", color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(32.dp))
            CircularProgressIndicator(color = MaterialTheme.colorScheme.secondary)
        }
    }
}
