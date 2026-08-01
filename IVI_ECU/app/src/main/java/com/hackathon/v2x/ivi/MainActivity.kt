package com.hackathon.v2x.ivi

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.IBinder
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.hilt.navigation.compose.hiltViewModel
import com.hackathon.v2x.ivi.service.R4ListenerService
import com.hackathon.v2x.ivi.ui.MainViewModel
import com.hackathon.v2x.ivi.ui.WarningViewModel
import com.hackathon.v2x.ivi.ui.screen.MainScreen
import com.hackathon.v2x.ivi.ui.view.IviWarningViewSeam
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject

/**
 * AAOS HMI entry point (16.5.4.1).
 *
 * Starts [R4ListenerService], binds its event flow into [WarningViewModel],
 * and hosts the Compose R16 scaffold with wake-on-warning via [MainViewModel].
 */
@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    @Inject
    lateinit var warningViewSeam: IviWarningViewSeam

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val serviceIntent = Intent(this, R4ListenerService::class.java)
        ContextCompat.startForegroundService(this, serviceIntent)

        setContent {
            val warningViewModel: WarningViewModel = hiltViewModel()

            BindR4ListenerService(warningViewModel)

            val mainViewModel: MainViewModel = viewModel(
                factory = MainViewModelFactory(warningViewModel),
            )

            MainScreen(
                mainViewModel = mainViewModel,
                warningViewModel = warningViewModel,
                warningViewSeam = warningViewSeam,
            )
        }
    }
}

/**
 * Binds [R4ListenerService] for the lifetime of the composition and attaches
 * its [R4ListenerService.r4EventFlow] to [WarningViewModel].
 */
@Composable
private fun BindR4ListenerService(warningViewModel: WarningViewModel) {
    val context = LocalContext.current
    DisposableEffect(warningViewModel) {
        val connection = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                val listener = (service as R4ListenerService.LocalBinder).getService()
                warningViewModel.attachService(listener.r4EventFlow)
            }

            override fun onServiceDisconnected(name: ComponentName?) = Unit
        }
        val intent = Intent(context, R4ListenerService::class.java)
        context.bindService(intent, connection, Context.BIND_AUTO_CREATE)
        onDispose {
            context.unbindService(connection)
        }
    }
}

/**
 * Constructs [MainViewModel] with the live [WarningViewModel.uiWarningState]
 * flow — Hilt cannot inject one `@HiltViewModel` into another.
 */
private class MainViewModelFactory(
    private val warningViewModel: WarningViewModel,
) : ViewModelProvider.Factory {
    @Suppress("UNCHECKED_CAST")
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        require(modelClass.isAssignableFrom(MainViewModel::class.java)) {
            "Unknown ViewModel class: $modelClass"
        }
        return MainViewModel(warningViewModel.uiWarningState) as T
    }
}
