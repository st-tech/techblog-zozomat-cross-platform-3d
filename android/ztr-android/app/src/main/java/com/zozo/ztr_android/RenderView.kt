//
// See LICENSE.txt for this sample’s licensing information.
//
// RenderView.kt
// ZOZO Technologies Cross Platform Renderer Example
//

package com.zozo.ztr_android

import android.content.Context
import android.content.res.AssetManager
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class RenderView(context: Context, onSurfaceCreatedClosure: ((view: RenderView) -> Unit)?) : GLSurfaceView(context) {

    init {

        // Pick an EGLConfig with RGB8 color, 16-bit depth, no stencil,
        // supporting OpenGL ES 2.0 or later backwards-compatible versions
        setEGLConfigChooser(8, 8, 8, 0, 16, 0)
        setEGLContextClientVersion(3)

        var renderer = Renderer(context.assets)

        renderer.onSurfaceCreatedClosure = onSurfaceCreatedClosure
        renderer.view = this

        setRenderer(renderer)
    }

    // Pull these directly from the NDK
    private var _mouseDown = 0
    private var _mouseDownUp = 0
    private var _mouseX = 0
    private var _mouseY = 0

    override fun onTouchEvent(event: MotionEvent?): Boolean {
        when (event?.action) {
            MotionEvent.ACTION_DOWN -> {
                _mouseDown = 1
                _mouseDownUp = 1
                _mouseX = event.x.toInt()
                _mouseY = event.y.toInt()
            }
            MotionEvent.ACTION_MOVE -> {
                _mouseDownUp = 0
                _mouseX = event.x.toInt()
                _mouseY = event.y.toInt()
            }
            else -> {
                _mouseDown = 0
                _mouseDownUp = 1
            }
        }
        return true
    }

    inner class Renderer(val assetManager: AssetManager) : GLSurfaceView.Renderer {

        var view: RenderView? = null
        var onSurfaceCreatedClosure: ((view: RenderView) -> Unit)? = null

        override fun onDrawFrame(gl: GL10) {
            RenderLib.draw(
                    this@RenderView._mouseDown,
                    this@RenderView._mouseDownUp,
                    this@RenderView._mouseX,
                    this@RenderView._mouseY
            )
        }

        override fun onSurfaceChanged(gl: GL10, width: Int, height: Int) {
            RenderLib.resize(width, height)
        }

        override fun onSurfaceCreated(gl: GL10, config: EGLConfig) {
            RenderLib.init(this.assetManager)

            this.view?.let {
                onSurfaceCreatedClosure?.invoke(it)
            }
        }
    }
}
