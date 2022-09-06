package io.github.mpostaire.gbmulator.bluetooth;

import android.bluetooth.BluetoothSocket;

public class BluetoothConnectionCallback implements Runnable {

    private volatile BluetoothSocket socket;

    @Override
    public void run() {

    }

    public void setSocket(BluetoothSocket socket) {
        this.socket = socket;
    }

    public BluetoothSocket getSocket() {
        return socket;
    }
}
