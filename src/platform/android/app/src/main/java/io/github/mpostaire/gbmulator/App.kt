package io.github.mpostaire.gbmulator

import android.app.Application

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        UserSettings.init(this)
    }
}
