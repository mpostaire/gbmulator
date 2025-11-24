package io.github.mpostaire.gbmulator

import SocketHelper.safeCloseFd
import SocketHelper.startClient
import SocketHelper.startServer
import android.Manifest
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.util.Size
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.resolutionselector.AspectRatioStrategy
import androidx.camera.core.resolutionselector.ResolutionSelector
import androidx.camera.core.resolutionselector.ResolutionStrategy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.google.androidgamesdk.GameActivity
import io.github.mpostaire.gbmulator.ui.components.LinkDialog
import io.github.mpostaire.gbmulator.ui.theme.GBmulatorTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import java.util.concurrent.CompletableFuture
import java.util.concurrent.ExecutionException
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import kotlin.coroutines.cancellation.CancellationException


data class EmuParams(
    val romFd: Int,
    val reset: Boolean,
    val refreshRate: Float,
    val mode: Int,
    val sound: Float,
    val speed: Float,
    val joypadOpacity: Float,
    val palette: Int
)

class EmulatorActivity : GameActivity() {
    companion object {
        init {
            System.loadLibrary("gbmulator")
        }
    }

    private var composeOverlay: ComposeView? = null

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUi()
        }
    }

    private fun hideSystemUi() {
        val decorView = window.decorView
        decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN)
    }

    fun getEmuParams(): EmuParams? {
        val reset = intent.getBooleanExtra("reset", false)

        val romUri: Uri? = intent.getParcelableExtra("rom_uri")
        if (romUri == null) {
            errorToast("Oops... Something bad happened!")
            finish()
            return null
        }

        val pfd = contentResolver.openFileDescriptor(romUri, "r")
        val fd = pfd?.detachFd() ?: -1

        return runBlocking {
            EmuParams(
                fd,
                reset,
                display.refreshRate,
                UserSettings.mode.first().toInt(),
                UserSettings.sound.first(),
                UserSettings.speed.first(),
                UserSettings.joypadOpacity.first(),
                UserSettings.palette.first().toInt()
            )
        }
    }

    fun errorToast(msg: String?) {
        runOnUiThread {
            Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
        }
    }

    fun showComposeOverlayDialog(
        onConfirm: (String?, Int) -> Unit,
        onCancel: () -> Unit
    ) {
        if (composeOverlay != null) return

        composeOverlay = ComposeView(this).apply {
            setContent {
                GBmulatorTheme {
                    // Full-screen overlay
                    Box(
                        modifier = Modifier
                            .fillMaxSize()
                            .background(Color.Black.copy(alpha = 0.35f)),
                        contentAlignment = Alignment.Center
                    ) {
                        LinkDialog(
                            onConfirm = { host, port ->
                                onConfirm(host, port)
                            },
                            onCancel = { onCancel() }
                        )
                    }
                }
            }
        }

        addContentView(
            composeOverlay,
            ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )
    }

    private var connectionJob: Job? = null
    private var openSocketFd: Int? = null

    fun showSocketDialog(): Int {
        val future = CompletableFuture<Int>()
        var isCancelled = false

        runOnUiThread {
            showComposeOverlayDialog(
                onConfirm = { host, port ->
                    // Cancel previous if any
                    connectionJob?.cancel()

                    connectionJob = lifecycleScope.launch(Dispatchers.IO) {
                        try {
                            val fd = if (host == null)
                                startServer(port)
                            else
                                startClient(host, port)

                            openSocketFd = fd
                            future.complete(fd)
                        } catch (e: CancellationException) {
                            // Close socket if we opened it
                            openSocketFd?.let { safeCloseFd(it) }
                            openSocketFd = null

                            // Cancelled by user
                            future.complete(-1)
                        } catch (e: Exception) {
                            future.complete(-1)
                        }
                    }
                },

                onCancel = {
                    isCancelled = true

                    // Cancel the running job
                    connectionJob?.cancel()

                    // Close socket if already opened
                    openSocketFd?.let { safeCloseFd(it) }
                    openSocketFd = null

                    // Complete the future
                    future.complete(-1)
                }
            )
        }

        val sfd = future.get() // BLOCKS but not UI

        // remove overlay (therefore dialog)
        runOnUiThread {
            composeOverlay?.let { view ->
                (view.parent as? ViewGroup)?.removeView(view)
            }
            composeOverlay = null
        }

        if (isCancelled)
            errorToast("Connection cancelled!")
        else if (sfd < 0)
            errorToast("Connection failed!")
        else
            errorToast("Connected!")

        return sfd
    }

    var imgAnalyzer: ImageAnalysis? = null
    private val executor: Executor = Executors.newSingleThreadExecutor()
    var cameraGrayscaleData: ByteArray? = null
    external fun updateCameraBuffer(
        data: ByteArray?,
        width: Int,
        height: Int,
        row_stride: Int,
        rotation: Int
    )

    fun initImageAnalysis() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)

        cameraProviderFuture.addListener({
            val cameraProvider = cameraProviderFuture.get()

            val resolutionSelector = ResolutionSelector.Builder()
                .setAllowedResolutionMode(ResolutionSelector.PREFER_CAPTURE_RATE_OVER_HIGHER_RESOLUTION)
                .setAspectRatioStrategy(AspectRatioStrategy.RATIO_4_3_FALLBACK_AUTO_STRATEGY)
                .setResolutionStrategy(
                    ResolutionStrategy(
                        Size(128, 128),
                        ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER
                    )
                )
                .build()

            imgAnalyzer = ImageAnalysis.Builder()
                .setResolutionSelector(resolutionSelector)
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setTargetRotation(display.rotation)
                .build()

            imgAnalyzer!!.setAnalyzer(executor) { image ->
                val y = image.planes[0]
                val buffer = y.buffer

                // Allocate only once
                if (cameraGrayscaleData == null ||
                    cameraGrayscaleData!!.size != image.width * image.height
                ) {
                    cameraGrayscaleData = ByteArray(image.width * image.height)
                }

                buffer.get(cameraGrayscaleData!!)
                buffer.rewind()

                updateCameraBuffer(
                    cameraGrayscaleData,
                    image.width,
                    image.height,
                    y.rowStride,
                    image.imageInfo.rotationDegrees
                )
                image.close()
            }

            val cameraSelector = runBlocking {
                if (UserSettings.camera.first().toInt() == 0)
                    CameraSelector.DEFAULT_BACK_CAMERA
                else
                    CameraSelector.DEFAULT_FRONT_CAMERA
            }

            cameraProvider.unbindAll()
            cameraProvider.bindToLifecycle(this@EmulatorActivity, cameraSelector, imgAnalyzer)
        }, ContextCompat.getMainExecutor(this))
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 42024 && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) initImageAnalysis()
    }

    fun initCamera() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED)
            initImageAnalysis()
        else
            requestPermissions(arrayOf<String?>(Manifest.permission.CAMERA), 42024)
    }
}
