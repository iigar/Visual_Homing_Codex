package ua.visualhoming.app.ui.components

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.concurrent.TimeUnit

@Composable
fun MjpegView(streamUrl: String, modifier: Modifier = Modifier) {
    var bitmap by remember { mutableStateOf<Bitmap?>(null) }
    var error by remember { mutableStateOf(false) }

    val client = remember {
        OkHttpClient.Builder()
            .connectTimeout(5, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .build()
    }

    LaunchedEffect(streamUrl) {
        withContext(Dispatchers.IO) {
            error = false
            try {
                val request = Request.Builder().url(streamUrl).build()
                client.newCall(request).execute().use { response ->
                    val source = response.body?.source() ?: return@withContext

                    while (isActive) {
                        val line = source.readUtf8Line() ?: break
                        if (!line.startsWith("--")) continue

                        var contentLength = 0
                        while (true) {
                            val header = source.readUtf8Line() ?: break
                            if (header.isEmpty()) break
                            if (header.lowercase().startsWith("content-length:")) {
                                contentLength = header.substringAfter(":").trim().toIntOrNull() ?: 0
                            }
                        }

                        if (contentLength > 0) {
                            val bytes = source.readByteArray(contentLength.toLong())
                            BitmapFactory.decodeByteArray(bytes, 0, bytes.size)?.let { bmp ->
                                bitmap = bmp
                            }
                        }
                    }
                }
            } catch (_: Exception) {
                error = true
            }
        }
    }

    Box(
        modifier = modifier.background(Color(0xFF0A0A0A)),
        contentAlignment = Alignment.Center
    ) {
        if (bitmap != null) {
            Image(
                bitmap = bitmap!!.asImageBitmap(),
                contentDescription = "Camera feed",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Fit
            )
        } else {
            Text(
                text = if (error) "Stream unavailable" else "Connecting...",
                color = Color.Gray
            )
        }
    }
}
