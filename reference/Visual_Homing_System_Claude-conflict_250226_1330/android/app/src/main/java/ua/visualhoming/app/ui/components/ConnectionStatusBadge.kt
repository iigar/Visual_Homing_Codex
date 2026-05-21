package ua.visualhoming.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import ua.visualhoming.app.domain.model.ConnectionState
import ua.visualhoming.app.domain.model.PiStatus

@Composable
fun ConnectionStatusBadge(piStatus: PiStatus, modifier: Modifier = Modifier) {
    val (dotColor, label) = when (piStatus.connectionState) {
        ConnectionState.CONNECTED -> Color(0xFF4CAF50) to "Connected · ${piStatus.ipAddress}"
        ConnectionState.SEARCHING -> Color(0xFFFF9800) to "Searching..."
        ConnectionState.OFFLINE -> Color(0xFFCF6679) to "Offline"
    }

    Row(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surface)
            .padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Spacer(
            modifier = Modifier
                .size(8.dp)
                .clip(CircleShape)
                .background(dotColor)
        )
        Spacer(Modifier.width(8.dp))
        Text(text = label, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurface)
    }
}
