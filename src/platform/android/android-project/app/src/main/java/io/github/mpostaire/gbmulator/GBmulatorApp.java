package io.github.mpostaire.gbmulator;

import android.app.Application;
import android.bluetooth.BluetoothSocket;

public class GBmulatorApp extends Application {

    public BluetoothSocket socket;
    public boolean isLinkConnected = false;

}
