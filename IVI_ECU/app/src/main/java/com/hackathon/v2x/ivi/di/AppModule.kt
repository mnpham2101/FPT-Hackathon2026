package com.hackathon.v2x.ivi.di

import com.hackathon.v2x.ivi.data.R4Deserializer
import com.hackathon.v2x.ivi.data.R4Repository
import com.hackathon.v2x.ivi.ui.view.CanvasWarningView
import com.hackathon.v2x.ivi.ui.view.IviWarningViewSeam
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

/**
 * Application-wide Hilt bindings (16.5.4.1).
 *
 * Wires the data/UI seams: deserializer, R4 repository singleton, and the
 * committed 2D [IviWarningViewSeam] implementation ([CanvasWarningView]).
 */
@Module
@InstallIn(SingletonComponent::class)
object AppModule {

    @Provides
    @Singleton
    fun provideR4Deserializer(): R4Deserializer = R4Deserializer()

    @Provides
    @Singleton
    fun provideR4Repository(): R4Repository = R4Repository()

    @Provides
    @Singleton
    fun provideIviWarningViewSeam(): IviWarningViewSeam = CanvasWarningView()
}
