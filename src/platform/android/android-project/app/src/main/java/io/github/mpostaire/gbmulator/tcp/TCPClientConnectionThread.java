package io.github.mpostaire.gbmulator.tcp;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;

public class TCPClientConnectionThread extends Thread {

    TCPConnectionCallback callback;
    public int port;
    public String ipAddress;
    Socket socket;

    public TCPClientConnectionThread(TCPConnectionCallback callback) {
        this.callback = callback;
    }

    public void run() {
        // Connect to the remote device through the socket. This call blocks
        // until it succeeds or throws an exception.
        InetAddress serverAddr = null;
        try {
            serverAddr = InetAddress.getByName(ipAddress);
        } catch (UnknownHostException e) {
            Log.e("GBmulator", "Could not resolve the server address", e);
            return;
        }
        try {
            socket = new Socket(serverAddr, port);
        } catch (IOException e) {
            Log.e("GBmulator", "Could not create the socket", e);
            e.printStackTrace();
            return;
        }

        // The connection attempt succeeded. Perform work associated with
        // the connection in a separate thread.
        callback.setSocket(socket);
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(callback);
    }

    // Closes the client socket and causes the thread to finish.
    public void cancel() {
        try {
            if (socket != null)
                socket.close();
        } catch (IOException e) {
            Log.e("GBmulator", "Could not close the client socket", e);
        }
    }

}
