import android.os.ParcelFileDescriptor
import java.io.IOException
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket

object SocketHelper {
    fun startServer(
        port: Int
    ): Int {
        val serverSocket = ServerSocket()
        serverSocket.reuseAddress = true
        serverSocket.bind(InetSocketAddress(port), 1)
        val clientSocket: Socket = serverSocket.accept()
        serverSocket.close()
        return socketToFd(clientSocket)
    }

    fun startClient(
        host: String,
        port: Int,
        connectTimeoutMs: Int = 0
    ): Int {
        val clientSocket = Socket()
        clientSocket.connect(InetSocketAddress(host, port), connectTimeoutMs)
        return socketToFd(clientSocket)
    }

    private fun socketToFd(socket: Socket): Int {
        try {
            val pfd: ParcelFileDescriptor = ParcelFileDescriptor.fromSocket(socket)
            val fd = pfd.detachFd()
            try { pfd.close() } catch (_: Throwable) {}
            return fd
        } catch (e: IOException) {
            throw RuntimeException("Failed to obtain fd from socket", e)
        }
    }

    fun safeCloseFd(fd: Int) {
        try {
            ParcelFileDescriptor.adoptFd(fd).close()
        } catch (_: Throwable) { }
    }
}
