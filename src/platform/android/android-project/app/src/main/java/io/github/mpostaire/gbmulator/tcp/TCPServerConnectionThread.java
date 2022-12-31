package io.github.mpostaire.gbmulator.tcp;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

public class TCPServerConnectionThread extends Thread {

    public int port;
    final TCPConnectionCallback callback;
    ServerSocket serverSocket;
    volatile boolean isRunning = true;

    public TCPServerConnectionThread(TCPConnectionCallback callback) {
        this.callback = callback;
    }

    public void run() {
        try {
            serverSocket = new ServerSocket(port);
        } catch (IOException e) {
            e.printStackTrace();
        }

        // Keep listening until exception occurs or a socket is returned.
        while (isRunning) {
            Socket socket = null;
            try {
                socket = serverSocket.accept();
            } catch (IOException e) {
                Log.e("GBmulator", "Socket's accept() method failed", e);
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
                    Log.e("GBmulator", "Socket's close() method failed", e);
                }
                break;
            }
        }

    }

    public void cancel() {
        try {
            isRunning = false;
            if (serverSocket != null)
                serverSocket.close();
        } catch (IOException e) {
            Log.e("GBmulator", "Could not close the connect socket", e);
        }
    }

}
