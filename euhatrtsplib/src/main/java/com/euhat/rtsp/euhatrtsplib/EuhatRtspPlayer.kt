/*
 *  EuhatRtsp
 *  library and sample to play and acquire rtsp stream on Android device
 *
 * Copyright (c) 2014-2018 Euhat.com euhat@hotmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *  All files in the folder are under this Apache License, Version 2.0.
 *  Files in the libjpeg-turbo, libusb, libuvc, rapidjson folder
 *  may have a different license, see the respective files.
 */
package com.euhat.rtsp.euhatrtsplib

import android.graphics.SurfaceTexture
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder

class EuhatRtspPlayer {
    companion object {
        private const val DEBUG = false
        private val TAG = EuhatRtspPlayer::class.java.simpleName
        const val DEFAULT_PREVIEW_WIDTH = 1920
        const val DEFAULT_PREVIEW_HEIGHT = 1080
        const val DEFAULT_PREVIEW_FPS = 25
        const val PIXEL_FORMAT_YV12 = 1
        const val PIXEL_FORMAT_NV21 = 2

        init {
            try {
                System.loadLibrary("EuhatRtsp")
            } catch (ex:Throwable) {
                ex.printStackTrace()
            }
        }
    }

    protected var mCurrentWidth = DEFAULT_PREVIEW_WIDTH
    protected var mCurrentHeight = DEFAULT_PREVIEW_HEIGHT
    protected var mNativePtr: Long
    @Synchronized
    fun open(url: String, rtspTimeOut: Int, rtspFlag: Int, reconnectTimeOut: Int, displayFps: Int, displayBufCount: Int) {
        val result: Int
        result = try {
            nativeConnect(mNativePtr, url, rtspTimeOut, rtspFlag, reconnectTimeOut, displayFps, displayBufCount)
        } catch (e: Exception) {
            Log.w(TAG, e)
            -1
        }
        if (result != 0) {
            throw UnsupportedOperationException("open failed, result is $result")
        }
    }

    fun open(url: String) {
        open(url, 8000, 1, 10000, 25, 100)
    }

    @Synchronized
    fun close() {
        stopPreview()
        if (mNativePtr != 0L) {
            nativeClose(mNativePtr)
        }
        if (DEBUG) Log.v(TAG, "closed.")
    }

    @Synchronized
    fun setPreviewDisplay(holder: SurfaceHolder) {
        nativeSetPreviewDisplay(mNativePtr, holder.surface)
    }

    @Synchronized
    fun setPreviewTexture(texture: SurfaceTexture?) {
        val surface = Surface(texture) // XXX API >= 14
        nativeSetPreviewDisplay(mNativePtr, surface)
    }

    @Synchronized
    fun setPreviewDisplay(surface: Surface) {
        nativeSetPreviewDisplay(mNativePtr, surface)
    }

    fun setStatusCallback(callback: IStatusCallback) {
        nativeSetStatusCallback(mNativePtr, callback)
    }

    fun setFrameCallback(callback: IFrameCallback?, pixelFormat: Int) {
        nativeSetFrameCallback(mNativePtr, callback, pixelFormat)
    }

    @Synchronized
    fun startPreview() {
        nativeStartPreview(mNativePtr)
    }

    @Synchronized
    fun stopPreview() {
        setFrameCallback(null, 0)
        nativeStopPreview(mNativePtr)
    }

    @Synchronized
    fun destroy() {
        close()
        if (mNativePtr != 0L) {
            nativeDestroy(mNativePtr)
            mNativePtr = 0
        }
    }

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(euhatRtsp: Long)
    private external fun nativeConnect(euhatRtsp: Long, url: String, rtspTimeOut: Int, rtspFlag: Int, reconnectTimeOut: Int, displayFps: Int, displayBufCount: Int): Int
    private external fun nativeClose(euhatRtsp: Long): Int
    private external fun nativeStartPreview(euhatRtsp: Long): Int
    private external fun nativeStopPreview(euhatRtsp: Long): Int
    private external fun nativeSetPreviewDisplay(euhatRtsp: Long, surface: Surface): Int
    private external fun nativeSetStatusCallback(euhatRtsp: Long, callback: IStatusCallback): Int
    private external fun nativeSetFrameCallback(euhatRtsp: Long, callback: IFrameCallback?, pixelFormat: Int): Int

    init {
        mNativePtr = nativeCreate()
    }
}