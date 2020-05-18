package com.zozo.ztr_android

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import java.io.IOException

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // TODO: Closure to ensure we only load after the view is shown.
        // Probably not needed, can just fail if the view isn't ready?
        val onSurfaceCreatedClosure: (view: ZTRJNIView) -> Unit = {
            val assetManager = assets
        }

        var ztrView = ZTRJNIView(application, onSurfaceCreatedClosure)
        setContentView (ztrView)
    }
}
