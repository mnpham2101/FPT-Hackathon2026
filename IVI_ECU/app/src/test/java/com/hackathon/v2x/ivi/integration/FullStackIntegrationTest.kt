package com.hackathon.v2x.ivi.integration

import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import com.hackathon.v2x.ivi.BuildConfig
import com.hackathon.v2x.ivi.data.R4Repository
import com.hackathon.v2x.ivi.service.R4ListenerService
import com.hackathon.v2x.ivi.ui.DisplayMode
import com.hackathon.v2x.ivi.ui.MainViewModel
import com.hackathon.v2x.ivi.ui.WarningUiState
import com.hackathon.v2x.ivi.ui.WarningViewModel
import com.hackathon.v2x.ivi.ui.view.Canvas3DWarningView
import com.hackathon.v2x.ivi.ui.view.CanvasWarningView
import com.hackathon.v2x.ivi.ui.view.IviWarningViewSeam
import dagger.hilt.android.testing.HiltAndroidRule
import dagger.hilt.android.testing.HiltAndroidTest
import dagger.hilt.android.testing.HiltTestApplication
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import kotlinx.coroutines.withTimeout
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.android.controller.ServiceController
import org.robolectric.annotation.Config

/**
 * Full-stack integration diagnostics for 16.5.4.1.
 *
 * Exercises the live path:
 * mock UDP → [R4ListenerService] → [R4Repository] → [WarningViewModel] → [MainViewModel]
 * and asserts wake-on-warning switches the Display Area to [DisplayMode.WarningView].
 */
@OptIn(ExperimentalCoroutinesApi::class)
@HiltAndroidTest
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], application = HiltTestApplication::class)
class FullStackIntegrationTest {

    @get:Rule
    val hiltRule = HiltAndroidRule(this)

    @Inject
    lateinit var repository: R4Repository

    @Inject
    lateinit var warningViewSeam: IviWarningViewSeam

    private lateinit var serviceController: ServiceController<R4ListenerService>
    private lateinit var service: R4ListenerService

    @Before
    fun setUp() {
        hiltRule.inject()
        Dispatchers.setMain(Dispatchers.Unconfined)

        val context = ApplicationProvider.getApplicationContext<android.content.Context>()
        val intent = Intent(context, R4ListenerService::class.java)
        serviceController = Robolectric.buildService(R4ListenerService::class.java, intent)
        service = serviceController.create().get()
    }

    @After
    fun tearDown() {
        runCatching { serviceController.destroy() }
        Dispatchers.resetMain()
    }

    @Test
    fun appModule_bindsSingletonCanvas3DWarningViewSeam() {
        assertTrue(
            "IviWarningViewSeam must resolve to Canvas3DWarningView",
            warningViewSeam is Canvas3DWarningView,
        )
    }

    @Test
    fun udpR4Warning_propagatesThroughServiceToMainViewModelWarningView() = runBlocking {
        val warningViewModel = WarningViewModel(repository)
        val mainViewModel = MainViewModel(warningViewModel.uiWarningState)

        assertEquals(DisplayMode.HomeView, mainViewModel.currentMode.value)
        assertTrue(warningViewModel.uiWarningState.value is WarningUiState.Idle)

        // Attach before sending — SharedFlow has replay=0.
        warningViewModel.attachService(service.r4EventFlow)

        // Allow the IO receive loop to bind BuildConfig.R4_UDP_PORT.
        awaitUdpListenerReady()

        val payload = loadSampleWarningJson()
        sendUdpWarning(payload)

        withTimeout(UDP_PROPAGATION_TIMEOUT_MS) {
            while (
                warningViewModel.uiWarningState.value !is WarningUiState.Active ||
                mainViewModel.currentMode.value != DisplayMode.WarningView
            ) {
                delay(POLL_INTERVAL_MS)
            }
        }

        val active = warningViewModel.uiWarningState.value as WarningUiState.Active
        assertEquals("high", active.event.riskState)
        assertEquals("nlos_obstruction", active.event.warningType)
        assertNotNull(warningViewModel.latestScene.value)
        assertEquals(DisplayMode.WarningView, mainViewModel.currentMode.value)
    }

    private fun loadSampleWarningJson(): ByteArray {
        val stream = requireNotNull(
            javaClass.classLoader!!.getResourceAsStream("contracts/samples/r4-warning.json"),
        ) { "Missing test resource contracts/samples/r4-warning.json" }
        return stream.use { it.readBytes() }
    }

    private fun sendUdpWarning(payload: ByteArray) {
        DatagramSocket().use { socket ->
            val packet = DatagramPacket(
                payload,
                payload.size,
                InetAddress.getByName(LOOPBACK),
                BuildConfig.R4_UDP_PORT,
            )
            socket.send(packet)
        }
    }

    /**
     * Probe-bind loop: wait until [BuildConfig.R4_UDP_PORT] is occupied by the service
     * (bind failure ⇒ listener is up), then release the probe.
     */
    private suspend fun awaitUdpListenerReady() {
        withTimeout(UDP_LISTENER_READY_TIMEOUT_MS) {
            while (true) {
                val busy = runCatching {
                    DatagramSocket(BuildConfig.R4_UDP_PORT).use { /* probe */ }
                    false
                }.getOrDefault(true)
                if (busy) return@withTimeout
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    companion object {
        private const val LOOPBACK = "127.0.0.1"
        private const val POLL_INTERVAL_MS = 50L
        private const val UDP_LISTENER_READY_TIMEOUT_MS = 5_000L
        private const val UDP_PROPAGATION_TIMEOUT_MS = 8_000L
    }
}
