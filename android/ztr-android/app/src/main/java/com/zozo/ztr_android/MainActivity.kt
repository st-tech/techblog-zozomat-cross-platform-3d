//
// See LICENSE.txt for this sample’s licensing information.
//
// MainActivity.kt
// ZOZO Technologies Cross Platform Renderer Example
//

package com.zozo.ztr_android

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val onSurfaceCreatedClosure: (view: RenderView) -> Unit = {
            val assetManager = assets
        }

        var renderView = RenderView(application, onSurfaceCreatedClosure)
        setContentView(renderView)
    }
}
