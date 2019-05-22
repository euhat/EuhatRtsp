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
#include "CommonOp.h"
#include "RtspOp.h"
#include "DecodeOp.h"
#include "SPSParser.h"
#include <android/native_window_jni.h>

static WH_THREAD_DEF(frameCallbackThread, arg)
{
	EuhatRtspOp *pThis = (EuhatRtspOp *)arg;
	pThis->frameCallbackProc();
	return 0;
}

MemPool::MemPool(int unitSize, int maxCount)
{
    unitSize_ = unitSize;
    maxCount_ = maxCount;
    whMutexInit(&mutex_);
}

MemPool::~MemPool()
{
    whMutexFini(&mutex_);
    while (pool_.size() > 0) {
        void *mem = pool_.front();
        pool_.pop_front();
        free(mem);
    }
}

void *MemPool::alloc(int size)
{
    if (size > unitSize_) {
        DBG(("need size %d bigger than unit size %d.\n", size, unitSize_));
        return NULL;
    }
    void *mem = NULL;
    WhMutexGuard guard(&mutex_);
    if (pool_.size() > 0) {
        mem = pool_.front();
        pool_.pop_front();
    }
    if (NULL == mem) {
        mem = malloc(unitSize_);
    }
    return mem;
}

void MemPool::dealloc(void *mem)
{
    WhMutexGuard guard(&mutex_);
    pool_.push_back(mem);
    if (pool_.size() > maxCount_) {
        mem = pool_.front();
        pool_.pop_front();
        free(mem);
    }
}

static WH_THREAD_DEF(decodingThread, arg)
{
	EuhatRtspOp *pThis = (EuhatRtspOp *)arg;
	pThis->decodingProc();
	return 0;
}

static WH_THREAD_DEF(previewThread, arg)
{
	EuhatRtspOp *pThis = (EuhatRtspOp *)arg;
	pThis->previewProc();
	return 0;
}

int EuhatRtspOp::connect3(const char *url)
{
    if (needReconnecting_) {
        return 0;
    }
    needReconnecting_ = 1;

    return connect2(url);
}

int EuhatRtspOp::close3()
{
    if (!needReconnecting_) {
        return 0;
    }
    needReconnecting_ = 0;

    close2();

    {
        WhMutexGuard guard(&mutexPreview_);
        while (listPreviewFrame_.size() > 0) {
            char *frame = listPreviewFrame_.front();
            listPreviewFrame_.pop_front();
            yuvMemPool_->dealloc(frame);
        }
    }

    return 0;
}

static WH_THREAD_DEF(timerThread, arg)
{
	EuhatRtspOp *pThis = (EuhatRtspOp *)arg;
	pThis->timerProc();
	return 0;
}

static WH_THREAD_DEF(statusCallbackThread, arg)
{
	EuhatRtspOp *pThis = (EuhatRtspOp *)arg;
	pThis->statusCallbackProc();
	return 0;
}

static WH_THREAD_DEF(messageThread, arg)
{
	EuhatRtspOp *pThis = (EuhatRtspOp *)arg;
	pThis->messageProc();
	return 0;
}

EuhatRtspOp::EuhatRtspOp()
{
    h264MemPool_ = new MemPool(MEM_POOL_WIDTH_MAX * MEM_POOL_HEIGHT_MAX * 3 / 2 + 40, 10);
    yuvMemPool_ = new MemPool(MEM_POOL_WIDTH_MAX * MEM_POOL_HEIGHT_MAX * 3 / 2 + 40, 10);
    rgbMemPool_ = new MemPool(MEM_POOL_WIDTH_MAX * MEM_POOL_HEIGHT_MAX * 4 + 40, 2);

    rtsp_ = NULL;
    rtspUseFlags_ = 0;
    rtspFlags_ = 1;
    rtspTimeOut_ = 8000;
    reconnectTimeOut_ = 10000;
    displayFps_ = 25;
    displayBufCount_ = 100;

    hasResolution_ = 0;
    hasFirstFrame_ = 0;
    whMutexInit(&mutexHasResolution_);
    pthread_cond_init(&syncHasResolution_, NULL);

    needReconnecting_ = 0;
    isConnected_ = 0;
    isPlaying_ = 0;

    whMutexInit(&mutexDecoder_);
    pthread_cond_init(&syncDecoder_, NULL);
    decoder_ = EuhatDecodeOp::genDecoder();
    decoder_->setOutputMemPool(yuvMemPool_);

    whMutexInit(&mutexPreview_);
    pthread_cond_init(&syncPreview_, NULL);
    whMutexInit(&mutexPreviewDraw_);
    winPreview_ = NULL;
    winPreviewNeedDraw_ = 1;

    whMutexInit(&mutexStatusCallback_);
    whMutexInit(&mutexStatusCallbackCall_);
    objStatusCallback_ = NULL;
    methodsStatusCallback_.onStatus = NULL;

    whMutexInit(&mutexFrameCallback_);
    pthread_cond_init(&syncFrameCallback_, NULL);
    whMutexInit(&mutexFrameCallbackCall_);
    objFrameCallback_ = NULL;
    methodsFrameCallback_.onFrame = NULL;

    whMutexInit(&mutexMessage_);
    whThreadCreate(messageThreadHandle_, messageThread, this);

    isTimerThreadExiting_ = 0;
    whThreadCreate(previewThreadHandle_, previewThread, this);
    whThreadCreate(frameCallbackThreadHandle_, frameCallbackThread, this);
    whThreadCreate(timerThreadHandle_, timerThread, this);
    whThreadCreate(statusCallbackThreadHandle_, statusCallbackThread, this);
}

