package com.krisp.krisptestapp

import android.app.ActivityManager
import android.content.Intent
import android.content.res.Configuration
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import android.util.Log
import android.view.View
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.documentfile.provider.DocumentFile

/**
 * What the test pages (file processing, audio capture) share: system bar insets, error dialogs, ✔/✘ status marks,
 * file name/size lookups, the free memory check, and picking + loading the model (one model for all pages, see
 * [KrispSdk]).
 */
abstract class KrispPageActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "KrispPage"

        fun megabytes(bytes: Long) = bytes / (1024.0 * 1024.0)
    }

    /** Called after a model was picked: [loaded] is false when the SDK rejected it (the previous one stays). */
    protected abstract fun onModelPicked(loaded: Boolean)

    /**
     * Android 15+ lays apps targeting SDK 35 out edge-to-edge: the page starts under the status bar and ends under the
     * navigation bar. Pad [root] by their sizes so the content stays visible, and pick system bar icon colors that show
     * on the page background (the bars are transparent there).
     */
    protected fun applySystemBarInsets(root: View) {
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
        if (Build.VERSION.SDK_INT >= 35) {
            val night = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) ==
                Configuration.UI_MODE_NIGHT_YES
            WindowCompat.getInsetsController(window, window.decorView).apply {
                isAppearanceLightStatusBars = !night
                isAppearanceLightNavigationBars = !night
            }
        }
    }

    protected fun showError(title: String, message: String) {
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    protected fun setStatusMark(view: TextView, ok: Boolean) {
        view.text = getString(if (ok) R.string.status_ok else R.string.status_fail)
        view.setTextColor(ContextCompat.getColor(this, if (ok) R.color.status_ok else R.color.status_fail))
    }

    /** The model line: the loaded model's name, or that none is loaded. */
    protected fun modelText(): String =
        KrispSdk.modelName?.let { getString(R.string.txt_model_loaded, it) } ?: getString(R.string.txt_model)

    /** The file name the user sees (e.g. "speech.wav"), not the content:// URI. */
    protected fun displayName(uri: Uri): String {
        val name = try {
            contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0) else null
            }
        } catch (_: Throwable) {
            null
        }
        return name ?: uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':') ?: uri.toString()
    }

    /** The file's size in bytes, or -1 if the provider doesn't tell. */
    protected fun fileSize(uri: Uri): Long {
        val size = try {
            contentResolver.query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { c ->
                if (c.moveToFirst() && !c.isNull(0)) c.getLong(0) else null
            }
        } catch (_: Throwable) {
            null
        }
        if (size != null && size > 0) return size
        return try {
            contentResolver.openFileDescriptor(uri, "r")?.use { it.statSize } ?: -1
        } catch (_: Throwable) {
            -1
        }
    }

    /** Null if [bytes] fit in the memory the system can give the app, else why they don't, ending with [advice]. */
    protected fun memoryShortageFor(bytes: Long, advice: Int = R.string.msg_memory_advice_file): String? {
        val activityManager = getSystemService(ACTIVITY_SERVICE) as ActivityManager
        val memory = ActivityManager.MemoryInfo().also { activityManager.getMemoryInfo(it) }
        // Below the threshold the system starts killing apps, this one included.
        val usable = (memory.availMem - memory.threshold).coerceAtLeast(0)
        return if (bytes > usable) {
            getString(R.string.msg_not_enough_memory, megabytes(bytes), megabytes(usable), getString(advice))
        } else {
            null
        }
    }

    // --- Model picking: the Files picker, or (long press) a folder to choose a .kef from.

    protected fun openModelPicker() {
        pickModelDoc.launch(arrayOf("application/*", "application/octet-stream", "*/*"))
    }

    protected fun openModelFolderPicker() {
        // Launch tree picker; user can pick "Downloads" manually if not pre-seeded
        pickFolder.launch(null)
    }

    private val pickModelDoc = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        uri ?: return@registerForActivityResult
        try {
            // Persist only if supported (OpenDocument supports; GetContent does not)
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (_: Throwable) { /* ignore */ }
        loadModel(uri)
    }

    private val pickFolder = registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { treeUri: Uri? ->
        treeUri ?: return@registerForActivityResult
        try {
            contentResolver.takePersistableUriPermission(treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (_: Throwable) {}
        val dir = DocumentFile.fromTreeUri(this, treeUri) ?: return@registerForActivityResult
        showKefChooser(dir)
    }

    private fun showKefChooser(dir: DocumentFile) {
        val files = dir.listFiles().filter { it.isFile && (it.name?.endsWith(".kef", true) == true) }
        if (files.isEmpty()) {
            AlertDialog.Builder(this)
                .setTitle("No .kef files found")
                .setMessage("Pick a different folder or copy your model into this folder.")
                .setPositiveButton(android.R.string.ok, null)
                .show()
            return
        }
        val names = files.map { it.name ?: "(unnamed)" }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Select model")
            .setItems(names) { _, which -> loadModel(files[which].uri) }
            .show()
    }

    private fun loadModel(uri: Uri) {
        val name = displayName(uri)
        try {
            val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() }
            when {
                bytes == null -> showError(getString(R.string.title_file_import_error), "Unable to access the selected file.")
                bytes.isEmpty() -> showError(getString(R.string.title_file_import_error), "File is empty")
                KrispSdk.loadModel(bytes, name) -> {
                    onModelPicked(true)
                    return
                }
                else -> showError(
                    getString(R.string.title_file_import_error),
                    KrispSdk.withSdkError(getString(R.string.msg_model_load_failed, name))
                )
            }
        } catch (t: Throwable) {
            // Reading the file failed before it reached the SDK: the loaded model (if any) is still in use.
            Log.e(TAG, "Loading the model failed for $uri", t)
            showError(getString(R.string.title_file_import_error), "Model open failed: ${t.javaClass.simpleName}: ${t.message}")
        }
        onModelPicked(false)
    }
}
