package io.github.mpostaire.gbmulator.tcp;

import java.net.Socket;

public class TCPConnectionCallback implements Runnable {

    private volatile Socket socket;

    @Override
    public void run() {

    }

    public void setSocket(Socket socket) {
        this.socket = socket;
    }

    public Socket getSocket() {
        return socket;
    }
}
