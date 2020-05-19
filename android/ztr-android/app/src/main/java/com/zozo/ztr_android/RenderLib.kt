package com.zozo.ztr_android
import android.content.res.AssetManager
import android.util.Log

public class RenderLib {

    companion object {
        init {
            System.loadLibrary("native-lib-renderer")
        }

        @JvmStatic
        external fun init(assetManager: AssetManager)

        @JvmStatic
        external fun draw(mouseDown: Int, mouseDownUp: Int, x: Int, y: Int)

        @JvmStatic
        external fun resize(width: Int, height: Int)

        @JvmStatic
        external fun free()

        @JvmStatic
        fun blah() {
            Log.d("TAG", "KAP blah was called")
        }
    }
}