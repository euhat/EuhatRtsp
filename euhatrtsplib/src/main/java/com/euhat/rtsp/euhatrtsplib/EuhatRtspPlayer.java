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

package com.euhat.rtsp.euhatrtsplib;

import java.io.File;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.graphics.SurfaceTexture;
import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

public class EuhatRtspPlayer {
	private static final boolean DEBUG = false;
	private static final String TAG = EuhatRtspPlayer.class.getSimpleName();

	public static final int DEFAULT_PREVIEW_WIDTH = 1920;
	public static final int DEFAULT_PREVIEW_HEIGHT = 1080;
	public static final int DEFAULT_PREVIEW_FPS = 25;

	public static final int PIXEL_FORMAT_YV12 = 1;
	public static final int PIXEL_FORMAT_NV21 = 2;

	static {
		System.loadLibrary("EuhatRtsp");
	}

	protected int mCurrentWidth = DEFAULT_PREVIEW_WIDTH, mCurrentHeight = DEFAULT_PREVIEW_HEIGHT;
    protected long mNativePtr;

    public EuhatRtspPlayer() {
    	mNativePtr = nativeCreate();
	}

    public synchronized void open(String url, int rtspTimeOut, int rtspFlag, int reconnectTimeOut, int displayFps, int displayBufCount) {
    	int result;
    	try {
			result = nativeConnect(mNativePtr, url, rtspTimeOut, rtspFlag, reconnectTimeOut, displayFps, displayBufCount);
		} catch (final Exception e) {
			Log.w(TAG, e);
			result = -1;
		}
		if (result != 0) {
			throw new UnsupportedOperationException("open failed, result is " + result);
		}
    }

    public void open(String url) {
    	open(url, 8000, 1, 10000, 25, 100);
	}

    public synchronized void close() {
    	stopPreview();
    	if (mNativePtr != 0) {
    		nativeClose(mNativePtr);
    	}
    	if (DEBUG) Log.v(TAG, "closed.");
    }

    public synchronized void setPreviewDisplay(final SurfaceHolder holder) {
   		nativeSetPreviewDisplay(mNativePtr, holder.getSurface());
    }

    public synchronized void setPreviewTexture(final SurfaceTexture texture) {
    	final Surface surface = new Surface(texture);	// XXX API >= 14
    	nativeSetPreviewDisplay(mNativePtr, surface);
    }

    public synchronized void setPreviewDisplay(final Surface surface) {
    	nativeSetPreviewDisplay(mNativePtr, surface);
    }

    public void setStatusCallback(final IStatusCallback callback) {
    	nativeSetStatusCallback(mNativePtr, callback);
	}

    public void setFrameCallback(final IFrameCallback callback, final int pixelFormat) {
       	nativeSetFrameCallback(mNativePtr, callback, pixelFormat);
    }

    public synchronized void startPreview() {
   		nativeStartPreview(mNativePtr);
    }

    public synchronized void stopPreview() {
    	setFrameCallback(null, 0);
    	nativeStopPreview(mNativePtr);
    }

    public synchronized void destroy() {
    	close();
    	if (mNativePtr != 0) {
    		nativeDestroy(mNativePtr);
    		mNativePtr = 0;
    	}
    }

    private final native long nativeCreate();
    private final native void nativeDestroy(long euhatRtsp);

    private final native int nativeConnect(long euhatRtsp, String url, int rtspTimeOut, int rtspFlag, int reconnectTimeOut, int displayFps, int displayBufCount);
    private static final native int nativeClose(long euhatRtsp);

    private static final native int nativeStartPreview(long euhatRtsp);
    private static final native int nativeStopPreview(long euhatRtsp);
    private static final native int nativeSetPreviewDisplay(long euhatRtsp, final Surface surface);
    private static final native int nativeSetStatusCallback(long euhatRtsp, final IStatusCallback callback);
    private static final native int nativeSetFrameCallback(long euhatRtsp, final IFrameCallback callback, int pixelFormat);

}
