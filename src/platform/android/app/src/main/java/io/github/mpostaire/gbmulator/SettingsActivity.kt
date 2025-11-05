package io.github.mpostaire.gbmulator

import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults.topAppBarColors
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.mpostaire.gbmulator.ui.theme.GBmulatorTheme
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlin.Float

private val Context.dataStore by preferencesDataStore("settings")

class UserSettings(
    private val context: Context
) {
    val sound = context.dataStore.data.map { preferences ->
        preferences[soundKey] ?: 0.25f
    }

    val speed = context.dataStore.data.map { preferences ->
        preferences[speedKey] ?: 1f
    }

    val frameskip = context.dataStore.data.map { preferences ->
        preferences[frameskipKey] ?: 0f
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

    suspend fun updateSound(value: Float) {
        if (value < 0f || value > 1f)
            return

        context.dataStore.edit { settings ->
            settings[soundKey] = value
        }
    }

    suspend fun updateSpeed(value: Float) {
        if (value < 1 || value > 6)
            return

        context.dataStore.edit { settings ->
            settings[speedKey] = value
        }
    }

    suspend fun updateFrameskip(value: Float) {
        if (value < 0 || value > 4)
            return

        context.dataStore.edit { settings ->
            settings[frameskipKey] = value
        }
    }

    suspend fun updateJoypadOpacity(value: Float) {
        if (value < 0f || value > 1f)
            return

        context.dataStore.edit { settings ->
            settings[joypadOpacityKey] = value
        }
    }

    suspend fun updatePalette(value: Float) {
        if (value < 0f || value > 1f)
            return

        context.dataStore.edit { settings ->
            settings[paletteKey] = value
        }
    }

    suspend fun updateMode(value: Float) {
        if (value < 0f || value > 1f)
            return

        context.dataStore.edit { settings ->
            settings[modeKey] = value
        }
    }

    suspend fun updateCamera(value: Float) {
        if (value < 0f || value > 1f)
            return

        context.dataStore.edit { settings ->
            settings[cameraKey] = value
        }
    }

    companion object {
        val soundKey = floatPreferencesKey("sound")
        val speedKey = floatPreferencesKey("speed")
        val frameskipKey = floatPreferencesKey("frameskip")
        val joypadOpacityKey = floatPreferencesKey("joypad_opacity")
        val paletteKey = floatPreferencesKey("palette")
        val modeKey = floatPreferencesKey("mode")
        val cameraKey = floatPreferencesKey("camera")
    }
}

class SettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            GBmulatorTheme {
                val settings = remember {
                    UserSettings(this)
                }

                SettingsActivityContent(
                    settings = settings
                )
            }
        }
    }
}

@Composable
fun MenuEntryContainer(onClick: () -> Unit = {}, content: @Composable RowScope.() -> Unit) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .height(78.dp),
        onClick = onClick
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically
        ) {
            content()
        }
    }
}

