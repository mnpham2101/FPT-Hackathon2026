package com.hackathon.v2x.ivi.net

import com.hackathon.v2x.ivi.model.R4WarningMessage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress

class R4UdpListener(
    private val host: String = "0.0.0.0",
    private val port: Int = 46004,
    private val bufferSizeBytes: Int = 8192,
) {
    suspend fun listen(onWarning: (R4WarningMessage) -> Unit) = withContext(Dispatchers.IO) {
        DatagramSocket(port, InetAddress.getByName(host)).use { socket ->
            while (isActive) {
                val buffer = ByteArray(bufferSizeBytes)
                val packet = DatagramPacket(buffer, buffer.size)
                socket.receive(packet)

                val payload = String(packet.data, packet.offset, packet.length, Charsets.UTF_8)
                val warning = R4WarningMessage.decode(payload)
                if (warning.type == "warning") {
                    onWarning(warning)
                }
            }
        }
    }
}