void EuhatRtspOp::startWorkThread()
{
    isExiting_ = 0;
    whThreadCreate(decodingThreadHandle_, decodingThread, this);
}

void EuhatRtspOp::stopWorkThread()
{
	isExiting_ = 1;

    pthread_cond_signal(&syncDecoder_);
    whThreadJoin(decodingThreadHandle_);
    while (listDecodeFrame_.size() > 0) {
        char *frame = listDecodeFrame_.front();
        listDecodeFrame_.pop_front();
        h264MemPool_->dealloc(frame);
    }
}

int EuhatRtspOp::postMessage(MessageParams &params)
{
    WhMutexGuard guard(&mutexMessage_);
    if (params.type != 0) {
        while (listMessage_.size() > 10) {
            listMessage_.pop_front();
        }
    }
    listMessage_.push_back(params);
    return 1;
}

int EuhatRtspOp::postMessageStr(int type, const char *str)
{
    MessageParams params;
    params.type = type;
    params.str = str;
    return postMessage(params);
}

int EuhatRtspOp::postMessageInt(int type, int param)
{
    MessageParams params;
    params.type = type;
    params.iParam = param;
    return postMessage(params);
}

EuhatRtspOp::~EuhatRtspOp()
{
    isTimerThreadExiting_ = 1;
    whThreadJoin(statusCallbackThreadHandle_);
    whThreadJoin(timerThreadHandle_);

    postMessageInt(0, 0);
    whThreadJoin(messageThreadHandle_);

    pthread_cond_signal(&syncFrameCallback_);
    whThreadJoin(frameCallbackThreadHandle_);
    while (listFrameCallback_.size() > 0) {
        char *frame = listFrameCallback_.front();
        listFrameCallback_.pop_front();
        yuvMemPool_->dealloc(frame);
    }
    pthread_cond_signal(&syncPreview_);
    whThreadJoin(previewThreadHandle_);
    while (listPreviewFrame_.size() > 0) {
        char *frame = listPreviewFrame_.front();
        listPreviewFrame_.pop_front();
        yuvMemPool_->dealloc(frame);
    }

    whMutexFini(&mutexMessage_);

    whMutexFini(&mutexFrameCallbackCall_);
    pthread_cond_destroy(&syncFrameCallback_);
    whMutexFini(&mutexFrameCallback_);

    whMutexFini(&mutexStatusCallbackCall_);

    if (winPreview_)
    	ANativeWindow_release(winPreview_);
    whMutexFini(&mutexPreviewDraw_);
    pthread_cond_destroy(&syncPreview_);
    whMutexFini(&mutexPreview_);

    delete decoder_;
    pthread_cond_destroy(&syncDecoder_);
    whMutexFini(&mutexDecoder_);

    pthread_cond_destroy(&syncHasResolution_);
    whMutexFini(&mutexHasResolution_);

    delete h264MemPool_;
    delete yuvMemPool_;
    delete rgbMemPool_;
}

void getResolutionFromSPS(unsigned char *buf, const int nLen, int &Width, int &Height)
{
	int iWidth = 0, iHeight = 0;
	SPSParser spsReader;
	bs_t s;
	s.p			= (uint8_t *)buf;
	s.p_start	= (uint8_t *)buf;
	s.p_end		= (uint8_t *)(buf + nLen);
	s.i_left	= 8;

	if (spsReader.Do_Read_SPS(&s, &iWidth, &iHeight) != 0)
	{
		iWidth = 1920;
		iHeight = 1088;
	}

	Width = iWidth;
	Height = iHeight;
}

char NALH[4] = {0x00, 0x00, 0x00, 0x01};

void EuhatRtspOp::streamCallBack(int streamType, unsigned char *frame, unsigned len, void *context, FrameInfo *info)
{
	EuhatRtspOp *euhatRtsp = (EuhatRtspOp *)context;
	char *fData = NULL;
	char *pYUV = NULL;
	char *buffer = NULL;
	int iRet = -1;
	if (streamType == 2) {
		if (!euhatRtsp->hasResolution_) {
			if ((frame[0] & 31) == 7) {
				getResolutionFromSPS(frame, len, euhatRtsp->videoWidth_, euhatRtsp->videoHeight_);
				if (1088 == euhatRtsp->videoHeight_) {
					euhatRtsp->videoHeightOff_ = 8;
				} else if (1090 == euhatRtsp->videoHeight_) {
					euhatRtsp->videoHeightOff_ = 10;
				} else {
					euhatRtsp->videoHeightOff_ = 0;
				}
				euhatRtsp->hasResolution_ = 1;
				euhatRtsp->decoder_->updateWH(euhatRtsp->videoWidth_, euhatRtsp->videoHeight_ - euhatRtsp->videoHeightOff_);
				//pthread_cond_signal(&euhatRtsp->syncHasResolution_);

                {
                    WhMutexGuard guard(&euhatRtsp->mutexPreviewDraw_);
                    if (euhatRtsp->winPreview_) {
                        ANativeWindow_setBuffersGeometry(euhatRtsp->winPreview_,
                            euhatRtsp->videoWidth_, euhatRtsp->videoHeight_ - euhatRtsp->videoHeightOff_, WINDOW_FORMAT_RGBA_8888);
                    }
                }
			}
		}

        struct timeval tv;
        gettimeofday(&tv, NULL);
        euhatRtsp->rtspLastGetTime_ = tv.tv_sec * 1000 + tv.tv_usec / 1000;

		if (!euhatRtsp->isPlaying_)
		    return;

		if (!euhatRtsp->hasResolution_)
		    return;

		fData = (char *)euhatRtsp->h264MemPool_->alloc(4 + 4 + len);
		*(int *)fData = len + 4;
		memcpy(fData + 4, NALH, 4);
		memcpy(fData + 4 + 4, frame, len);

        if ((frame[0] & 31) == 7) {
            euhatRtsp->decoder_->updateSps(fData);
        } else if ((frame[0] & 31) == 8) {
            euhatRtsp->decoder_->updatePps(fData);
        }

        WhMutexGuard guard(&euhatRtsp->mutexDecoder_);
		euhatRtsp->listDecodeFrame_.push_back(fData);
		pthread_cond_signal(&euhatRtsp->syncDecoder_);
		if (euhatRtsp->listDecodeFrame_.size() > 100) {
		    euhatRtsp->informStatus(EUHAT_STATUS_DECODING_QUEUE_TOO_LARGE, euhatRtsp->listDecodeFrame_.size() - 100, 0);
            while (euhatRtsp->listDecodeFrame_.size() > 1) {
                char *frame = euhatRtsp->listDecodeFrame_.front();
                euhatRtsp->listDecodeFrame_.pop_front();
                euhatRtsp->h264MemPool_->dealloc(frame);
            }
		}
	}
}

