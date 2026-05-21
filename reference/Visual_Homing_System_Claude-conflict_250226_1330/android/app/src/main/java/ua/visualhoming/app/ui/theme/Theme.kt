package ua.visualhoming.app.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val DarkColorScheme = darkColorScheme(
    primary = DarkBlue,
    secondary = Cyan,
    background = Background,
    surface = Surface,
    error = ErrorRed,
    onBackground = OnBackground,
    onSurface = OnSurface,
    onPrimary = Color.White,
    onSecondary = Color.Black,
    surfaceVariant = Color(0xFF2A2A2A),
    onSurfaceVariant = Color(0xFF909090),
)

@Composable
fun VisualHomingTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        typography = Typography,
        content = content
    )
}
