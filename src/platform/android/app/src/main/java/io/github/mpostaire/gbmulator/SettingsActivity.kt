package io.github.mpostaire.gbmulator

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.input.rememberTextFieldState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.mpostaire.gbmulator.ui.theme.GBmulatorTheme
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.launch

class SettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            GBmulatorTheme {
                SettingsActivityContent()
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

@Composable
fun TextInputDialog(
    label: String,
    value: String,
    onConfirm: (String) -> Unit,
    onDismiss: () -> Unit
) {
    val text = rememberTextFieldState(value)

    AlertDialog(
        title = {
            Text(text = label)
        },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                OutlinedTextField(state = text)
            }
        },
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(
                onClick = { onConfirm(text.text.toString()) }
            ) {
                Text("OK")
            }
        },
        dismissButton = {
            TextButton(
                onClick = onDismiss
            ) {
                Text("Cancel")
            }
        }
    )
}

@Composable
fun SettingsTextEntry(
    label: String,
    valueFlow: Flow<String>,
    onValueChange: suspend (String) -> Unit
) {
    val value by valueFlow.collectAsStateWithLifecycle(initialValue = "")
    val coroutineScope = rememberCoroutineScope()

    var showDialog by remember { mutableStateOf(false) }

    if (showDialog) {
        TextInputDialog(
            label = label,
            value = value,
            onConfirm = {
                showDialog = false
                coroutineScope.launch { onValueChange(it) }
            },
            onDismiss = { showDialog = false }
        )
    }

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .height(78.dp),
        onClick = {
            showDialog = true
        }
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = label,
                style = MaterialTheme.typography.titleMedium
            )
            Text(
                text = value,
                style = MaterialTheme.typography.bodyMedium
            )
        }
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
fun SettingsActivityContent() {
    val context = LocalContext.current
    val versionText =
        "Version ${BuildConfig.VERSION_NAME}" + if (BuildConfig.DEBUG) " (debug)" else ""

    var toast by remember { mutableStateOf<Toast>(Toast.makeText(context, "Copied to clipboard", Toast.LENGTH_SHORT)) }

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
                valueFlow = UserSettings.sound,
                onValueChange = { UserSettings.setSound(it) },
                valueLabelFormatter = { "${(it * 100).toInt()}%"}
            )
            HorizontalDivider()
            SettingsSliderEntry(
                text = "Emulation speed",
                valueFlow = UserSettings.speed,
                onValueChange = { UserSettings.setSpeed(it) },
                steps = 4,
                valueRange = 1.0f..6.0f,
                valueLabelFormatter = { "x${(it).toInt()}"}
            )
            HorizontalDivider()
            SettingsSliderEntry(
                text = "Joypad opacity",
                valueFlow = UserSettings.joypadOpacity,
                onValueChange = { UserSettings.setJoypadOpacity(it) },
                valueRange = 0.1f..1.0f,
                valueLabelFormatter = { "${(it * 100).toInt()}%"}
            )
            HorizontalDivider()
            SettingsMenuEntry(
                text = "Game Boy Palette",
                items = listOf("Grayscale", "Original"),
                valueFlow = UserSettings.palette,
                onValueChange = { UserSettings.setPalette(it) }
            )
            HorizontalDivider()
            SettingsMenuEntry(
                text = "Emulation mode",
                items = listOf("Game Boy", "Game Boy Color", "Game Boy Advance"),
                valueFlow = UserSettings.mode,
                onValueChange = { UserSettings.setMode(it) }
            )
            HorizontalDivider()
            SettingsMenuEntry(
                text = "Select Camera",
                items = listOf("Back", "Front"),
                valueFlow = UserSettings.camera,
                onValueChange = { UserSettings.setCamera(it) }
            )
            HorizontalDivider()
            SettingsTextEntry(
                label = "WiFi Link Host",
                valueFlow = UserSettings.host,
                onValueChange = { UserSettings.setHost(it) }
            )
            HorizontalDivider()
            SettingsTextEntry(
                label = "WiFi Link Port",
                valueFlow = UserSettings.port,
                onValueChange = { UserSettings.setPort(it) }
            )
            HorizontalDivider()
            MenuEntryContainer(
                onClick = {
                    val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    val clip = ClipData.newPlainText("version", versionText)
                    clipboard.setPrimaryClip(clip)

                    toast.show()
                },
                content = {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = versionText,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            textAlign = TextAlign.Center
                        )
                    }
                }
            )
        }
    }
}