int EuhatRtspOp::decodingProc()
{
    JavaVM *vm = getVM();
    JNIEnv *env;
    vm->AttachCurrentThread(&env, NULL);

    while (!isExiting_) {
        char *frame = NULL;
        {
            WhMutexGuard guard(&mutexDecoder_);
            if (listDecodeFrame_.size() > 0) {
                frame = listDecodeFrame_.front();
                listDecodeFrame_.pop_front();
            } else {
                whCondWait(&syncDecoder_, &mutexDecoder_);
            }
        }
        if (NULL == frame)
            continue;

        decoder_->decode(frame + 4, *(int *)frame);

        h264MemPool_->dealloc(frame);
    }

    vm->DetachCurrentThread();

    return 1;
}

int EuhatRtspOp::initRtspSource()
{
	int iRet = -1;

	int i = 0;
	rtsp_ = rtsp_source_new(streamCallBack, this);
	if (NULL == rtsp_) {
		return -1;
	}
	int flag = 1;
	int rtspTimeOut = 8000;
	if (rtspUseFlags_) {
	    flag = rtspFlags_;
	    rtspTimeOut = rtspTimeOut_;
	}
	iRet = rtsp_source_open(rtsp_, rtspUrl_.c_str(), flag, rtspTimeOut);
	DBG(("rtsp flag is %d.\n", flag));
	if (iRet < 0) {
	    DBG(("rtsp source open [%s], use flags %d, failed.\n", rtspUrl_.c_str(), rtspUseFlags_));
		rtsp_source_free(rtsp_);
		rtsp_ = NULL;
		return -1;
	}
	iRet = rtsp_source_play(rtsp_);
	if (iRet < 0) {
	    DBG(("rtsp source play failed.\n"));
		rtsp_source_close(rtsp_);
		rtsp_source_free(rtsp_);
		rtsp_ = NULL;
		return -1;
	}
	return 0;
}

void EuhatRtspOp::finiRtspSource()
{
	if (rtsp_)
	{
		rtsp_source_close(rtsp_);
		rtsp_source_free(rtsp_);
		rtsp_ = NULL;
	}
}

