package ua.visualhoming.app.data.preferences

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "vh_settings")

class AppPreferences(private val context: Context) {
    companion object {
        val PI_URL = stringPreferencesKey("pi_url")
        val LAST_KNOWN_IP = stringPreferencesKey("last_known_ip")
    }

    val piUrl: Flow<String> = context.dataStore.data.map { prefs ->
        prefs[PI_URL] ?: "http://visual-homing.local:5000"
    }

    val lastKnownIp: Flow<String> = context.dataStore.data.map { prefs ->
        prefs[LAST_KNOWN_IP] ?: ""
    }

    suspend fun setPiUrl(url: String) {
        context.dataStore.edit { it[PI_URL] = url }
    }

    suspend fun setLastKnownIp(ip: String) {
        context.dataStore.edit { it[LAST_KNOWN_IP] = ip }
    }
}
