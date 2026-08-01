package com.hackathon.v2x.ivi

import app.cash.turbine.test
import com.hackathon.v2x.ivi.data.R4Repository
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.R3Timestamps
import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4ServiceError
import com.hackathon.v2x.ivi.model.R4StateMessage
import com.hackathon.v2x.ivi.model.R4Vehicles
import com.hackathon.v2x.ivi.model.R4WarningEvent
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

/**
 * Unit tests for [R4Repository] event routing and last-value-wins state (4.5.1.4 / 17.5.4.2).
 */
@OptIn(ExperimentalCoroutinesApi::class)
class R4RepositoryTest {

    private lateinit var repository: R4Repository
    private lateinit var serviceFlow: MutableSharedFlow<R4Message>

    @Before
    fun setUp() {
        repository = R4Repository()
        serviceFlow = MutableSharedFlow(extraBufferCapacity = 8)
    }

    private fun warning(id: String = "C-001") = R4WarningEvent(
        schemaVersion = 1,
        warningType = R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION,
        riskState = "high",
        objectSnapshot = R3Snapshot(
            id = id,
            source = R3Snapshot.SOURCE_V2X_RELAYED,
            position = VehiclePosition(25f, 0f),
            timestamps = R3Timestamps(),
        ),
        geometry = SceneGeometry(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(15f, 0f),
            vehicleC = VehiclePosition(25f, 0f),
        ),
    )

    private fun state(seq: Long) = R4StateMessage(
        schemaVersion = 1,
        seq = seq,
        vehicles = R4Vehicles(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(seq.toFloat(), 0f),
            vehicleC = null,
        ),
    )

    @Test
    fun warningEvent_isRoutedToWarningEventsFlow() = runTest(UnconfinedTestDispatcher()) {
        repository.attachToService(serviceFlow, backgroundScope)

        repository.warningEvents.test {
            serviceFlow.emit(warning("trk-1"))
            val event = awaitItem()
            assertEquals("trk-1", event.objectSnapshot.id)
            assertEquals("high", event.riskState)
            cancelAndIgnoreRemainingEvents()
        }
    }

    @Test
    fun stateMessages_lastValueWinsOnCurrentState() = runTest(UnconfinedTestDispatcher()) {
        repository.attachToService(serviceFlow, backgroundScope)
        assertNull(repository.currentState.value)

        serviceFlow.emit(state(seq = 1))
        assertEquals(1L, repository.currentState.value?.seq)

        serviceFlow.emit(state(seq = 7))
        assertEquals(7L, repository.currentState.value?.seq)
        assertEquals(7f, repository.currentState.value?.vehicles?.vehicleB?.x)
    }

    @Test
    fun serviceError_doesNotMutateWarningOrStateFlows() = runTest(UnconfinedTestDispatcher()) {
        repository.attachToService(serviceFlow, backgroundScope)
        serviceFlow.emit(state(seq = 3))

        repository.warningEvents.test {
            expectNoEvents()
            serviceFlow.emit(R4ServiceError())
            expectNoEvents()
            cancelAndIgnoreRemainingEvents()
        }
        assertEquals(3L, repository.currentState.value?.seq)
    }

    @Test
    fun mixedTraffic_routesWarningsAndStatesIndependently() = runTest(UnconfinedTestDispatcher()) {
        repository.attachToService(serviceFlow, backgroundScope)

        repository.warningEvents.test {
            serviceFlow.emit(state(seq = 10))
            serviceFlow.emit(warning("mix-c"))
            serviceFlow.emit(state(seq = 11))

            val event = awaitItem()
            assertEquals("mix-c", event.objectSnapshot.id)
            assertEquals(11L, repository.currentState.value?.seq)
            cancelAndIgnoreRemainingEvents()
        }
    }
}