static int Table_fv1[256] = { -180, -179, -177, -176, -174, -173, -172, -170, -169, -167, -166, -165, -163, -162, -160, -159, -158, -156, -155, -153, -152, -151, -149, -148, -146, -145, -144, -142, -141, -139, -138, -137, -135, -134, -132, -131, -130, -128, -127, -125, -124, -123, -121, -120, -118, -117, -115, -114, -113, -111, -110, -108, -107, -106, -104, -103, -101, -100, -99, -97, -96, -94, -93, -92, -90, -89, -87, -86, -85, -83, -82, -80, -79, -78, -76, -75, -73, -72, -71, -69, -68, -66, -65, -64, -62, -61, -59, -58, -57, -55, -54, -52, -51, -50, -48, -47, -45, -44, -43, -41, -40, -38, -37, -36, -34, -33, -31, -30, -29, -27, -26, -24, -23, -22, -20, -19, -17, -16, -15, -13, -12, -10, -9, -8, -6, -5, -3, -2, 0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14, 15, 16, 18, 19, 21, 22, 23, 25, 26, 28, 29, 30, 32, 33, 35, 36, 37, 39, 40, 42, 43, 44, 46, 47, 49, 50, 51, 53, 54, 56, 57, 58, 60, 61, 63, 64, 65, 67, 68, 70, 71, 72, 74, 75, 77, 78, 79, 81, 82, 84, 85, 86, 88, 89, 91, 92, 93, 95, 96, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 114, 116, 117, 119, 120, 122, 123, 124, 126, 127, 129, 130, 131, 133, 134, 136, 137, 138, 140, 141, 143, 144, 145, 147, 148, 150, 151, 152, 154, 155, 157, 158, 159, 161, 162, 164, 165, 166, 168, 169, 171, 172, 173, 175, 176, 178 };
static int Table_fv2[256] = { -92, -91, -91, -90, -89, -88, -88, -87, -86, -86, -85, -84, -83, -83, -82, -81, -81, -80, -79, -78, -78, -77, -76, -76, -75, -74, -73, -73, -72, -71, -71, -70, -69, -68, -68, -67, -66, -66, -65, -64, -63, -63, -62, -61, -61, -60, -59, -58, -58, -57, -56, -56, -55, -54, -53, -53, -52, -51, -51, -50, -49, -48, -48, -47, -46, -46, -45, -44, -43, -43, -42, -41, -41, -40, -39, -38, -38, -37, -36, -36, -35, -34, -33, -33, -32, -31, -31, -30, -29, -28, -28, -27, -26, -26, -25, -24, -23, -23, -22, -21, -21, -20, -19, -18, -18, -17, -16, -16, -15, -14, -13, -13, -12, -11, -11, -10, -9, -8, -8, -7, -6, -6, -5, -4, -3, -3, -2, -1, 0, 0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 7, 8, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 17, 18, 19, 20, 20, 21, 22, 22, 23, 24, 25, 25, 26, 27, 27, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35, 36, 37, 37, 38, 39, 40, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47, 47, 48, 49, 50, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 57, 58, 59, 60, 60, 61, 62, 62, 63, 64, 65, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72, 72, 73, 74, 75, 75, 76, 77, 77, 78, 79, 80, 80, 81, 82, 82, 83, 84, 85, 85, 86, 87, 87, 88, 89, 90, 90 };
static int Table_fu1[256] = { -44, -44, -44, -43, -43, -43, -42, -42, -42, -41, -41, -41, -40, -40, -40, -39, -39, -39, -38, -38, -38, -37, -37, -37, -36, -36, -36, -35, -35, -35, -34, -34, -33, -33, -33, -32, -32, -32, -31, -31, -31, -30, -30, -30, -29, -29, -29, -28, -28, -28, -27, -27, -27, -26, -26, -26, -25, -25, -25, -24, -24, -24, -23, -23, -22, -22, -22, -21, -21, -21, -20, -20, -20, -19, -19, -19, -18, -18, -18, -17, -17, -17, -16, -16, -16, -15, -15, -15, -14, -14, -14, -13, -13, -13, -12, -12, -11, -11, -11, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -6, -6, -6, -5, -5, -5, -4, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 33, 33, 33, 34, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43 };
static int Table_fu2[256] = { -227, -226, -224, -222, -220, -219, -217, -215, -213, -212, -210, -208, -206, -204, -203, -201, -199, -197, -196, -194, -192, -190, -188, -187, -185, -183, -181, -180, -178, -176, -174, -173, -171, -169, -167, -165, -164, -162, -160, -158, -157, -155, -153, -151, -149, -148, -146, -144, -142, -141, -139, -137, -135, -134, -132, -130, -128, -126, -125, -123, -121, -119, -118, -116, -114, -112, -110, -109, -107, -105, -103, -102, -100, -98, -96, -94, -93, -91, -89, -87, -86, -84, -82, -80, -79, -77, -75, -73, -71, -70, -68, -66, -64, -63, -61, -59, -57, -55, -54, -52, -50, -48, -47, -45, -43, -41, -40, -38, -36, -34, -32, -31, -29, -27, -25, -24, -22, -20, -18, -16, -15, -13, -11, -9, -8, -6, -4, -2, 0, 1, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19, 21, 23, 24, 26, 28, 30, 31, 33, 35, 37, 39, 40, 42, 44, 46, 47, 49, 51, 53, 54, 56, 58, 60, 62, 63, 65, 67, 69, 70, 72, 74, 76, 78, 79, 81, 83, 85, 86, 88, 90, 92, 93, 95, 97, 99, 101, 102, 104, 106, 108, 109, 111, 113, 115, 117, 118, 120, 122, 124, 125, 127, 129, 131, 133, 134, 136, 138, 140, 141, 143, 145, 147, 148, 150, 152, 154, 156, 157, 159, 161, 163, 164, 166, 168, 170, 172, 173, 175, 177, 179, 180, 182, 184, 186, 187, 189, 191, 193, 195, 196, 198, 200, 202, 203, 205, 207, 209, 211, 212, 214, 216, 218, 219, 221, 223, 225 };

static bool YV122BGR32Table(unsigned char *pYUV, unsigned char *pRGB32, int width, int height)
{
	if (width < 1 || height < 1 || pYUV == NULL || pRGB32 == NULL)
		return false;
	const long len = width * height;
	unsigned char* yData = pYUV;
	unsigned char* vData = &yData[len];
	unsigned char* uData = &vData[len >> 2];

	int rgb[3];
	int yIdx, uIdx, vIdx, idx;
	int rdif, invgdif, bdif;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			yIdx = i * width + j;
			vIdx = (i >> 1) * (width >> 1) + (j >> 1);
			uIdx = vIdx;

			rdif = Table_fv1[vData[vIdx]];
			invgdif = Table_fu1[uData[uIdx]] + Table_fv2[vData[vIdx]];
			bdif = Table_fu2[uData[uIdx]];

			rgb[0] = yData[yIdx] + bdif;
			rgb[1] = yData[yIdx] - invgdif;
			rgb[2] = yData[yIdx] + rdif;

			for (int k = 0; k < 3; k++) {
				idx = (i * width + j) * 4 + k;
				if (rgb[k] >= 0 && rgb[k] <= 255)
					pRGB32[idx] = rgb[k];
				else
					pRGB32[idx] = (rgb[k] < 0) ? 0 : 255;
			}
		}
	}
	return true;
}

static void copyFrame(const char *src, char *dest, const int width, int height, const int stride_src, const int stride_dest) {
	const int h8 = height % 8;
	for (int i = 0; i < h8; i++) {
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
	}
	for (int i = 0; i < height; i += 8) {
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
		memcpy(dest, src, width);
		dest += stride_dest; src += stride_src;
	}
}

