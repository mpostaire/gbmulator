package io.github.mpostaire.gbmulator;

import android.app.Application;
import android.bluetooth.BluetoothSocket;

import java.net.Socket;
import java.util.UUID;

public class GBmulatorApp extends Application {

    public static final String BLUETOOTH_SERVICE_NAME = "GBmulator";
    public static final UUID BLUETOOTH_SERVICE_UUID = UUID.fromString("c424dd68-a613-42d5-99bf-d82848e929d2");

    public BluetoothSocket bluetoothSocket;
    public int tcpSocket;

}
