package ua.visualhoming.app.data.repository

import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import ua.visualhoming.app.data.api.dto.DocContentDto
import ua.visualhoming.app.data.api.dto.DocFileDto
import ua.visualhoming.app.data.api.dto.RouteDto
import ua.visualhoming.app.data.api.dto.RoutePointDto
import ua.visualhoming.app.data.local.RouteDao
import ua.visualhoming.app.data.local.RouteEntity
import ua.visualhoming.app.domain.model.Route
import ua.visualhoming.app.domain.model.RoutePoint
import ua.visualhoming.app.network.ApiClient

class RouteRepository(
    private val apiClient: ApiClient,
    private val routeDao: RouteDao
) {
    private val gson = Gson()

    val localRoutes: Flow<List<Route>> = routeDao.getAllRoutes().map { entities ->
        entities.map { it.toDomain() }
    }

    suspend fun fetchRemoteRoutes(): Result<List<RouteDto>> = runCatching {
        apiClient.getPiService().getRoutes()
    }

    suspend fun syncRoute(route: RouteDto) {
        routeDao.insertRoute(route.toEntity())
    }

    suspend fun deleteLocalRoute(id: String) {
        routeDao.deleteRouteById(id)
    }

    suspend fun deleteRemoteRoute(id: String): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().deleteRoute(id)
    }

    suspend fun uploadRoute(route: Route): Result<Map<String, Any>> = runCatching {
        apiClient.getPiService().createRoute(route.toDto())
    }

    suspend fun getDocs(): Result<List<DocFileDto>> = runCatching {
        apiClient.getPiService().listDocs()
    }

    suspend fun getDoc(filename: String): Result<DocContentDto> = runCatching {
        apiClient.getPiService().getDoc(filename)
    }

    private fun RouteEntity.toDomain(): Route {
        val pointType = object : TypeToken<List<RoutePoint>>() {}.type
        return Route(
            id = id, name = name,
            points = gson.fromJson(pointsJson, pointType) ?: emptyList(),
            keyframes = gson.fromJson(keyframesJson, pointType) ?: emptyList(),
            totalDistance = totalDistance,
            createdAt = createdAt,
            isSynced = isSynced
        )
    }

    private fun RouteDto.toEntity() = RouteEntity(
        id = id, name = name,
        pointsJson = gson.toJson(points.map { it.toDomain() }),
        keyframesJson = gson.toJson(keyframes.map { it.toDomain() }),
        totalDistance = totalDistance,
        createdAt = createdAt,
        isSynced = true
    )

    private fun RoutePointDto.toDomain() = RoutePoint(x, y, z, yaw, timestamp, isKeyframe)

    private fun Route.toDto() = RouteDto(
        id = id, name = name,
        points = points.map { RoutePointDto(it.x, it.y, it.z, it.yaw, it.timestamp, it.isKeyframe) },
        keyframes = keyframes.map { RoutePointDto(it.x, it.y, it.z, it.yaw, it.timestamp, it.isKeyframe) },
        totalDistance = totalDistance,
        createdAt = createdAt
    )
}
