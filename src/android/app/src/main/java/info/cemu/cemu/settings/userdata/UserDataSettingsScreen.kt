package info.cemu.cemu.settings.userdata

import android.app.Activity
import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import info.cemu.cemu.common.android.context.internalFolder
import info.cemu.cemu.common.ui.components.Button
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.localization.tr
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

@Composable
fun UserDataSettingsScreen(navigateBack: () -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var isProcessing by remember { mutableStateOf(false) }
    var processingLabel by remember { mutableStateOf("") }

    // Launcher: buka Document Provider untuk menentukan lokasi & nama file zip (Export)
    val exportLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                scope.launch {
                    processingLabel = tr("Exporting data, please wait...")
                    isProcessing = true
                    withContext(Dispatchers.IO) {
                        val baseDirectory = context.internalFolder()
                        context.contentResolver.openOutputStream(uri)?.use { out ->
                            ZipOutputStream(out).use { zip ->
                                baseDirectory.walkTopDown().forEach { file ->
                                    if (file.isFile) {
                                        val entryName = file.relativeTo(baseDirectory).path
                                        zip.putNextEntry(ZipEntry(entryName))
                                        FileInputStream(file).use { it.copyTo(zip) }
                                        zip.closeEntry()
                                    }
                                }
                            }
                        }
                    }
                    isProcessing = false
                }
            }
        }
    }

    // Launcher: buka Document Provider untuk memilih file zip (Import)
    val importLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                scope.launch {
                    processingLabel = tr("Importing data, please wait...")
                    isProcessing = true
                    withContext(Dispatchers.IO) {
                        val baseDirectory = context.internalFolder()
                        context.contentResolver.openInputStream(uri)?.use { input ->
                            ZipInputStream(input).use { zip ->
                                var entry = zip.nextEntry
                                while (entry != null) {
                                    val outFile = File(baseDirectory, entry.name)
                                    outFile.parentFile?.mkdirs()
                                    outFile.outputStream().use { zip.copyTo(it) }
                                    zip.closeEntry()
                                    entry = zip.nextEntry
                                }
                            }
                        }
                    }
                    isProcessing = false
                }
            }
        }
    }

    // Dialog popup progress saat proses berjalan
    if (isProcessing) {
        Dialog(
            onDismissRequest = {},
            properties = DialogProperties(
                dismissOnBackPress = false,
                dismissOnClickOutside = false,
            )
        ) {
            Surface(
                shape = MaterialTheme.shapes.medium,
                tonalElevation = 6.dp,
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(24.dp),
                ) {
                    Text(
                        text = processingLabel,
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    Spacer(modifier = Modifier.height(16.dp))
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            }
        }
    }

    ScreenContent(
        appBarText = tr("User data"),
        navigateBack = navigateBack,
    ) {
        Text(
            text = tr("Use these options to back up or restore your user data. Export will create a ZIP file containing all your saved data, settings, and configurations. Import will extract a previously exported ZIP file and overwrite the existing data."),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp)
        )
        Button(
            label = tr("Export data"),
            onClick = {
                if (!isProcessing) {
                    val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                        addCategory(Intent.CATEGORY_OPENABLE)
                        type = "application/zip"
                        putExtra(Intent.EXTRA_TITLE, "cemu_userdata_backup.zip")
                    }
                    exportLauncher.launch(intent)
                }
            }
        )
        Button(
            label = tr("Import data"),
            onClick = {
                if (!isProcessing) {
                    val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                        addCategory(Intent.CATEGORY_OPENABLE)
                        type = "application/zip"
                    }
                    importLauncher.launch(intent)
                }
            }
        )
    }
}
