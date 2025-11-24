package io.github.mpostaire.gbmulator

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults.topAppBarColors
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import io.github.mpostaire.gbmulator.ui.components.MainMenuEntry
import io.github.mpostaire.gbmulator.ui.components.RestartDialog
import io.github.mpostaire.gbmulator.ui.theme.GBmulatorTheme

class MainActivity : ComponentActivity() {
    private var romUri: Uri? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            GBmulatorTheme {
                MainActivityContent()
            }
        }
    }

    fun resume(reset: Boolean) {
        val intent = Intent(this@MainActivity, EmulatorActivity::class.java)
        intent.putExtra("rom_uri", romUri)
        intent.putExtra("reset", reset)
        startActivity(intent)
    }

    fun openSettings() {
        val intent = Intent(this@MainActivity, SettingsActivity::class.java)
        startActivity(intent)
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
                Text(stringResource(R.string.app_name))
            }
        )
    }

    @Preview(showBackground = true)
    @Composable
    fun MainActivityContent() {
        var showRestartDialog by remember {
            mutableStateOf(false)
        }
        var romUriState by rememberSaveable { mutableStateOf(romUri) }

        val launcher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.StartActivityForResult()
        ) { result ->
            if (result.resultCode != RESULT_OK || result.data == null)
                return@rememberLauncherForActivityResult

            romUri = result.data?.data
            romUriState = romUri
            resume(true)
        }

        if (showRestartDialog) {
            RestartDialog(
                onDismiss = {
                    showRestartDialog = false
                },
                onConfirm = {
                    showRestartDialog = false
                    resume(true)
                }
            )
        }

        Scaffold(
            modifier = Modifier.fillMaxSize(),
            topBar = {
                TopBar()
            }
        ) { innerPadding ->
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding),
                verticalArrangement = Arrangement.SpaceAround,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    text = romUriState?.path ?: "Empty cartridge slot",
                    style = MaterialTheme.typography.headlineMedium,
                    color = MaterialTheme.colorScheme.onSurface
                )
                Column(
                    modifier = Modifier
                        .verticalScroll(rememberScrollState())
                ) {
                    MainMenuEntry(
                        text = "RESUME",
                        icon = R.drawable.baseline_play_arrow_24,
                        enabled = romUriState != null,
                        onClick = { resume(false) }
                    )
                    HorizontalDivider()
                    MainMenuEntry(
                        text = "LOAD CARTRIDGE",
                        icon = R.drawable.baseline_file_open_24,
                        enabled = true,
                        onClick = {
                            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                                addCategory(Intent.CATEGORY_OPENABLE)
                                type = "*/*"
                                setAction(Intent.ACTION_OPEN_DOCUMENT)
                            }
                            launcher.launch(intent)
                        }
                    )
                    HorizontalDivider()
                    MainMenuEntry(
                        text = "RESTART EMULATOR",
                        icon = R.drawable.baseline_refresh_24,
                        enabled = romUri != null,
                        onClick = {
                            showRestartDialog = true
                        }
                    )
                    HorizontalDivider()
                    MainMenuEntry(
                        text = "SETTINGS",
                        icon = R.drawable.baseline_settings_24,
                        enabled = true,
                        onClick = ::openSettings
                    )
                }
            }
        }
    }
}
