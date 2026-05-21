package ua.visualhoming.app.data.api

import ua.visualhoming.app.data.api.dto.*
import retrofit2.http.*

interface PiApiService {
    @GET("/api/status")
    suspend fun getStatus(): PiStatusDto

    @GET("/api/routes")
    suspend fun getRoutes(): List<RouteDto>

    @GET("/api/routes/{id}")
    suspend fun getRoute(@Path("id") id: String): RouteDto

    @POST("/api/routes")
    suspend fun createRoute(@Body route: RouteDto): Map<String, Any>

    @DELETE("/api/routes/{id}")
    suspend fun deleteRoute(@Path("id") id: String): Map<String, Any>

    @POST("/api/recording/start")
    suspend fun startRecording(): Map<String, Any>

    @POST("/api/stop")
    suspend fun stop(): Map<String, Any>

    @POST("/api/return/start")
    suspend fun startReturn(): Map<String, Any>

    @POST("/api/return/stop")
    suspend fun stopReturn(): Map<String, Any>

    @GET("/api/settings")
    suspend fun getSettings(): SettingsDto

    @POST("/api/settings")
    suspend fun saveSettings(@Body settings: SettingsDto): Map<String, Any>

    @GET("/api/docs/list")
    suspend fun listDocs(): List<DocFileDto>

    @GET("/api/docs/{filename}")
    suspend fun getDoc(@Path("filename") filename: String): DocContentDto

    @GET("/api/position")
    suspend fun getPosition(): Map<String, Any>
}
