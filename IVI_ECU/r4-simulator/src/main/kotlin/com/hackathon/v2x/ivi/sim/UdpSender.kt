package com.hackathon.v2x.ivi.sim

import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress

/**
 * Fire-and-forget datagram sender — one send per step. A send error is logged
 * and counted, never fatal: the simulator is test equipment and must stay
 * alive for the whole scenario, exactly as the bench's sender does.
 */
class UdpSender(private val host: String, private val port: Int) : AutoCloseable {

    private val socket = DatagramSocket()
    private val address: InetAddress = InetAddress.getByName(host)

    var errorCount: Int = 0
        private set

    fun send(bytes: ByteArray): Boolean = try {
        socket.send(DatagramPacket(bytes, bytes.size, address, port))
        true
    } catch (e: Exception) {
        errorCount++
        System.err.println("[SND-ERR] send to $host:$port failed: ${e.message}")
        false
    }

    override fun close() = socket.close()
}