static int copyToSurface(char *rgb32, int width, int height, ANativeWindow *window)
{
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(window, &buffer, NULL) == 0) {
        const char *src = rgb32;
        const int src_w = width * 4;
        const int src_step = width * 4;
        char *dest = (char *)buffer.bits;
        const int dest_w = buffer.width * 4;
        const int dest_step = buffer.stride * 4;
        const int w = src_w < dest_w ? src_w : dest_w;
        const int h = height < buffer.height ? height : buffer.height;
        copyFrame(src, dest, w, h, src_step, dest_step);
        ANativeWindow_unlockAndPost(window);
    }
	return 1;
}

static void yv122Nv21(char *yv12, char *nv21, int width, int height)
{
    int frameSize = width * height;
    memcpy(nv21, yv12, frameSize);
    nv21 += frameSize;
    yv12 += frameSize;
    int halfWidth = width / 2;
    int halfHeight = height / 2;
    int quadFrame = halfWidth * halfHeight;
    for (int i = 0; i < halfHeight; i++) {
        for (int j = 0; j < halfWidth; j++) {
            *nv21++ = *(yv12 + i * halfWidth + j);
            *nv21++ = *(yv12 + quadFrame + i * halfWidth + j);
        }
    }
}

int EuhatRtspOp::previewProc()
{
    JavaVM *vm = getVM();
    JNIEnv *env;
    vm->AttachCurrentThread(&env, NULL);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int lastStampMs = tv.tv_usec / 1000;

    while (!isTimerThreadExiting_) {
        int intervalRuleMs = 1000 / displayFps_;

        char *frame = NULL;
        {
            WhMutexGuard guard(&mutexPreview_);
            if (listPreviewFrame_.size() > 0) {
                frame = listPreviewFrame_.front();
                listPreviewFrame_.pop_front();
            } else {
                whCondWait(&syncPreview_, &mutexPreview_);
            }
        }
        if (NULL == frame)
            continue;

        int width = *(int *)frame;
        int height = *(int *)(frame + 4);
        int sizeYUV = *(int *)(frame + 8);
        if (winPreviewNeedDraw_) {
            char *rgb32Buf = (char *)rgbMemPool_->alloc(width * height * 4);
            YV122BGR32Table((unsigned char *)frame + 4 * 3, (unsigned char *)rgb32Buf, width, height);
            {
                WhMutexGuard guard(&mutexPreviewDraw_);
                if (NULL != winPreview_) {
                    copyToSurface(rgb32Buf, width, height, winPreview_);
                }
            }
            rgbMemPool_->dealloc(rgb32Buf);
        }

        if (pixelFormat_ == 2) {
            char *frameTmp = (char *)yuvMemPool_->alloc(sizeYUV + 4 * 3);
            *(int *)frameTmp = width;
            *(int *)(frameTmp + 4) = height;
            *(int *)(frameTmp + 8) = sizeYUV;
            yv122Nv21(frame + 4 * 3, frameTmp + 4 * 3, width, height);
            yuvMemPool_->dealloc(frame);
            frame = frameTmp;
        }

        {
            WhMutexGuard guard(&mutexFrameCallback_);
            listFrameCallback_.push_back(frame);
            pthread_cond_signal(&syncFrameCallback_);
            while (listFrameCallback_.size() > 1) {
                char *frameOld = listFrameCallback_.front();
                listFrameCallback_.pop_front();
                yuvMemPool_->dealloc(frameOld);
            }
		}

        gettimeofday(&tv, NULL);
        int curStampMs = tv.tv_usec / 1000;
        int intervalMs = curStampMs - lastStampMs;
        if (intervalMs < 0)
            intervalMs += 1000;
        int toSleepMs = intervalRuleMs - intervalMs;
        if (toSleepMs > 0)
            whSleepMs(toSleepMs);
        lastStampMs = curStampMs;
    }

    vm->DetachCurrentThread();

    return 1;
}

static int decoderCallback(char *outBuf, void *context)
{
    EuhatRtspOp *euhatRtsp = (EuhatRtspOp *)context;
    return euhatRtsp->decodingCb(outBuf);
}

int EuhatRtspOp::connect2(const char *url)
{
    DBG(("euhat rtsp op connect in.\n"));

    int iRet = -1;
    if (isConnected_) {
        return -1;
    }

    decoder_->init(decoderCallback, this);
    winPreviewNeedDraw_ = decoder_->updateSurface(winPreview_);

    rtspUrl_ = url;

    iRet = initRtspSource();
    if (iRet < 0) {
        DBG(("euhat rtsp init rtsp source failed.\n"));
        informStatus(EUHAT_STATUS_RTSP_CONNECT_FAILED, 0, 0);
        decoder_->fini();
        return -2;
    }
/*
    {
        WhMutexGuard guard(&mutexHasResolution_);
        whCondWait(&syncHasResolution_, &mutexHasResolution_);
    }

    DBG(("euhat rtsp get width %d, height %d.\n", videoWidth_, videoHeight_));
	if (!hasResolution_ || videoWidth_ == 0 || videoHeight_ == 0) {
		finiRtspSource();
		return -3;
	}
*/
    startWorkThread();

    struct timeval tv;
    gettimeofday(&tv, NULL);
    rtspLastGetTime_ = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    isConnected_ = 1;

    return 0;
}

