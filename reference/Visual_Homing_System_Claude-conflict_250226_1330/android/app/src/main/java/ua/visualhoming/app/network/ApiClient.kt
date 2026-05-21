package ua.visualhoming.app.network

import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import ua.visualhoming.app.data.api.PiApiService
import ua.visualhoming.app.data.preferences.AppPreferences
import java.util.concurrent.TimeUnit

class ApiClient(private val preferences: AppPreferences) {
    val okHttpClient: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .writeTimeout(10, TimeUnit.SECONDS)
        .addInterceptor(HttpLoggingInterceptor().apply {
            level = HttpLoggingInterceptor.Level.BASIC
        })
        .build()

    private fun buildRetrofit(baseUrl: String): Retrofit = Retrofit.Builder()
        .baseUrl(baseUrl)
        .client(okHttpClient)
        .addConverterFactory(GsonConverterFactory.create())
        .build()

    fun getPiService(): PiApiService {
        val url = runBlocking { preferences.piUrl.first() }
        return buildRetrofit(url).create(PiApiService::class.java)
    }

    fun getWebSocketUrl(): String {
        return runBlocking {
            preferences.piUrl.first().replace("http://", "ws://").replace("https://", "wss://")
        } + "/ws/telemetry"
    }

    fun getVideoStreamUrl(): String {
        return runBlocking { preferences.piUrl.first() } + "/video_feed"
    }
}
