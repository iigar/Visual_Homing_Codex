package ua.visualhoming.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import ua.visualhoming.app.domain.model.Route

@Composable
fun RouteMap2D(route: Route?, modifier: Modifier = Modifier) {
    Box(modifier = modifier.background(Color(0xFF0D1117))) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val points = route?.points ?: return@Canvas
            if (points.size < 2) return@Canvas

            val xs = points.map { it.x.toFloat() }
            val ys = points.map { it.y.toFloat() }
            val minX = xs.min()
            val maxX = xs.max()
            val minY = ys.min()
            val maxY = ys.max()
            val rangeX = (maxX - minX).coerceAtLeast(1f)
            val rangeY = (maxY - minY).coerceAtLeast(1f)
            val padding = 40f

            fun toScreen(x: Float, y: Float): Offset {
                val sx = padding + (x - minX) / rangeX * (size.width - 2 * padding)
                val sy = padding + (y - minY) / rangeY * (size.height - 2 * padding)
                return Offset(sx, sy)
            }

            val path = Path()
            toScreen(xs[0], ys[0]).also { path.moveTo(it.x, it.y) }
            for (i in 1 until points.size) {
                toScreen(xs[i], ys[i]).also { path.lineTo(it.x, it.y) }
            }
            drawPath(path, Color(0xFF00BCD4), style = Stroke(width = 2f))

            // Keyframes as dots
            route.keyframes.forEach { kf ->
                drawCircle(
                    color = Color(0xFFFF9800),
                    radius = 4f,
                    center = toScreen(kf.x.toFloat(), kf.y.toFloat())
                )
            }

            // Start (green) and end (red)
            drawCircle(Color(0xFF4CAF50), 6f, toScreen(xs.first(), ys.first()))
            drawCircle(Color(0xFFCF6679), 6f, toScreen(xs.last(), ys.last()))
        }
    }
}
