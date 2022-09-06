package io.github.mpostaire.gbmulator.bluetooth;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;

public class BluetoothServerConnectionThread extends Thread {
    private final BluetoothServerSocket serverSocket;
    BluetoothConnectionCallback callback;

    @SuppressLint("MissingPermission")
    public BluetoothServerConnectionThread(BluetoothAdapter bluetoothAdapter, BluetoothConnectionCallback callback) {
        // Use a temporary object that is later assigned to serverSocket
        // because serverSocket is final.
        BluetoothServerSocket tmp = null;
        this.callback = callback;
        try {
            // MY_UUID is the app's UUID string, also used by the client code.
            tmp = bluetoothAdapter.listenUsingRfcommWithServiceRecord(BluetoothConstants.SERVICE_NAME, BluetoothConstants.SERVICE_UUID);
        } catch (IOException e) {
            Log.e(BluetoothConstants.SERVICE_NAME, "Socket's listen() method failed", e);
        }
        serverSocket = tmp;
    }

    public void run() {
        BluetoothSocket socket = null;
        // Keep listening until exception occurs or a socket is returned.
        while (true) {
            try {
                socket = serverSocket.accept();
            } catch (IOException e) {
                Log.e(BluetoothConstants.SERVICE_NAME, "Socket's accept() method failed", e);
                break;
            }

            if (socket != null) {
                // A connection was accepted. Perform work associated with
                // the connection in a separate thread.
                callback.setSocket(socket);
                Handler handler = new Handler(Looper.getMainLooper());
                handler.post(callback);
                try {
                    serverSocket.close();
                } catch (IOException e) {
                    Log.e(BluetoothConstants.SERVICE_NAME, "Socket's close() method failed", e);
                }
                break;
            }
        }
    }

    // Closes the connect socket and causes the thread to finish.
    public void cancel() {
        try {
            serverSocket.close();
        } catch (IOException e) {
            Log.e(BluetoothConstants.SERVICE_NAME, "Could not close the connect socket", e);
        }
    }
}