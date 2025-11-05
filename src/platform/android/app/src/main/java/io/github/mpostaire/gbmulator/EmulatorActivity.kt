package io.github.mpostaire.gbmulator

import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.Toast
import com.google.androidgamesdk.GameActivity


class EmulatorActivity : GameActivity() {
    external fun loadRom(fd: Int)

    companion object {
        init {
            System.loadLibrary("gbmulator")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val romUri: Uri? = intent.getParcelableExtra("rom_uri")
        if (romUri == null) {
            errorToast("Oops... Something bad happened!")
            finish()
            return
        }

        errorToast("Launching: " + romUri.path)

        val pfd = contentResolver.openFileDescriptor(romUri, "r")
        val fd = pfd?.detachFd() ?: -1

        loadRom(fd)
    }

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

    fun errorToast(msg: String?) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
    }
}