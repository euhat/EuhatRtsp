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
#pragma once
#include <string>
#include <list>
#include <unistd.h>
#include <jni.h>
#include <android/native_window.h>
#include <pthread.h>
#include <stdatomic.h>
#include "librtspsource.h"
#include "CommonOp.h"

using namespace std;

#define whSleep(_secs) sleep(_secs)
#define whSleepMs(_ms) usleep(_ms * 1000)
#define WhThreadHandle pthread_t
#define WH_INVALID_THREAD 0
#define WH_THREAD_DEF(_proc, _arg) void *_proc(void *_arg)
#define whThreadCreate(_handle, _loop, _param) pthread_create(&_handle, NULL, _loop, _param)
#define whThreadJoin(_handle) pthread_join(_handle, NULL)
#define whThreadTerminate(_handle) pthread_cancel(_handle)

#if 0
#define WhMutex atomic_flag

#define whMutexInit(_handle) *(_handle) = ATOMIC_FLAG_INIT

#define whMutexEnter(_handle) while(atomic_flag_test_and_set(_handle))
#define whMutexLeave(_handle) atomic_flag_clear(_handle)
#define whCondWait(_cond, _mutex)

#define whMutexFini(_handle)
#else
struct WhMutexStruct
{
	pthread_mutexattr_t attr_;
	pthread_mutex_t m_;
};
#define WhMutexParam WhMutexStruct

#define whMutexInit(_handle) \
do { \
	if (pthread_mutexattr_init(&(_handle)->attr_) != 0) { \
		DBG(("init mutex attr failed.\n")); \
		break; \
				} \
	pthread_mutexattr_settype(&(_handle)->attr_, PTHREAD_MUTEX_RECURSIVE_NP); \
	pthread_mutex_init(&(_handle)->m_, &(_handle)->attr_); \
} while (0)

#define whMutexEnter(_handle) pthread_mutex_lock(&(_handle)->m_)
#define whMutexLeave(_handle) pthread_mutex_unlock(&(_handle)->m_)
#define whCondWait(_handle, _mutex) pthread_cond_wait(_handle, &(_mutex)->m_)

#define whMutexFini(_handle) \
do { \
	pthread_mutex_destroy(&(_handle)->m_); \
	pthread_mutexattr_destroy(&(_handle)->attr_); \
} while (0)
#endif

#define WhMutex WhMutexStruct

class WhMutexGuard
{
public:
	WhMutexGuard(WhMutex *cs)
	{
		cs_ = cs;
		whMutexEnter(cs_);
	}
	~WhMutexGuard()
	{
		whMutexLeave(cs_);
	}
	WhMutex *cs_;
};

#define MEM_POOL_WIDTH_MAX      1920
#define MEM_POOL_HEIGHT_MAX     1080

class MemPool
{
    int unitSize_;
    int maxCount_;
    list<void *> pool_;
    WhMutex mutex_;

public:
    MemPool(int unitSize, int maxCount);
    ~MemPool();

    void *alloc(int size);
    void dealloc(void *);
};

JavaVM *getVM();

class EuhatDecoderBase;

#define EUHAT_STATUS_RTSP_CONNECT_FAILED        -1
#define EUHAT_STATUS_RTSP_RECONNECTING          -2
#define EUHAT_STATUS_DECODING_QUEUE_TOO_LARGE   -3
#define EUHAT_STATUS_DISPLAY_QUEUE_TOO_LARGE    -4

typedef struct {
    int type;
    int param0;
    int param1;
} StatusCallbackOnStatusParams;

typedef struct {
    jmethodID onStatus;
} StatusCallbackObjMethods;

typedef struct {
	jmethodID onFrame;
} FrameCallbackObjMethods;

typedef struct {
    int type;
    string str;
    int iParam;
} MessageParams;

class EuhatRtspOp
{
    static void streamCallBack(int streamType, unsigned char *frame, unsigned len, void *context, FrameInfo *info);

    int initRtspSource();
    void finiRtspSource();
    void startWorkThread();
    void stopWorkThread();

    int postMessage(MessageParams &params);
    int postMessageStr(int type, const char *str);
    int postMessageInt(int type, int param);
    int connect2(const char *url);
    int close2();
    int connect3(const char *url);
    int close3();
    int onTimer();

    int informStatus(int type, int param0, int param1);

    MemPool *h264MemPool_;
    MemPool *yuvMemPool_;
    MemPool *rgbMemPool_;

    void *rtsp_;
    string rtspUrl_;
    int rtspUseFlags_;
    int rtspFlags_;
    int rtspTimeOut_;
    int reconnectTimeOut_;
    int displayFps_;
    int displayBufCount_;
    unsigned long rtspLastGetTime_;

    int hasResolution_;
    int hasFirstFrame_;
    int videoWidth_;
    int videoHeight_;
    int videoHeightOff_;
    WhMutex mutexHasResolution_;
    pthread_cond_t syncHasResolution_;

    int needReconnecting_;
    int isConnected_;
    int isPlaying_;
    int isExiting_;
    int isTimerThreadExiting_;

    WhMutex mutexDecoder_;
    pthread_cond_t syncDecoder_;
    list<char *> listDecodeFrame_;
    EuhatDecoderBase *decoder_;
    WhThreadHandle decodingThreadHandle_;

    WhMutex mutexPreview_;
    pthread_cond_t syncPreview_;
    WhMutex mutexPreviewDraw_;
    ANativeWindow *winPreview_;
    int winPreviewNeedDraw_;
    list<char *> listPreviewFrame_;
    WhThreadHandle previewThreadHandle_;

    WhMutex mutexStatusCallback_;
    WhMutex mutexStatusCallbackCall_;
    list<StatusCallbackOnStatusParams> listStatusCallbackOnStatus_;
    jobject objStatusCallback_;
    StatusCallbackObjMethods methodsStatusCallback_;
    WhThreadHandle statusCallbackThreadHandle_;

    WhMutex mutexFrameCallback_;
    pthread_cond_t syncFrameCallback_;
    WhMutex mutexFrameCallbackCall_;
    list<char *> listFrameCallback_;
    jobject objFrameCallback_;
    FrameCallbackObjMethods methodsFrameCallback_;
    int pixelFormat_;
    WhThreadHandle frameCallbackThreadHandle_;

    WhThreadHandle timerThreadHandle_;

    WhMutex mutexMessage_;
    list<MessageParams> listMessage_;
    WhThreadHandle messageThreadHandle_;

public:
    EuhatRtspOp();
    ~EuhatRtspOp();

    int connect(const char *url, int rtspTimeOut, int rtspFlag, int reconnectTimeOut, int displayFps, int displayBufCount);
    int close();
    int startPreview();
    int stopPreview();
    int setPreviewDisplay(ANativeWindow *nativeWin);
    int setStatusCallback(JNIEnv *env, jobject statusCallbackObj);
    int setFrameCallback(JNIEnv *env, jobject frameCallbackObj, int pixel_format);
    int onTime();

    int decodingProc();
    int previewProc();
    int frameCallbackProc();
    int statusCallbackProc();
    int timerProc();
    int messageProc();

    int decodingCb(char *yuv);
};
