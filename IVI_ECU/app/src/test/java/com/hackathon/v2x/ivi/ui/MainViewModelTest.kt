package com.hackathon.v2x.ivi.ui

import app.cash.turbine.test
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.R3Timestamps
import com.hackathon.v2x.ivi.model.R4WarningEvent
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

/**
 * Wake-on-warning tests: Standby → Warning → Standby (Lead-simplified HMI).
 */
@OptIn(ExperimentalCoroutinesApi::class)
class MainViewModelTest {

    private val testDispatcher = StandardTestDispatcher()
    private lateinit var warningState: MutableStateFlow<WarningUiState>
    private lateinit var viewModel: MainViewModel

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
        warningState = MutableStateFlow(WarningUiState.Idle)
        viewModel = MainViewModel(warningState)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    private fun makeActiveWarning(): WarningUiState.Active =
        WarningUiState.Active(
            R4WarningEvent(
                schemaVersion = 1,
                warningType = R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION,
                riskState = "high",
                objectSnapshot = R3Snapshot(
                    id = "C-001",
                    source = R3Snapshot.SOURCE_V2X_RELAYED,
                    position = VehiclePosition(25f, 0f),
                    timestamps = R3Timestamps(),
                ),
                geometry = SceneGeometry(
                    ego = VehiclePosition(0f, 0f),
                    vehicleB = VehiclePosition(15f, 0f),
                    vehicleC = VehiclePosition(25f, 0f),
                ),
            ),
        )

    @Test
    fun defaultMode_isStandbyView() {
        assertEquals(DisplayMode.StandbyView, viewModel.currentMode.value)
    }

    @Test
    fun standbyToActive_autoSwitchesToWarningView_andRecordsStandby() = runTest {
        assertEquals(DisplayMode.StandbyView, viewModel.currentMode.value)

        viewModel.currentMode.test {
            assertEquals(DisplayMode.StandbyView, awaitItem())

            warningState.value = makeActiveWarning()
            advanceUntilIdle()

            assertEquals(DisplayMode.WarningView, awaitItem())
            assertEquals(DisplayMode.StandbyView, viewModel.previousMode)
            assertFalse(viewModel.userOverrodeDuringWarning)
            cancelAndIgnoreRemainingEvents()
        }
    }

    @Test
    fun standbyToWarning_thenIdle_restoresStandby() = runTest {
        assertEquals(DisplayMode.StandbyView, viewModel.currentMode.value)

        warningState.value = makeActiveWarning()
        advanceUntilIdle()
        assertEquals(DisplayMode.WarningView, viewModel.currentMode.value)
        assertEquals(DisplayMode.StandbyView, viewModel.previousMode)

        warningState.value = WarningUiState.Idle
        advanceUntilIdle()

        assertEquals(DisplayMode.StandbyView, viewModel.currentMode.value)
    }

    @Test
    fun idleToActive_autoSwitchesToWarningView_andRecordsPreviousMode() = runTest {
        viewModel.setMode(DisplayMode.AppsView)
        advanceUntilIdle()

        viewModel.currentMode.test {
            assertEquals(DisplayMode.AppsView, awaitItem())

            warningState.value = makeActiveWarning()
            advanceUntilIdle()

            assertEquals(DisplayMode.WarningView, awaitItem())
            assertEquals(DisplayMode.AppsView, viewModel.previousMode)
            assertFalse(viewModel.userOverrodeDuringWarning)
            cancelAndIgnoreRemainingEvents()
        }
    }

    @Test
    fun activeToIdle_withoutOverride_restoresPreviousMode() = runTest {
        viewModel.setMode(DisplayMode.SettingsView)
        advanceUntilIdle()

        warningState.value = makeActiveWarning()
        advanceUntilIdle()
        assertEquals(DisplayMode.WarningView, viewModel.currentMode.value)
        assertEquals(DisplayMode.SettingsView, viewModel.previousMode)

        warningState.value = WarningUiState.Idle
        advanceUntilIdle()

        assertEquals(DisplayMode.SettingsView, viewModel.currentMode.value)
    }

    @Test
    fun userOverrideDuringActive_idleDoesNotForceRestore() = runTest {
        viewModel.setMode(DisplayMode.HomeView)
        advanceUntilIdle()

        warningState.value = makeActiveWarning()
        advanceUntilIdle()
        assertEquals(DisplayMode.WarningView, viewModel.currentMode.value)

        viewModel.setMode(DisplayMode.AppsView)
        advanceUntilIdle()
        assertEquals(DisplayMode.AppsView, viewModel.currentMode.value)
        assertTrue(viewModel.userOverrodeDuringWarning)

        warningState.value = WarningUiState.Idle
        advanceUntilIdle()

        assertEquals(DisplayMode.AppsView, viewModel.currentMode.value)
    }
}