int EuhatRtspOp::decodingCb(char *yuv)
{
    WhMutexGuard guard(&mutexPreview_);

    if (!hasFirstFrame_) {
        hasFirstFrame_ = 1;
        while (listPreviewFrame_.size() > 0) {
            char *frame = listPreviewFrame_.front();
            listPreviewFrame_.pop_front();
            yuvMemPool_->dealloc(frame);
        }
    }

    if (listPreviewFrame_.size() > displayBufCount_) {
        informStatus(EUHAT_STATUS_DISPLAY_QUEUE_TOO_LARGE, listPreviewFrame_.size() - displayBufCount_, 0);
        while (listPreviewFrame_.size() > 0) {
            char *frame = listPreviewFrame_.front();
            listPreviewFrame_.pop_front();
            yuvMemPool_->dealloc(frame);
        }
    }

    listPreviewFrame_.push_back(yuv);
    pthread_cond_signal(&syncPreview_);

    return 1;
}

int EuhatRtspOp::close2()
{
    if (!isConnected_)
        return -1;

    isConnected_ = 0;

    stopWorkThread();

    finiRtspSource();

    decoder_->fini();

    hasResolution_ = 0;
    hasFirstFrame_ = 0;
    videoWidth_ = videoHeight_ = 0;

    return 0;
}

int EuhatRtspOp::startPreview()
{
    DBG(("euhat rtsp start preview.\n"));
    isPlaying_ = 1;
    return 0;
}

int EuhatRtspOp::stopPreview()
{
    DBG(("euhat rtsp stop preview.\n"));
    isPlaying_ = 0;
    return 0;
}

int EuhatRtspOp::setPreviewDisplay(ANativeWindow *nativeWin)
{
    WhMutexGuard guard(&mutexPreviewDraw_);

    if (winPreview_ != nativeWin) {
        if (winPreview_)
            ANativeWindow_release(winPreview_);
        winPreview_ = nativeWin;
        if (winPreview_) {
            ANativeWindow_setBuffersGeometry(winPreview_,
                1920, 1080, WINDOW_FORMAT_RGBA_8888);
        }
    }

    return 0;
}

int EuhatRtspOp::setStatusCallback(JNIEnv *env, jobject statusCallbackObj)
{
    WhMutexGuard guard(&mutexStatusCallbackCall_);

    if (!env->IsSameObject(objStatusCallback_, statusCallbackObj)) {
        DBG(("register status callback.\n"));
        methodsStatusCallback_.onStatus = NULL;
        if (objStatusCallback_) {
            env->DeleteGlobalRef(objStatusCallback_);
        }
        objStatusCallback_ = statusCallbackObj;
        if (statusCallbackObj) {
            jclass clazz = env->GetObjectClass(statusCallbackObj);
            if (clazz) {
                methodsStatusCallback_.onStatus = env->GetMethodID(clazz,
                    "onStatus", "(III)V");
            } else {
                DBG(("failed to get IStatusCallback object class.\n"));
                env->DeleteGlobalRef(statusCallbackObj);
                objStatusCallback_ = statusCallbackObj = NULL;
                return -1;
            }
            env->ExceptionClear();
            if (!methodsStatusCallback_.onStatus) {
                DBG(("can't find IStatusCallback#onStatus.\n"));
                env->DeleteGlobalRef(statusCallbackObj);
                objStatusCallback_ = statusCallbackObj = NULL;
            }
        }
    } else {
        env->DeleteGlobalRef(statusCallbackObj);
    }
    return 0;
}

int EuhatRtspOp::setFrameCallback(JNIEnv *env, jobject frameCallbackObj, int pixelFormat)
{
    WhMutexGuard guard(&mutexFrameCallbackCall_);

    pixelFormat_ = pixelFormat;

    if (!env->IsSameObject(objFrameCallback_, frameCallbackObj)) {
        DBG(("register frame callback.\n"));
        methodsFrameCallback_.onFrame = NULL;
        if (objFrameCallback_) {
            env->DeleteGlobalRef(objFrameCallback_);
        }
        objFrameCallback_ = frameCallbackObj;
        if (frameCallbackObj) {
            jclass clazz = env->GetObjectClass(frameCallbackObj);
            if (clazz) {
                methodsFrameCallback_.onFrame = env->GetMethodID(clazz,
                    "onFrame",	"(Ljava/nio/ByteBuffer;II)V");
            } else {
                DBG(("failed to get IFrameCallback object class.\n"));
                env->DeleteGlobalRef(frameCallbackObj);
                objFrameCallback_ = frameCallbackObj = NULL;
                return -1;
            }
            env->ExceptionClear();
            if (!methodsFrameCallback_.onFrame) {
                DBG(("can't find IFrameCallback#onFrame.\n"));
                env->DeleteGlobalRef(frameCallbackObj);
                objFrameCallback_ = frameCallbackObj = NULL;
            }
        }
    } else {
        env->DeleteGlobalRef(frameCallbackObj);
    }
    return 0;
}

static jlong setField_long(JNIEnv *env, jobject java_obj, const char *field_name, jlong val) {
	jclass clazz = env->GetObjectClass(java_obj);
	jfieldID field = env->GetFieldID(clazz, field_name, "J");
	if (field)
		env->SetLongField(java_obj, field, val);
	else {
		DBG(("setField_long, field '%s' not found.\n", field_name));
	}
#ifdef ANDROID_NDK
	env->DeleteLocalRef(clazz);
#endif
	return val;
}

