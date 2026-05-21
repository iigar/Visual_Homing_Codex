package ua.visualhoming.app.ui.screens.routes

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.navigation.NavController
import ua.visualhoming.app.domain.model.Route
import ua.visualhoming.app.ui.components.RouteMap2D

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RoutesScreen(vm: RoutesViewModel, navController: NavController) {
    val state by vm.uiState.collectAsState()
    var selectedRoute by remember { mutableStateOf<Route?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Routes") },
                navigationIcon = { IconButton(onClick = { navController.popBackStack() }) { Icon(Icons.Default.ArrowBack, null) } },
                actions = {
                    IconButton(onClick = { vm.syncFromPi() }) {
                        if (state.loading) CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                        else Icon(Icons.Default.Sync, "Sync from Pi")
                    }
                }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier.fillMaxSize().padding(padding)
        ) {
            if (state.message.isNotEmpty()) {
                Text(
                    state.message,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    color = MaterialTheme.colorScheme.secondary
                )
            }

            // Map preview
            selectedRoute?.let { route ->
                RouteMap2D(
                    route = route,
                    modifier = Modifier.fillMaxWidth().height(200.dp)
                )
                Divider()
            }

            if (state.localRoutes.isEmpty()) {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Icon(Icons.Default.Route, null, Modifier.size(48.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
                        Spacer(Modifier.height(8.dp))
                        Text("No routes saved", color = MaterialTheme.colorScheme.onSurfaceVariant)
                        Spacer(Modifier.height(8.dp))
                        Button(onClick = { vm.syncFromPi() }) { Text("Sync from Pi") }
                    }
                }
            } else {
                LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(state.localRoutes, key = { it.id }) { route ->
                        RouteItem(
                            route = route,
                            selected = selectedRoute?.id == route.id,
                            onClick = { selectedRoute = if (selectedRoute?.id == route.id) null else route },
                            onDelete = { vm.deleteLocal(route.id) },
                            onUpload = { vm.uploadToPi(route) }
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun RouteItem(
    route: Route,
    selected: Boolean,
    onClick: () -> Unit,
    onDelete: () -> Unit,
    onUpload: () -> Unit
) {
    Card(
        onClick = onClick,
        border = if (selected) CardDefaults.outlinedCardBorder() else null
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(route.name, style = MaterialTheme.typography.titleLarge)
                Text(
                    "${route.keyframes.size} keyframes · ${"%.1f".format(route.totalDistance)}m · ${if (route.isSynced) "Synced" else "Local"}",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.bodySmall
                )
            }
            IconButton(onClick = onUpload) { Icon(Icons.Default.Upload, "Upload") }
            IconButton(onClick = onDelete) { Icon(Icons.Default.Delete, "Delete", tint = MaterialTheme.colorScheme.error) }
        }
    }
}

