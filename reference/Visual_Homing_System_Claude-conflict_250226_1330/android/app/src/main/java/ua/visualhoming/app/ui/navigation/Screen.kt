package ua.visualhoming.app.ui.navigation

sealed class Screen(val route: String) {
    object Splash : Screen("splash")
    object Home : Screen("home")
    object Dashboard : Screen("dashboard")
    object Routes : Screen("routes")
    object Settings : Screen("settings")
    object Docs : Screen("docs")
    object Hotspot : Screen("hotspot")
}