int EuhatRtspOp::statusCallbackProc()
{
    JavaVM *vm = getVM();
    JNIEnv *env;
    vm->AttachCurrentThread(&env, NULL);

    while (!isTimerThreadExiting_) {
        StatusCallbackOnStatusParams frame;
        frame.type = 0;
        {
            WhMutexGuard guard(&mutexStatusCallback_);
            if (listStatusCallbackOnStatus_.size() > 0) {
                frame = listStatusCallbackOnStatus_.front();
                listStatusCallbackOnStatus_.pop_front();
            }
        }
        if (0 == frame.type) {
            whSleepMs(10);
            continue;
        }
        {
            WhMutexGuard guard(&mutexStatusCallbackCall_);
            if (objStatusCallback_ != NULL && methodsStatusCallback_.onStatus != NULL) {
                env->CallVoidMethod(objStatusCallback_, methodsStatusCallback_.onStatus, frame.type, frame.param0, frame.param1);
                env->ExceptionClear();
            } else {
                DBG(("status callback not registered.\n"));
            }
        }
    }

    vm->DetachCurrentThread();

    return 1;
}

static jlong nativeCreate(JNIEnv *env, jobject thiz) {
	EuhatRtspOp *euhatRtsp = new EuhatRtspOp();
	setField_long(env, thiz, "mNativePtr", reinterpret_cast<jlong>(euhatRtsp));
	return reinterpret_cast<jlong>(euhatRtsp);
}

static void nativeDestroy(JNIEnv *env, jobject thiz, jlong rtspObj) {
	setField_long(env, thiz, "mNativePtr", 0);
	EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	delete euhatRtsp;
}

int EuhatRtspOp::frameCallbackProc()
{
    JavaVM *vm = getVM();
    JNIEnv *env;
    vm->AttachCurrentThread(&env, NULL);

    while (!isTimerThreadExiting_) {
        char *frame = NULL;
        {
            WhMutexGuard guard(&mutexFrameCallback_);
            if (listFrameCallback_.size() > 0) {
                frame = listFrameCallback_.front();
                listFrameCallback_.pop_front();
            } else {
                whCondWait(&syncFrameCallback_, &mutexFrameCallback_);
            }
        }
        if (NULL == frame)
            continue;

        {
            WhMutexGuard guard(&mutexFrameCallbackCall_);
            if (objFrameCallback_ != NULL && methodsFrameCallback_.onFrame != NULL) {
                int width = *(int *)frame;
                int height = *(int *)(frame + 4);
                int sizeYUV = *(int *)(frame + 8);
                jobject buf = env->NewDirectByteBuffer(frame + 4 * 3, sizeYUV);
                env->CallVoidMethod(objFrameCallback_, methodsFrameCallback_.onFrame, buf, width, height);
                env->ExceptionClear();
                env->DeleteLocalRef(buf);
            }
        }
        yuvMemPool_->dealloc(frame);
    }

    vm->DetachCurrentThread();

    return 1;
}

int EuhatRtspOp::informStatus(int type, int param0, int param1)
{
    WhMutexGuard guard(&mutexStatusCallback_);
    StatusCallbackOnStatusParams frame;
    frame.type = type;
    frame.param0 = param0;
    frame.param1 = param1;
    listStatusCallbackOnStatus_.push_back(frame);
    while (listStatusCallbackOnStatus_.size() > 1000) {
        listStatusCallbackOnStatus_.pop_front();
    }
    return 1;
}

int EuhatRtspOp::connect(const char *url, int rtspTimeOut, int rtspFlag, int reconnectTimeOut, int displayFps, int displayBufCount)
{
    rtspUseFlags_ = 1;
    rtspFlags_ = rtspFlag;
    rtspTimeOut_ = rtspTimeOut;
    reconnectTimeOut_ = reconnectTimeOut;
    displayFps_ = displayFps;
    displayBufCount_ = displayBufCount;
    postMessageStr(1, url);
    DBG(("euhat rtsp connect, reconnect time out %d, display fps %d, buf count %d.\n", reconnectTimeOut_, displayFps_, displayBufCount_));
    return 0;
}

int EuhatRtspOp::close()
{
    DBG(("euhat rtsp op send close.\n"));
    postMessageInt(2, 0);
    return 0;
}

int EuhatRtspOp::onTimer()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long nowTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    unsigned long interval;
    if (nowTime < rtspLastGetTime_) {
        interval = ((unsigned long)-1) - rtspLastGetTime_ + nowTime;
    } else {
        interval = nowTime - rtspLastGetTime_;
    }
    if (interval > reconnectTimeOut_) {
        rtspLastGetTime_ = nowTime;

        {
            WhMutexGuard guard(&mutexMessage_);
            for (list<MessageParams>::iterator it = listMessage_.begin(); it != listMessage_.end(); it++) {
                if (it->type == 4) {
                    return 1;
                }
            }
        }

        DBG(("on timer post reconnect message.\n"));

        postMessageInt(4, 0);
    }
    return 1;
}

static jint nativeConnect(JNIEnv *env, jobject thiz, jlong rtspObj, jstring jstrUrl, jint rtspTimeOut, jint rtspFlag, jint reconnectTimeOut, jint displayFps, jint displayBufCount) {
	int result = JNI_ERR;
	EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	const char *url = env->GetStringUTFChars(jstrUrl, JNI_FALSE);
	result = euhatRtsp->connect(url, rtspTimeOut, rtspFlag, reconnectTimeOut, displayFps, displayBufCount);
	env->ReleaseStringUTFChars(jstrUrl, url);
	return result;
}

static jint nativeClose(JNIEnv *env, jobject thiz, jlong rtspObj) {
	int result = JNI_ERR;
	EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	result = euhatRtsp->close();
	return result;
}

static jint nativeStartPreview(JNIEnv *env, jobject thiz, jlong rtspObj) {
    EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	return euhatRtsp->startPreview();
}

