package io.github.mpostaire.gbmulator.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp

// TODO maybe change connect protocol to local broadcast of tcp port or just hardcoded port
//      if so, then this dialog and connection process can be simplified a lot for the user
//      (one click on a button in both devices to connect, no other user input needed --> what about GBA 4 player link?)

@Composable
@Preview
fun LinkDialog(
    onConfirm: (String?, Int) -> Unit = {a, b -> },
    onCancel: () -> Unit = {}
) {
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("") }
    var isConnecting by remember { mutableStateOf(false) }
    var selectedMode by remember { mutableIntStateOf(0) }

    AlertDialog(
        title = {
            Text(text = "WiFi Link")
        },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                SingleChoiceSegmentedButton(
                    items = listOf("Client", "Server"),
                    selected = selectedMode,
                    enabled = !isConnecting,
                    onClick = { selectedMode = it }
                )

                AnimatedVisibility(
                    visible = selectedMode == 0
                ) {
                    OutlinedTextField(
                        modifier = Modifier.fillMaxWidth(),
                        value = host,
                        enabled = !isConnecting,
                        onValueChange = { host = it },
                        label = { Text("Host") },
                        placeholder = { Text("127.0.0.1") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri)
                    )
                }

                OutlinedTextField(
                    modifier = Modifier.fillMaxWidth(),
                    value = port,
                    enabled = !isConnecting,
                    onValueChange = { port = it },
                    label = { Text("Port") },
                    placeholder = { Text("7777") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)
                )

                AnimatedVisibility(
                    visible = isConnecting
                ) {
                    Column(
                        verticalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Text(if (selectedMode == 0) "Connecting..." else "Waiting for connection...")
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    }
                }
            }

        },
        onDismissRequest = {
            onCancel()
        },
        confirmButton = {
            TextButton(
                enabled = !isConnecting,
                onClick = {
                    isConnecting = true
                }
            ) {
                Text("OK")
            }

            //if (!isConnecting)
            //    onConfirm(host, 7777)
        },
        dismissButton = {
            TextButton(
                onClick = {
                    onCancel()
                }
            ) {
                Text("Cancel")
            }
        }
    )
}

@Composable
@Preview
fun LinkDialogMaybeBetter(
    onCancel: () -> Unit = {}
) {
    val statusLabel = "Connecting..."

    AlertDialog(
        title = {
            Text(text = "WiFi Link")
        },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                CircularProgressIndicator()
                Text(statusLabel)
            }

        },
        onDismissRequest = onCancel,
        confirmButton = {},
        dismissButton = {
            TextButton(
                onClick = onCancel
            ) {
                Text("Cancel")
            }
        }
    )
}
