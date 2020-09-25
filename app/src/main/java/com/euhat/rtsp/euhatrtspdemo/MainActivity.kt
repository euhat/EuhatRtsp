/*
 *  EuhatRtspDemo
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
package com.euhat.rtsp.euhatrtspdemo

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.util.Log
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.Window
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import com.euhat.rtsp.euhatrtsplib.EuhatRtspPlayer
import com.euhat.rtsp.euhatrtsplib.IFrameCallback
import com.euhat.rtsp.euhatrtsplib.IStatusCallback
import java.io.BufferedWriter
import java.io.FileOutputStream
import java.io.IOException
import java.io.OutputStreamWriter
import java.nio.ByteBuffer
import java.util.*

class MainActivity : AppCompatActivity() {
    private val mSync = Any()
    private var mEuhatPlayer: EuhatRtspPlayer? = null
    private var mEuhatView: TextureView? = null
    private var mPreviewSurface: Surface? = null
    private var mIsStarted = false
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        setContentView(R.layout.activity_main)
        val btnStart = findViewById<View>(R.id.startBtn) as Button
        btnStart.setOnClickListener(View.OnClickListener {
            synchronized(mSync) {
                if (mIsStarted) return@OnClickListener
                val st = mEuhatView!!.surfaceTexture
                if (st != null) {
                    if (null == mPreviewSurface) {
                        mPreviewSurface = Surface(st)
                        mEuhatPlayer!!.setPreviewDisplay(mPreviewSurface!!)
                    }
                    mEuhatPlayer!!.startPreview()
                }
                // using UVCCamera.PIXEL_FORMAT_RAW or UVCCamera.PIXEL_FORMAT_YUV need not do transforming.
                mEuhatPlayer!!.setFrameCallback(mIFrameCallback, EuhatRtspPlayer.PIXEL_FORMAT_NV21)
                mEuhatPlayer!!.setStatusCallback(mIStatusCallback)
                val edUrl = findViewById<View>(R.id.euhatPlayerUrl) as EditText
                val url = edUrl.text.toString()
                val edFps = findViewById<View>(R.id.euhatPlayerFps) as EditText
                val fps = edFps.text.toString().toInt()

                // url = "rtsp://192.168.1.67:8554/h264";
                // url = "rtsp://192.168.1.41:554/";
                // url = "rtsp://admin:admin@192.168.1.224/1";
                mEuhatPlayer!!.open(url, 8000, 0, 1000, fps, 300)
                mIsStarted = true
            }
        })
        val btnStop = findViewById<View>(R.id.stopBtn) as Button
        btnStop.setOnClickListener(View.OnClickListener {
            synchronized(mSync) {
                if (!mIsStarted) return@OnClickListener
                mEuhatPlayer!!.close()
                mIsStarted = false
            }
        })
        mEuhatView = findViewById<View>(R.id.euhatPlayerTextureView1) as TextureView
        mEuhatPlayer = EuhatRtspPlayer()
        writeLog("hello, I'm here.")
    }

    override fun onStart() {
        super.onStart()
    }

    override fun onStop() {
        super.onStop()
    }

    public override fun onDestroy() {
        synchronized(mSync) {
            mEuhatPlayer!!.destroy()
            mEuhatPlayer = null
        }
        super.onDestroy()
    }

    private fun writeLog(msg: String) {
        try {
            val fos = FileOutputStream("/sdcard/test/test.txt", true)
            val osWriter = OutputStreamWriter(fos)
            val writer = BufferedWriter(osWriter)
            val cal = Calendar.getInstance()
            val timeStamp = cal[Calendar.MINUTE].toString() + "-" + cal[Calendar.SECOND] + "-" + cal[Calendar.MILLISECOND] + ": "
            writer.write(timeStamp)
            writer.write(msg)
            writer.newLine()
            writer.flush()
            osWriter.flush()
            fos.flush()
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    private val mIFrameCallback: IFrameCallback = object : IFrameCallback {
        override fun onFrame(frame: ByteBuffer?, width: Int, height: Int) {
            val msg = "get yuv data here, width $width, height $height."
            Log.v("euhat", msg)
            //			writeLog(msg);
        }
    }
    private val mIStatusCallback: IStatusCallback = object : IStatusCallback {
        override fun onStatus(type: Int, param0: Int, param1: Int) {
            val msg: String
            msg = when (type) {
                -1 -> "rtsp connect failed."
                -2 -> "rtsp reconnecting."
                -3 -> "decoding queue is too large."
                -4 -> "display queue is too large."
                else -> "unknown status message."
            }
            Log.v("euhat1", msg)
            val txtView = findViewById<View>(R.id.euhatPlayerStatus) as TextView
            txtView.text = msg
            txtView.invalidate()
        }
    }

    companion object {
        private const val DEBUG = true
        private const val TAG = "MainActivity"
    }
}