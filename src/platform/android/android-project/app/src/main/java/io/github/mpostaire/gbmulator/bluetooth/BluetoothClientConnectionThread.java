package io.github.mpostaire.gbmulator.bluetooth;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;

public class BluetoothClientConnectionThread extends Thread {
    final BluetoothSocket serverSocket;
    private final BluetoothDevice serverDevice;
    private final BluetoothAdapter bluetoothAdapter;
    BluetoothConnectionCallback callback;

    @SuppressLint("MissingPermission")
    public BluetoothClientConnectionThread(BluetoothAdapter bluetoothAdapter, BluetoothDevice serverDevice, BluetoothConnectionCallback callback) {
        // Use a temporary object that is later assigned to serverSocket
        // because serverSocket is final.
        BluetoothSocket tmp = null;
        this.serverDevice = serverDevice;
        this.bluetoothAdapter = bluetoothAdapter;

        try {
            // Get a BluetoothSocket to connect with the given BluetoothDevice.
            // MY_UUID is the app's UUID string, also used in the server code.
            tmp = serverDevice.createRfcommSocketToServiceRecord(BluetoothConstants.SERVICE_UUID);
        } catch (IOException e) {
            Log.e(BluetoothConstants.SERVICE_NAME, "Socket's create() method failed", e);
        }
        this.callback = callback;
        serverSocket = tmp;
    }

    @SuppressLint("MissingPermission")
    public void run() {
        // Cancel discovery because it otherwise slows down the connection.
        bluetoothAdapter.cancelDiscovery();

        try {
            // Connect to the remote device through the socket. This call blocks
            // until it succeeds or throws an exception.
            serverSocket.connect();
        } catch (IOException connectException) {
            // Unable to connect; close the socket and return.
            try {
                serverSocket.close();
            } catch (IOException closeException) {
                Log.e(BluetoothConstants.SERVICE_NAME, "Could not close the client socket", closeException);
            }
            return;
        }

        // The connection attempt succeeded. Perform work associated with
        // the connection in a separate thread.
        callback.setSocket(serverSocket);
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(callback);
    }

    // Closes the client socket and causes the thread to finish.
    public void cancel() {
        try {
            serverSocket.close();
        } catch (IOException e) {
            Log.e(BluetoothConstants.SERVICE_NAME, "Could not close the client socket", e);
        }
    }
}