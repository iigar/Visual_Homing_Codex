package ua.visualhoming.app.data.local

import androidx.room.TypeConverter
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import ua.visualhoming.app.domain.model.RoutePoint

class Converters {
    private val gson = Gson()

    @TypeConverter
    fun fromRoutePointList(points: List<RoutePoint>): String = gson.toJson(points)

    @TypeConverter
    fun toRoutePointList(json: String): List<RoutePoint> {
        val type = object : TypeToken<List<RoutePoint>>() {}.type
        return gson.fromJson(json, type) ?: emptyList()
    }
}
