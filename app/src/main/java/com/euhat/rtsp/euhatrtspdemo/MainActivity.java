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

package com.euhat.rtsp.euhatrtspdemo;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.ByteBuffer;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.Locale;

import android.graphics.SurfaceTexture;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;
import android.view.Gravity;
import android.view.Surface;
import android.view.TextureView.SurfaceTextureListener;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.TextureView;
import android.view.Window;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import com.euhat.rtsp.euhatrtsplib.IFrameCallback;
import com.euhat.rtsp.euhatrtsplib.EuhatRtspPlayer;
import com.euhat.rtsp.euhatrtsplib.IStatusCallback;

public final class MainActivity extends AppCompatActivity {
	private static final boolean DEBUG = true;
	private static final String TAG = "MainActivity";

	private final Object mSync = new Object();
	private EuhatRtspPlayer mEuhatPlayer;
	private TextureView mEuhatView;
	private Surface mPreviewSurface;
	private boolean mIsStarted = false;

	@Override
	protected void onCreate(final Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		setContentView(R.layout.activity_main);

		Button btnStart = (Button)findViewById(R.id.startBtn);
		btnStart.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				synchronized (mSync) {
					if (mIsStarted)
						return;

					final SurfaceTexture st = mEuhatView.getSurfaceTexture();
					if (st != null) {
						if (null == mPreviewSurface) {
							mPreviewSurface = new Surface(st);
							mEuhatPlayer.setPreviewDisplay(mPreviewSurface);
						}
						mEuhatPlayer.startPreview();
					}
					// using UVCCamera.PIXEL_FORMAT_RAW or UVCCamera.PIXEL_FORMAT_YUV need not do transforming.
					mEuhatPlayer.setFrameCallback(mIFrameCallback, EuhatRtspPlayer.PIXEL_FORMAT_NV21);
					mEuhatPlayer.setStatusCallback(mIStatusCallback);

					EditText edUrl = (EditText)findViewById(R.id.euhatPlayerUrl);
					String url = edUrl.getText().toString();
					EditText edFps = (EditText)findViewById(R.id.euhatPlayerFps);
					int fps = Integer.parseInt(edFps.getText().toString());

					// url = "rtsp://192.168.1.67:8554/h264";
					// url = "rtsp://192.168.1.41:554/";
					// url = "rtsp://admin:admin@192.168.1.224/1";
					mEuhatPlayer.open(url, 8000, 1, 1000, fps, 300);

					mIsStarted = true;
				}
			}
		});

		Button btnStop = (Button)findViewById(R.id.stopBtn);
		btnStop.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				synchronized (mSync) {
					if (!mIsStarted)
						return;

					mEuhatPlayer.close();

					mIsStarted = false;
				}
			}
		});

		mEuhatView = (TextureView)findViewById(R.id.euhatPlayerTextureView1);

		mEuhatPlayer = new EuhatRtspPlayer();

		writeLog("hello, I'm here.");
	}
	@Override
	protected void onStart() {
		super.onStart();
	}
	@Override
	protected void onStop() {
		super.onStop();
	}
	@Override
	public void onDestroy() {
		synchronized (mSync) {
			mEuhatPlayer.destroy();
			mEuhatPlayer = null;
		}
		super.onDestroy();
	}

	private void writeLog(String msg)
	{
		try {
			FileOutputStream fos = new FileOutputStream("/sdcard/test/test.txt", true);
			OutputStreamWriter osWriter = new OutputStreamWriter(fos);
			BufferedWriter writer = new BufferedWriter(osWriter);

			Calendar cal = Calendar.getInstance();
			String timeStamp = cal.get(Calendar.MINUTE) + "-" + cal.get(Calendar.SECOND) + "-" + cal.get(Calendar.MILLISECOND) + ": ";
			writer.write(timeStamp);
			writer.write(msg);
			writer.newLine();
			writer.flush();
			osWriter.flush();
			fos.flush();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private final IFrameCallback mIFrameCallback = new IFrameCallback() {
		@Override
		public void onFrame(final ByteBuffer frame, int width, int height) {
			String msg = "get yuv data here, width " + width + ", height " + height + ".";
			Log.v("euhat", msg);
//			writeLog(msg);
		}
	};

	private final IStatusCallback mIStatusCallback = new IStatusCallback() {
		@Override
		public void onStatus(int type, int param0, int param1) {
			String msg;
			switch (type) {
				case -1: msg = "rtsp connect failed."; break;
				case -2: msg = "rtsp reconnecting."; break;
				case -3: msg = "decoding queue is too large."; break;
				case -4: msg = "display queue is too large."; break;
				default: msg = "unknown status message."; break;
			}
			Log.v("euhat1", msg);
			TextView txtView = (TextView)findViewById(R.id.euhatPlayerStatus);
			txtView.setText(msg);
			txtView.invalidate();
		}
	};
}