@Composable
fun SettingsMenuEntry(
    text: String,
    items: List<String>,
    valueFlow: Flow<Float>,
    onValueChange: suspend (Float) -> Unit
) {
    val value by valueFlow.collectAsStateWithLifecycle(initialValue = 0f)
    val coroutineScope = rememberCoroutineScope()

    var expanded by remember { mutableStateOf(false) }

    MenuEntryContainer(
        onClick = {
            expanded = !expanded
        },
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = text,
                style = MaterialTheme.typography.titleMedium
            )
            Row {
                Text(
                    text = items[value.toInt()],
                    style = MaterialTheme.typography.bodyLarge
                )
                Icon(
                    painter = painterResource(R.drawable.baseline_arrow_drop_down_24),
                    contentDescription = "$text Icon",
                    tint = MaterialTheme.colorScheme.onSurface
                )
                DropdownMenu(
                    expanded = expanded,
                    onDismissRequest = { expanded = false }
                ) {
                    items.forEachIndexed { i, option ->
                        DropdownMenuItem(
                            text = { Text(
                                text = option,
                                style = MaterialTheme.typography.bodyLarge
                            ) },
                            onClick = {
                                expanded = false
                                coroutineScope.launch {
                                    onValueChange(i.toFloat())
                                }
                            }
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun SettingsSliderEntry(
    valueFlow: Flow<Float>,
    onValueChange: suspend (Float) -> Unit,
    text: String,
    steps: Int = 0,
    valueRange: ClosedFloatingPointRange<Float> = 0f..1f,
    valueLabelFormatter: (Float) -> String = { value -> "$value" },
) {
    val value by valueFlow.collectAsStateWithLifecycle(initialValue = valueRange.start)
    val coroutineScope = rememberCoroutineScope()

    Column(
        modifier = Modifier
            .padding(16.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(32.dp),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(
                text = text,
                style = MaterialTheme.typography.titleMedium
            )
            Text(
                text = valueLabelFormatter(value),
                style = MaterialTheme.typography.bodyMedium
            )
        }
        // Spacer(Modifier.padding(8.dp))
        Slider(
            value = value,
            onValueChange = {
                coroutineScope.launch {
                    onValueChange(it)
                }
            },
            modifier = Modifier.height(20.dp),
            steps = steps,
            valueRange = valueRange
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TopBar() {
    TopAppBar(
        colors = topAppBarColors(
            containerColor = MaterialTheme.colorScheme.primaryContainer,
            titleContentColor = MaterialTheme.colorScheme.primary,
        ),
        title = {
            Text(stringResource(R.string.title_activity_settings))
        }
    )
}

@Preview(showBackground = true)
@Composable
fun SettingsActivityContent(
    settings: UserSettings? = null
) {
    if (settings == null)
        return

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        topBar = {
            TopBar()
        }
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .verticalScroll(rememberScrollState()),
        ) {
            SettingsSliderEntry(
                text = "Volume level",
                valueFlow = settings.sound,
                onValueChange = { settings.updateSound(it) },
                valueLabelFormatter = { value -> "${(value * 100).toInt()}%"}
            )
            HorizontalDivider()
            SettingsSliderEntry(
                text = "Emulation speed",
                valueFlow = settings.speed,
                onValueChange = { settings.updateSpeed(it) },
                steps = 4,
                valueRange = 1.0f..6.0f,
                valueLabelFormatter = { value -> "x${(value).toInt()}"}
            )
            HorizontalDivider()
            SettingsSliderEntry(
                text = "Frame skip",
                valueFlow = settings.frameskip,
                onValueChange = { settings.updateFrameskip(it) },
                steps = 3,
                valueRange = 0.0f..4.0f,
                valueLabelFormatter = { value -> if (value == 0f) "OFF" else "${value.toInt()}" }
            )
            HorizontalDivider()
            SettingsSliderEntry(
                text = "Joypad opacity",
                valueFlow = settings.joypadOpacity,
                onValueChange = { settings.updateJoypadOpacity(it) },
                valueRange = 0.1f..1.0f,
                valueLabelFormatter = { value -> "${(value * 100).toInt()}%"}
            )
            HorizontalDivider()
            SettingsMenuEntry(
                text = "Game Boy Palette",
                items = listOf("Grayscale", "Original"),
                valueFlow = settings.palette,
                onValueChange = { settings.updatePalette(it) }
            )
            HorizontalDivider()
            SettingsMenuEntry(
                text = "Emulation mode",
                items = listOf("Game Boy", "Game Boy Color", "Game Boy Advance"),
                valueFlow = settings.mode,
                onValueChange = { settings.updateMode(it) }
            )
            HorizontalDivider()
            SettingsMenuEntry(
                text = "Select Camera",
                items = listOf("Front", "Back"),
                valueFlow = settings.camera,
                onValueChange = { settings.updateCamera(it) }
            )
        }
    }
}