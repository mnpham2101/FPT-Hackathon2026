package com.hackathon.v2x.ivi.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.util.Log
import com.hackathon.v2x.ivi.BuildConfig
import com.hackathon.v2x.ivi.data.R4Deserializer
import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4ServiceError
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.SocketException
import javax.inject.Inject

/**
 * Android ForegroundService that opens a UDP socket on [BuildConfig.R4_UDP_PORT],
 * receives R4 JSON packets from ADA ECU, and emits deserialized [R4Message] events
 * to [r4EventFlow].
 *
 * Key behaviours:
 * - Non-blocking receive loop on [Dispatchers.IO] coroutine
 * - Auto-reconnect on socket error: 1-second back-off, max [MAX_RETRIES] attempts
 * - After [MAX_RETRIES] consecutive failures, sets [serviceError]
 * - Port is driven by [BuildConfig.R4_UDP_PORT] — no hardcoded literals in source
 */
@AndroidEntryPoint
class R4ListenerService : Service() {

    @Inject
    lateinit var deserializer: R4Deserializer

    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var receiveJob: Job? = null

    /** Live UDP socket — closed in [onDestroy] so blocking [DatagramSocket.receive] unblocks. */
    @Volatile
    private var datagramSocket: DatagramSocket? = null

    private val _r4EventFlow = MutableSharedFlow<R4Message>(extraBufferCapacity = 64)
    /** Public API — collect in [R4Repository]. */
    val r4EventFlow: SharedFlow<R4Message> = _r4EventFlow

    // §4.3 fix: transport errors are NOT R4 wire messages. Expose them on a separate
    // channel so callers can distinguish link-state from message-stream events.
    private val _serviceError = MutableStateFlow<R4ServiceError?>(null)
    /** Emits once when the receive loop gives up after [MAX_RETRIES] consecutive failures. */
    val serviceError: StateFlow<R4ServiceError?> = _serviceError

    private val binder = LocalBinder()

    inner class LocalBinder : Binder() {
        fun getService(): R4ListenerService = this@R4ListenerService
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    override fun onCreate() {
        super.onCreate()
        startForegroundWithNotification()
        startReceiveLoop()
    }

    override fun onBind(intent: Intent): IBinder = binder

    override fun onDestroy() {
        receiveJob?.cancel()
        // Unblock Dispatchers.IO receive(); cancel alone cannot interrupt socket.receive.
        runCatching { datagramSocket?.close() }
        datagramSocket = null
        super.onDestroy()
    }

    // ── Receive loop ──────────────────────────────────────────────────────────

    private fun startReceiveLoop() {
        receiveJob = serviceScope.launch(Dispatchers.IO) {
            var retries = 0
            while (isActive) {
                try {
                    openSocketAndReceive()
                    retries = 0
                } catch (e: SocketException) {
                    if (!isActive) return@launch
                    retries++
                    Log.w(TAG, "UDP socket error (attempt $retries/$MAX_RETRIES): ${e.message}")
                    if (retries >= MAX_RETRIES) {
                        Log.e(TAG, "Max retries reached — emitting ServiceError")
                        _serviceError.value = R4ServiceError("Max UDP retries reached")
                        return@launch
                    }
                    delay(RETRY_DELAY_MS)
                } catch (e: Exception) {
                    if (!isActive) return@launch
                    Log.e(TAG, "Unexpected error in receive loop", e)
                    retries++
                    if (retries >= MAX_RETRIES) {
                        _serviceError.value = R4ServiceError("Unexpected error: ${e.message}")
                        return@launch
                    }
                    delay(RETRY_DELAY_MS)
                }
            }
        }
    }

    private suspend fun openSocketAndReceive() {
        val buffer = ByteArray(BUFFER_SIZE)
        val packet = DatagramPacket(buffer, buffer.size)
        DatagramSocket(BuildConfig.R4_UDP_PORT).use { socket ->
            datagramSocket = socket
            Log.i(TAG, "UDP socket open on port ${BuildConfig.R4_UDP_PORT}")
            try {
                while (currentCoroutineContext().isActive) {
                    // Reset length each iteration — without this, one short datagram permanently
                    // shrinks the receive window and every later longer message is silently truncated
                    // (§4.2 fix, subtask 4.5.3.2).
                    packet.setLength(buffer.size)
                    socket.receive(packet)
                    val bytes = packet.data.copyOf(packet.length)
                    deserializer.deserialize(bytes)
                        .onSuccess { message -> _r4EventFlow.emit(message) }
                        .onFailure { e -> Log.w(TAG, "Skipping bad packet: ${e.message}") }
                }
            } finally {
                if (datagramSocket === socket) {
                    datagramSocket = null
                }
            }
        }
    }

    // ── Foreground notification ───────────────────────────────────────────────

    private fun startForegroundWithNotification() {
        val channelId = "r4_listener"
        val channel = NotificationChannel(
            channelId, "R4 Warning Listener", NotificationManager.IMPORTANCE_LOW
        )
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
        val notification = Notification.Builder(this, channelId)
            .setContentTitle("V2X IVI — Listening for R4 warnings")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .build()
        startForeground(NOTIFICATION_ID, notification)
    }

    companion object {
        private const val TAG = "R4ListenerService"
        private const val BUFFER_SIZE = 4096
        private const val MAX_RETRIES = 5
        private const val RETRY_DELAY_MS = 1_000L
        private const val NOTIFICATION_ID = 1001
    }
}

