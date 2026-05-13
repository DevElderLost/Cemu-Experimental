package info.cemu.cemu.settings

import androidx.compose.runtime.Composable
import androidx.lifecycle.compose.dropUnlessResumed
import info.cemu.cemu.common.ui.components.Button
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.localization.tr

// Data class ini masih bisa dipertahankan jika akan dipakai di tempat lain,
// tapi saat ini lebih bersih menggunakan parameter individual.
data class SettingsHomeScreenActions(
    val goToGeneralSettings: () -> Unit,
    val goToInputSettings: () -> Unit,
    val goToGraphicsSettings: () -> Unit,
    val goToAudioSettings: () -> Unit,
    val goToAccountSettings: () -> Unit,
    val goToOverlaySettings: () -> Unit,
    val goToUserDataSettings: () -> Unit,
)

@Composable
fun SettingsHomeScreen(
    goToGeneralSettings: () -> Unit,
    goToInputSettings: () -> Unit,
    goToGraphicsSettings: () -> Unit,
    goToEmulatedUSBDevicesSettings: () -> Unit,
    goToAudioSettings: () -> Unit,
    goToAccountSettings: () -> Unit,
    goToOverlaySettings: () -> Unit,
    goToUserDataSettings: () -> Unit,   // ← Tambahkan ini
    navigateBack: () -> Unit
) {
    ScreenContent(
        appBarText = tr("Settings"),
        navigateBack = navigateBack,
    ) {
        Button(
            label = tr("General settings"),
            onClick = dropUnlessResumed(block = goToGeneralSettings)
        )
        Button(
            label = tr("Input settings"),
            onClick = dropUnlessResumed(block = goToInputSettings)
        )
        Button(
            label = tr("Graphics settings"),
            onClick = dropUnlessResumed(block = goToGraphicsSettings)
        )
        Button(
            label = tr("Audio settings"),
            onClick = dropUnlessResumed(block = goToAudioSettings)
        )
        Button(
            label = tr("Overlay settings"),
            onClick = dropUnlessResumed(block = goToOverlaySettings)
        )
        Button(
            label = tr("Emulated USB Devices"),
            onClick = dropUnlessResumed(block = goToEmulatedUSBDevicesSettings)
        )
        Button(
            label = tr("Account settings"),
            onClick = dropUnlessResumed(block = goToAccountSettings)
        )
        Button(
            label = tr("User data"),
            onClick = dropUnlessResumed(block = goToUserDataSettings)   // ← Perbaikan di sini
        )
    }
}