static jint nativeStopPreview(JNIEnv *env, jobject thiz, jlong rtspObj) {
    EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	return euhatRtsp->stopPreview();
}

int EuhatRtspOp::timerProc()
{
    while (!isTimerThreadExiting_) {
        whSleepMs(5);
        if (needReconnecting_) {
            {
                WhMutexGuard guard(&mutexMessage_);
                int hasSent = 0;
                for (list<MessageParams>::iterator it = listMessage_.begin(); it != listMessage_.end(); it++) {
                    if (it->type == 3) {
                        hasSent = 1;
                        break;
                    }
                }
                if (hasSent) {
                    continue;
                }
            }
            postMessageInt(3, 0);
        }
    }
    return 1;
}

static jint nativeSetPreviewDisplay(JNIEnv *env, jobject thiz, jlong rtspObj, jobject jSurface) {
	jint result = JNI_ERR;
	EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	ANativeWindow *previewWindow = jSurface ? ANativeWindow_fromSurface(env, jSurface) : NULL;
	result = euhatRtsp->setPreviewDisplay(previewWindow);
	return result;
}

static jint nativeSetStatusCallback(JNIEnv *env, jobject thiz,
    jlong rtspObj, jobject jIStatusCallback) {

    jint result = JNI_ERR;
    EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
    jobject statusCallbackObj = env->NewGlobalRef(jIStatusCallback);
    result = euhatRtsp->setStatusCallback(env, statusCallbackObj);
    return result;
}

static jint nativeSetFrameCallback(JNIEnv *env, jobject thiz,
	jlong rtspObj, jobject jIFrameCallback, jint pixelFormat) {

	jint result = JNI_ERR;
	EuhatRtspOp *euhatRtsp = reinterpret_cast<EuhatRtspOp *>(rtspObj);
	jobject frameCallbackObj = env->NewGlobalRef(jIFrameCallback);
	result = euhatRtsp->setFrameCallback(env, frameCallbackObj, pixelFormat);
    return result;
}

int EuhatRtspOp::messageProc()
{
    JavaVM *vm = getVM();
    JNIEnv *env;
    vm->AttachCurrentThread(&env, NULL);

    while (1) {
        MessageParams msg;
        {
            WhMutexGuard guard(&mutexMessage_);
            if (listMessage_.size() > 0) {
                msg = listMessage_.front();
                listMessage_.pop_front();
            } else {
                msg.type = -1;
            }
        }
        if (msg.type == -1) {
            whSleepMs(10);
            continue;
        }
        if (msg.type == 1) {
            DBG(("manually connect [%s].\n", msg.str.c_str()));
            connect3(msg.str.c_str());
        } else if (msg.type == 2) {
            DBG(("manually close.\n"));
            close3();
        } else if (msg.type == 3) {
            onTimer();
        } else if (msg.type == 4) {
            informStatus(EUHAT_STATUS_RTSP_RECONNECTING, 0, 0);
            string url = rtspUrl_;
            close2();
            connect2(url.c_str());
        } else if (msg.type == 0) {
            close2();
            break;
        }
    }

    vm->DetachCurrentThread();

    return 1;
}

static JNINativeMethod methods[] = {
	{ "nativeCreate",					"()J", (void *)nativeCreate },
	{ "nativeDestroy",					"(J)V", (void *)nativeDestroy },
	{ "nativeConnect",					"(JLjava/lang/String;IIIII)I", (void *)nativeConnect },
	{ "nativeClose",					"(J)I", (void *)nativeClose },
	{ "nativeStartPreview",				"(J)I", (void *)nativeStartPreview },
	{ "nativeStopPreview",				"(J)I", (void *)nativeStopPreview },
	{ "nativeSetPreviewDisplay",		"(JLandroid/view/Surface;)I", (void *)nativeSetPreviewDisplay },
	{ "nativeSetStatusCallback",        "(JLcom/euhat/rtsp/euhatrtsplib/IStatusCallback;)I", (void *)nativeSetStatusCallback },
	{ "nativeSetFrameCallback",			"(JLcom/euhat/rtsp/euhatrtsplib/IFrameCallback;I)I", (void *)nativeSetFrameCallback },
};

#define	NUM_ARRAY_ELEMENTS(p) ((int) sizeof(p) / sizeof(p[0]))

jint registerNativeMethods(JNIEnv* env, const char *class_name, JNINativeMethod *methods, int num_methods) {
	int result = 0;

	jclass clazz = env->FindClass(class_name);
	if (clazz) {
		int result = env->RegisterNatives(clazz, methods, num_methods);
		if (result < 0) {
			DBG(("registerNativeMethods failed, class is %s.\n", class_name));
		}
	} else {
		DBG(("registerNativeMethods: class '%s' not found.\n", class_name));
	}
	return result;
}

int registerEuhatRtsp(JNIEnv *env) {
	if (registerNativeMethods(env,
		"com/euhat/rtsp/euhatrtsplib/EuhatRtspPlayer",
		methods, NUM_ARRAY_ELEMENTS(methods)) < 0) {
		return -1;
	}
    return 0;
}

static JavaVM *savedVm;

void setVM(JavaVM *vm) {
	savedVm = vm;
}

JavaVM *getVM() {
	return savedVm;
}

JNIEnv *getEnv() {
    JNIEnv *env = NULL;
    if (savedVm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    	env = NULL;
    }
    return env;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
	DBG(("I'm loaded==========================\n"));

    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    int result = registerEuhatRtsp(env);
	setVM(vm);

    return JNI_VERSION_1_6;
}
