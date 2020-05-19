package com.zozo.ztr_android

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // TODO: Closure to ensure we only load after the view is shown.
        // Probably not needed, can just fail if the view isn't ready?
        val onSurfaceCreatedClosure: (view: RenderView) -> Unit = {
            val assetManager = assets
        }

        var renderView = RenderView(application, onSurfaceCreatedClosure)
        setContentView (renderView)
    }
}
