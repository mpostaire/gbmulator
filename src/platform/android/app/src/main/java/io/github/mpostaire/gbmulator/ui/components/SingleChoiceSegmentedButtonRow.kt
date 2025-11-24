package io.github.mpostaire.gbmulator.ui.components

import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable

@Composable
fun SingleChoiceSegmentedButton(
    items: List<String>,
    selected: Int = 0,
    enabled: Boolean = true,
    onClick: (Int) -> Unit
) {
    SingleChoiceSegmentedButtonRow {
        items.forEachIndexed { index, label ->
            SegmentedButton(
                shape = SegmentedButtonDefaults.itemShape(
                    index = index,
                    count = items.size
                ),
                enabled = enabled,
                onClick = { onClick(index) },
                selected = index == selected,
                label = { Text(label) }
            )
        }
    }
}
