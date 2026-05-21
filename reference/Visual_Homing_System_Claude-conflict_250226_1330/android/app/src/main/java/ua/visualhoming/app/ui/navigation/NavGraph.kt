package ua.visualhoming.app.ui.navigation

import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import ua.visualhoming.app.VisualHomingApp
import ua.visualhoming.app.ui.screens.dashboard.DashboardScreen
import ua.visualhoming.app.ui.screens.dashboard.DashboardViewModel
import ua.visualhoming.app.ui.screens.docs.DocsScreen
import ua.visualhoming.app.ui.screens.docs.DocsViewModel
import ua.visualhoming.app.ui.screens.home.HomeScreen
import ua.visualhoming.app.ui.screens.home.HomeViewModel
import ua.visualhoming.app.ui.screens.hotspot.HotspotScreen
import ua.visualhoming.app.ui.screens.routes.RoutesScreen
import ua.visualhoming.app.ui.screens.routes.RoutesViewModel
import ua.visualhoming.app.ui.screens.settings.SettingsScreen
import ua.visualhoming.app.ui.screens.settings.SettingsViewModel
import ua.visualhoming.app.ui.screens.splash.SplashScreen

@Composable
fun NavGraph() {
    val navController = rememberNavController()
    val context = LocalContext.current
    val app = context.applicationContext as VisualHomingApp

    NavHost(navController = navController, startDestination = Screen.Splash.route) {
        composable(Screen.Splash.route) {
            SplashScreen(onReady = { navController.navigate(Screen.Home.route) {
                popUpTo(Screen.Splash.route) { inclusive = true }
            }})
        }
        composable(Screen.Home.route) {
            val vm: HomeViewModel = viewModel { HomeViewModel(app.piRepository, app.preferences, app.discoveryManager) }
            HomeScreen(vm = vm, navController = navController)
        }
        composable(Screen.Dashboard.route) {
            val vm: DashboardViewModel = viewModel { DashboardViewModel(app.piRepository, app.telemetryWebSocket) }
            DashboardScreen(vm = vm, navController = navController)
        }
        composable(Screen.Routes.route) {
            val vm: RoutesViewModel = viewModel { RoutesViewModel(app.routeRepository) }
            RoutesScreen(vm = vm, navController = navController)
        }
        composable(Screen.Settings.route) {
            val vm: SettingsViewModel = viewModel { SettingsViewModel(app.piRepository, app.preferences, app.discoveryManager) }
            SettingsScreen(vm = vm, navController = navController)
        }
        composable(Screen.Docs.route) {
            val vm: DocsViewModel = viewModel { DocsViewModel(app.routeRepository) }
            DocsScreen(vm = vm, navController = navController)
        }
        composable(Screen.Hotspot.route) {
            HotspotScreen(navController = navController)
        }
    }
}
