package info.cemu.cemu.settings.userdata

import android.app.Activity
import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.LocalContext
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

    // Launcher: buka Document Provider untuk menentukan lokasi & nama file zip (Export)
    val exportLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                scope.launch {
                    withContext(Dispatchers.IO) {
                        // Sumber: internalFolder() — sama dengan baseDirectory di DocumentsProvider
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
                    withContext(Dispatchers.IO) {
                        // Tujuan: internalFolder() — sama dengan baseDirectory di DocumentsProvider
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
                }
            }
        }
    }

    ScreenContent(
        appBarText = tr("User data"),
        navigateBack = navigateBack,
    ) {
        Button(
            label = tr("Export data"),
            onClick = {
                val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    type = "application/zip"
                    putExtra(Intent.EXTRA_TITLE, "cemu_userdata_backup.zip")
                }
                exportLauncher.launch(intent)
            }
        )
        Button(
            label = tr("Import data"),
            onClick = {
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    type = "application/zip"
                }
                importLauncher.launch(intent)
            }
        )
    }
}
