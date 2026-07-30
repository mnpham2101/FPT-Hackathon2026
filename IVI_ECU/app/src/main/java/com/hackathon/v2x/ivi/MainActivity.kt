package com.hackathon.v2x.ivi

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.lifecycle.lifecycleScope
import com.hackathon.v2x.ivi.net.R4UdpListener
import com.hackathon.v2x.ivi.ui.MainViewModel
import com.hackathon.v2x.ivi.ui.screen.MainScreen
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        lifecycleScope.launch {
            try {
                R4UdpListener().listen { warning ->
                    viewModel.onR4Warning(warning)
                }
            } catch (error: Exception) {
                Log.e("R4UdpListener", "R4 UDP listener stopped", error)
            }
        }

        setContent {
            MainScreen(viewModel = viewModel)
        }
    }
}
