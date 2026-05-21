package ua.visualhoming.app.data.local

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "routes")
data class RouteEntity(
    @PrimaryKey val id: String,
    val name: String,
    val pointsJson: String,
    val keyframesJson: String,
    val totalDistance: Double,
    val createdAt: String,
    val isSynced: Boolean = false
)
