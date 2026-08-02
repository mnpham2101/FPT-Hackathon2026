package com.hackathon.v2x.ivi

import app.cash.turbine.test
import com.hackathon.v2x.ivi.data.R4Repository
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4StateMessage
import com.hackathon.v2x.ivi.model.R4WarningEvent
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import com.hackathon.v2x.ivi.ui.WarningUiState
import com.hackathon.v2x.ivi.ui.WarningViewModel
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

/**
 * Unit tests for [WarningViewModel] — covers Idle/Active/timeout state transitions (subtask 4.5.1.4).
 */
@OptIn(ExperimentalCoroutinesApi::class)
class WarningViewModelTest {

    private val testDispatcher = StandardTestDispatcher()
    private lateinit var repository: R4Repository
    private lateinit var warningFlow: MutableSharedFlow<R4WarningEvent>
    private lateinit var viewModel: WarningViewModel

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
        warningFlow = MutableSharedFlow(extraBufferCapacity = 8)
        repository = mockk {
            every { warningEvents } returns warningFlow
            every { currentState } returns MutableStateFlow<R4StateMessage?>(null)
        }
        viewModel = WarningViewModel(repository)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    private fun makeWarningEvent(warningType: String = "nlos_obstruction") =
        R4WarningEvent(
            schemaVersion = 1,
            warningType = warningType,
            riskState = "high",
            objectSnapshot = R3Snapshot(
                id = "C-001",
                objectClass = "vehicle",
                source = "v2x_relayed",
                position = VehiclePosition(25f, 1.5f),
                distance = 25f,
                speed = 13.5f,
                confidence = 0.87f,
                state = "tracked",
                timestamps = com.hackathon.v2x.ivi.model.R3Timestamps(measured = 0L, received = 0L, lastUpdated = 0L)
            ),
            geometry = SceneGeometry(
                ego = VehiclePosition(0f, 0f),
                vehicleB = VehiclePosition(15f, 0f),
                vehicleC = VehiclePosition(25f, 1.5f),
            )
        )

    // ── Test 1: Idle → Active on warning event ────────────────────────────────

    @Test
    fun `state transitions from Idle to Active when warning event received`() = runTest {
        viewModel.uiWarningState.test {
            assertEquals(WarningUiState.Idle, awaitItem())

            warningFlow.emit(makeWarningEvent())
            testDispatcher.scheduler.runCurrent()

            val active = awaitItem()
            assertTrue("Expected Active state", active is WarningUiState.Active)
            val event = (active as WarningUiState.Active).event
            assertEquals("nlos_obstruction", event.warningType)

            cancelAndIgnoreRemainingEvents()
        }
    }

    // ── Test 2: Active → Idle after timeout ──────────────────────────────────

    @Test
    fun `state transitions from Active to Idle after WARNING_TIMEOUT_MS`() = runTest {
        viewModel.uiWarningState.test {
            assertEquals(WarningUiState.Idle, awaitItem()) // initial

            warningFlow.emit(makeWarningEvent())
            testDispatcher.scheduler.runCurrent()
            assertTrue("Should be Active", awaitItem() is WarningUiState.Active)

            // Advance past timeout
            advanceTimeBy(11_000L)
            testDispatcher.scheduler.runCurrent()

            assertEquals(WarningUiState.Idle, awaitItem())
            cancelAndIgnoreRemainingEvents()
        }
    }

    // ── Test 3: latestScene updates on warning event ──────────────────────────

    @Test
    fun `latestScene is populated when warning event is received`() = runTest {
        assertNull(viewModel.latestScene.value)

        warningFlow.emit(makeWarningEvent())
        testDispatcher.scheduler.runCurrent()

        val scene = viewModel.latestScene.value
        assertTrue("Scene should be non-null after warning", scene != null)
        assertEquals(25f, scene!!.vehicleC!!.x)
    }

    // ── Test 4: latestScene cleared after timeout ─────────────────────────────

    @Test
    fun `latestScene is cleared after timeout returns to Idle`() = runTest {
        warningFlow.emit(makeWarningEvent())
        testDispatcher.scheduler.runCurrent()
        assertTrue("Scene should be set", viewModel.latestScene.value != null)

        advanceTimeBy(11_000L)
        testDispatcher.scheduler.runCurrent()

        assertNull("Scene should be cleared after timeout", viewModel.latestScene.value)
    }

    // ── Test 5 (R19 regression): provenance guard snapshot wired into scene ───
    //
    // §4.1 fix: vehicleCSnapshot must be composed from event.objectSnapshot, not left
    // null from event.geometry. A null snapshot makes CanvasWarningView treat every
    // ghost-C as trusted and bypasses the R19 provenance guard entirely.

    @Test
    fun `provenance guard snapshot is wired from objectSnapshot into scene geometry`() = runTest {
        val event = makeWarningEvent()
        warningFlow.emit(event)
        testDispatcher.scheduler.runCurrent()

        val scene = viewModel.latestScene.value
        assertTrue("Scene must be non-null", scene != null)
        val snapshot = scene!!.vehicleCSnapshot
        assertTrue(
            "vehicleCSnapshot must not be null — null means R19 guard is inert",
            snapshot != null
        )
        assertEquals(
            "vehicleCSnapshot.source must match objectSnapshot.source",
            event.objectSnapshot.source,
            snapshot!!.source
        )
        assertEquals(
            "vehicleCSnapshot.id must match objectSnapshot.id",
            event.objectSnapshot.id,
            snapshot.id
        )
    }
}

