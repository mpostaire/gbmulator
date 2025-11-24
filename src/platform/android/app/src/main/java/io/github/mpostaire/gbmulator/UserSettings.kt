package io.github.mpostaire.gbmulator

import android.app.Application
import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import io.github.mpostaire.gbmulator.UserSettingsKeys.cameraKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.hostKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.joypadOpacityKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.modeKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.paletteKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.portKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.soundKey
import io.github.mpostaire.gbmulator.UserSettingsKeys.speedKey
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore("settings")

object UserSettings {
    private lateinit var repo: UserSettingsRepository

    fun init(application: Application) {
        repo = UserSettingsRepository(application.applicationContext)
    }

    val sound get() = repo.sound
    val speed get() = repo.speed
    val joypadOpacity get() = repo.joypadOpacity
    val palette get() = repo.palette
    val mode get() = repo.mode
    val camera get() = repo.camera
    val host get() = repo.host
    val port get() = repo.port

    suspend fun setSound(value: Float) = repo.setSound(value)
    suspend fun setSpeed(value: Float) = repo.setSpeed(value)
    suspend fun setJoypadOpacity(value: Float) = repo.setJoypadOpacity(value)
    suspend fun setPalette(value: Float) = repo.setPalette(value)
    suspend fun setMode(value: Float) = repo.setMode(value)
    suspend fun setCamera(value: Float) = repo.setCamera(value)
    suspend fun setHost(value: String) = repo.setHost(value)
    suspend fun setPort(value: String) = repo.setPort(value)
}

object UserSettingsKeys {
    val soundKey = floatPreferencesKey("sound")
    val speedKey = floatPreferencesKey("speed")
    val joypadOpacityKey = floatPreferencesKey("joypad_opacity")
    val paletteKey = floatPreferencesKey("palette")
    val modeKey = floatPreferencesKey("mode")
    val cameraKey = floatPreferencesKey("camera")
    val hostKey = stringPreferencesKey("host")
    val portKey = stringPreferencesKey("port")
}

class UserSettingsRepository(
    private val context: Context
) {
    val sound = context.dataStore.data.map { preferences ->
        preferences[soundKey] ?: 0.25f
    }

    val speed = context.dataStore.data.map { preferences ->
        preferences[speedKey] ?: 1f
    }

    val joypadOpacity = context.dataStore.data.map { preferences ->
        preferences[joypadOpacityKey] ?: 0.8f
    }

    val palette = context.dataStore.data.map { preferences ->
        preferences[paletteKey] ?: 0f
    }

    val mode = context.dataStore.data.map { preferences ->
        preferences[modeKey] ?: 0f
    }

    val camera = context.dataStore.data.map { preferences ->
        preferences[cameraKey] ?: 0f
    }

    val host = context.dataStore.data.map { preferences ->
        preferences[hostKey] ?: "127.0.0.1"
    }

    val port = context.dataStore.data.map { preferences ->
        preferences[portKey] ?: "7777"
    }

    suspend fun setSound(value: Float) {
        if (value !in 0f..1f)
            return

        context.dataStore.edit { settings ->
            settings[soundKey] = value
        }
    }

    suspend fun setSpeed(value: Float) {
        if (value !in 1.0..6.0)
            return

        context.dataStore.edit { settings ->
            settings[speedKey] = value
        }
    }

    suspend fun setJoypadOpacity(value: Float) {
        if (value !in 0f..1f)
            return

        context.dataStore.edit { settings ->
            settings[joypadOpacityKey] = value
        }
    }

    suspend fun setPalette(value: Float) {
        if (value !in 0f..1f)
            return

        context.dataStore.edit { settings ->
            settings[paletteKey] = value
        }
    }

    suspend fun setMode(value: Float) {
        if (value !in 0f..2f)
            return

        context.dataStore.edit { settings ->
            settings[modeKey] = value
        }
    }

    suspend fun setCamera(value: Float) {
        if (value !in 0f..1f)
            return

        context.dataStore.edit { settings ->
            settings[cameraKey] = value
        }
    }

    suspend fun setHost(value: String) {
        context.dataStore.edit { settings ->
            settings[hostKey] = value
        }
    }

    suspend fun setPort(value: String) {
        context.dataStore.edit { settings ->
            settings[portKey] = value
        }
    }
}
