#include "DecoderHard.h"
#include <media/NdkMediaCodec.h>
#include "CommonOp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include "RtspOp.h"
#include "CommonOp.h"

#define MIN(_a,_b) ((_a) > (_b) ? (_b) : (_a))

static int test() {
    char *key = (char *)"ro.build.version.sdk";
    //char *key = (char *)"ro.build.version.release";
    char value[1024] = {0};
    int ret = __system_property_get(key, value);
    if (ret <= 0 ) {
        DBG(("get prop value failed.\n"));
        return 0;
    }
    DBG(("ro.build.id is [%s]\n", value));

    return 0;
}

typedef AMediaCodec *(*AMediaCodec_createCodecByName_t)(const char *name);
typedef AMediaCodec *(*AMediaCodec_createDecoderByType_t)(const char *mime_type);
typedef media_status_t (*AMediaCodec_stop_t)(AMediaCodec *);
typedef media_status_t (*AMediaCodec_delete_t)(AMediaCodec *);
typedef media_status_t (*AMediaCodec_configure_t)(
            AMediaCodec *,
            const AMediaFormat *format,
            ANativeWindow *surface,
            AMediaCrypto *crypto,
            uint32_t flags);
typedef media_status_t (*AMediaCodec_start_t)(AMediaCodec*);
typedef ssize_t (*AMediaCodec_dequeueInputBuffer_t)(AMediaCodec *, int64_t timeoutUs);
typedef uint8_t *(*AMediaCodec_getInputBuffer_t)(AMediaCodec *, size_t idx, size_t *out_size);
typedef media_status_t (*AMediaCodec_queueInputBuffer_t)(AMediaCodec *,
            size_t idx, off_t offset, size_t size, uint64_t time, uint32_t flags);
typedef ssize_t (*AMediaCodec_dequeueOutputBuffer_t)(AMediaCodec *, AMediaCodecBufferInfo *info, int64_t timeoutUs);
typedef uint8_t *(*AMediaCodec_getOutputBuffer_t)(AMediaCodec *, size_t idx, size_t *out_size);
typedef media_status_t (*AMediaCodec_releaseOutputBuffer_t)(AMediaCodec *, size_t idx, bool render);
typedef AMediaFormat *(*AMediaCodec_getOutputFormat_t)(AMediaCodec *);

typedef AMediaFormat *(*AMediaFormat_new_t)();
typedef media_status_t (*AMediaFormat_delete_t)(AMediaFormat *);
typedef bool (*AMediaFormat_getInt32_t)(AMediaFormat *, const char *name, int32_t *out);
typedef void (*AMediaFormat_setInt32_t)(AMediaFormat *, const char *name, int32_t value);
typedef void (*AMediaFormat_setString_t)(AMediaFormat *, const char *name, const char *value);
typedef void (*AMediaFormat_setBuffer_t)(AMediaFormat *, const char *name, void *data, size_t size);

#define API_LOOP(_m) \
    _m(AMediaCodec_createCodecByName) \
    _m(AMediaCodec_createDecoderByType) \
    _m(AMediaCodec_stop) \
    _m(AMediaCodec_delete) \
    _m(AMediaCodec_configure) \
    _m(AMediaCodec_start) \
    _m(AMediaCodec_dequeueInputBuffer) \
    _m(AMediaCodec_getInputBuffer) \
    _m(AMediaCodec_queueInputBuffer) \
    _m(AMediaCodec_dequeueOutputBuffer) \
    _m(AMediaCodec_getOutputBuffer) \
    _m(AMediaCodec_releaseOutputBuffer) \
    _m(AMediaCodec_getOutputFormat) \
    _m(AMediaFormat_new) \
    _m(AMediaFormat_delete) \
    _m(AMediaFormat_getInt32) \
    _m(AMediaFormat_setInt32) \
    _m(AMediaFormat_setString) \
    _m(AMediaFormat_setBuffer)

#define API_DEF(_name) _name##_t _name##_;
#define API_(_name) gApi._name##_
#define API_LOAD(_name) API_(_name) = (_name##_t)dlsym(handle, #_name);

struct CodecApi
{
    API_LOOP(API_DEF)
};

static const char *gAMEDIAFORMAT_KEY_WIDTH = "width";
static const char *gAMEDIAFORMAT_KEY_HEIGHT = "height";
static const char *gAMEDIAFORMAT_KEY_FRAME_RATE = "frame-rate";

static CodecApi gApi;
static int gIsApiLoaded = 0;

int EuhatDecoderHard::canWork()
{
    if (gIsApiLoaded)
        return 1;

    void *handle = dlopen("libmediandk.so", RTLD_LAZY);
    if (NULL == handle) {
        return 0;
    }

    API_LOOP(API_LOAD)

    gIsApiLoaded = 1;
/*  char* szError = dlerror();
    dlclose(handle);
*/
    return 1;
}

EuhatDecoderHard::EuhatDecoderHard()
{
//  test();
    mediaCodec_ = NULL;
    spsChanged_ = 0;
    ppsChanged_ = 0;
    sps_ = pps_ = NULL;
    surfaceChanged_ = 0;
    isCodecInited_ = 0;
}

EuhatDecoderHard::~EuhatDecoderHard()
{
    if (isCodecInited_) {
        API_(AMediaCodec_stop)(mediaCodec_);
    }
    API_(AMediaCodec_delete)(mediaCodec_);
    mediaCodec_ = NULL;
}

int EuhatDecoderHard::init(EuhatDecoderCallback callback, void *context)
{
    callback_ = callback;
    context_ = context;
    surface_ = NULL;

    if (NULL == mediaCodec_) {
#if 1
        const char *mine = "video/avc";
        mediaCodec_ = API_(AMediaCodec_createDecoderByType)(mine);
        if (NULL == mediaCodec_) {
            DBG(("avc hard decoder is not supported.\n"));
            return 0;
        }
#else
        // const char *name = "OMX.qcom.video.decoder.avc";
        const char *name = "OMX.SEC.avc.sw.dec";
        // const char *name = "OMX.google.h264.decoder";
        mediaCodec_ = API_(AMediaCodec_createCodecByName)(name);
        if (NULL == mediaCodec_) {
            DBG(("hard decoder [%s] is not supported.\n", name));
            return 0;
        } else {
            DBG(("hard decoder [%s] is in work.\n", name));
        }
#endif
    }

    DBG(("codec is %p\n", mediaCodec_));

    return 1;
}

int EuhatDecoderHard::fini()
{
    spsChanged_ = 0;
    ppsChanged_ = 0;
    free(sps_); sps_ = NULL;
    free(pps_); pps_ = NULL;

    return 1;
}

int EuhatDecoderHard::updateSps(const char *sps)
{
    spsChanged_ = 1;
    free(sps_);
    sps_ = (char *)malloc((*(int *)sps) + 4);
    memcpy(sps_, sps, (*(int *)sps) + 4);

    updateSpsAndPps();
    return 1;
}

int EuhatDecoderHard::updatePps(const char *pps)
{
    ppsChanged_ = 1;
    free(pps_);
    pps_ = (char *)malloc((*(int *)pps) + 4);
    memcpy(pps_, pps, (*(int *)pps) + 4);

    updateSpsAndPps();
    return 1;
}

int EuhatDecoderHard::updateWH(int width, int height)
{
    width_ = width;
    height_ = height;
    return 1;
}

int EuhatDecoderHard::updateSurface(void *surface)
{
#if 1
    surface_ = NULL;
    return 1;
#endif
    if (surface_ != surface) {
        surfaceChanged_ = 1;
    }
    surface_ = surface;
    return 0;
}

int EuhatDecoderHard::updateSpsAndPps()
{
    if (spsChanged_ && !isCodecInited_) {
        media_status_t status;
        const char *mine = "video/avc";
        AMediaFormat *videoFormat = API_(AMediaFormat_new)();
        API_(AMediaFormat_setString)(videoFormat, "mime", mine);
        API_(AMediaFormat_setInt32)(videoFormat, gAMEDIAFORMAT_KEY_WIDTH, width_);
        API_(AMediaFormat_setInt32)(videoFormat, gAMEDIAFORMAT_KEY_HEIGHT, height_);
        API_(AMediaFormat_setInt32)(videoFormat, gAMEDIAFORMAT_KEY_FRAME_RATE, 25);
//      API_(AMediaFormat_setInt32)(videoFormat, "color-format", 19);
        API_(AMediaFormat_setBuffer)(videoFormat, "csd-0", sps_ + 4, *(int *)sps_);
//      API_(AMediaFormat_setBuffer)(videoFormat, "csd-1", pps_ + 4, *(int *)pps_);
        DBG(("codec %p, format %p, surface %p\n", mediaCodec_, videoFormat, surface_));
        status = API_(AMediaCodec_configure)(mediaCodec_, videoFormat, (ANativeWindow *)surface_, NULL, 0);
        API_(AMediaFormat_delete)(videoFormat);
        if (status != AMEDIA_OK) {
            DBG(("media codec configured failed, %d.\n", status));
            return 0;
        }

        colorFormat_ = 0;
        oriHeight_ = height_;
        stride_ = width_;
        sliceHeight_ = height_;
        cropTop_ = -1;
        cropBottom_ = -1;
        cropLeft_ = -1;
        cropRight_ = -1;

        status = API_(AMediaCodec_start)(mediaCodec_);
        if (status != AMEDIA_OK)
        {
            DBG(("media codec started failed, %d.\n", status));
            return 0;
        }
        DBG(("codec configured.\n"));
        isCodecInited_ = 1;
    }
    return 1;
}

static void nv212Yv12(char *nv21, char *yv12, int width, int height)
{
    int frameSize = width * height;
    memcpy(yv12, nv21, frameSize);
    nv21 += frameSize;
    yv12 += frameSize;
    int halfWidth = width / 2;
    int halfHeight = height / 2;
    int quadFrame = halfWidth * halfHeight;
    for (int i = 0; i < halfHeight; i++) {
        for (int j = 0; j < halfWidth; j++) {
            *(yv12 + i * halfWidth + j) = *nv21++;
            *(yv12 + quadFrame + i * halfWidth + j) = *nv21++;
        }
    }
}

int EuhatDecoderHard::decode(char *frame, int frameLen)
{
    if (!isCodecInited_)
        return 0;

    ssize_t bufIdx;
    int times = 10;
    do {
        bufIdx = API_(AMediaCodec_dequeueInputBuffer)(mediaCodec_, 2000 * 100);
        if (bufIdx >= 0) {
            size_t outSize;
            uint8_t *inputBuf = API_(AMediaCodec_getInputBuffer)(mediaCodec_, bufIdx, &outSize);
            if (inputBuf != nullptr && frameLen <= outSize) {
                memcpy(inputBuf, frame, frameLen);
                media_status_t status = API_(AMediaCodec_queueInputBuffer)(mediaCodec_, bufIdx, 0, frameLen, 2000 /* pts */, 0);
            } else {
                DBG(("inputBuf %p, outSize %d, frameLen %d.\n", inputBuf, outSize, frameLen));
            }
            break;
        } else {
            whSleepMs(10);
            // DBG(("hard codec dequeue input buffer failed.\n"));
        }
    } while (--times > 0);

    if (times <= 0) {
        DBG(("hard codec dequeue input buffer failed still.\n"));
    }

    AMediaCodecBufferInfo info;
    ssize_t outBufIdx = API_(AMediaCodec_dequeueOutputBuffer)(mediaCodec_, &info, 2000);
    if (outBufIdx >= 0) {
        size_t outSize;
        uint8_t *outputBuf = API_(AMediaCodec_getOutputBuffer)(mediaCodec_, outBufIdx, &outSize);
        if (outputBuf != nullptr) {

            int sizeYUV = width_ * height_ * 3 / 2;
            char *yuv = (char *)outputMemPool_->alloc(sizeYUV + 4 * 3);
            *(int *)yuv = width_;
            *(int *)(yuv + 4) = height_;
            *(int *)(yuv + 8) = sizeYUV;
            char *dst = yuv + 4 * 3;

            /*
            DBG(("width %d, height %d, oriHeight %d, format %d, stride %d, sliceHeight %d, outSize %d, cropTop %d, cropBottom %d, cropLeft %d, cropRight %d.\n",
                width_, height_, oriHeight_, colorFormat_, stride_, sliceHeight_, outSize, cropTop_, cropBottom_, cropLeft_, cropRight_));
            */

            if (colorFormat_ != 21) {
                uint8_t *src = outputBuf + info.offset;
                memcpy(dst, src, width_ * height_);
                src += sliceHeight_ * stride_;
                dst += width_ * height_;
                memcpy(dst, src, width_ * height_ / 4);
                src += sliceHeight_ * stride_ / 4;
                dst += width_ * height_ / 4;
                memcpy(dst, src, width_ * height_ / 4);
            } else {
                uint8_t *src = outputBuf + info.offset;
                memcpy(dst, src, width_ * height_);
                src += sliceHeight_ * stride_;
                dst += width_ * height_;
                memcpy(dst, src, width_ * height_ / 2);

                char *yuvYv12 = (char *)outputMemPool_->alloc(sizeYUV + 4 * 3);
                *(int *)yuvYv12 = width_;
                *(int *)(yuvYv12 + 4) = height_;
                *(int *)(yuvYv12 + 8) = sizeYUV;
                nv212Yv12(yuv + 4 * 3, yuvYv12 + 4 * 3, width_, height_);
                outputMemPool_->dealloc(yuv);
                yuv = yuvYv12;
            }

            callback_(yuv, context_);

            API_(AMediaCodec_releaseOutputBuffer)(mediaCodec_, outBufIdx, info.size != 0);
            return 1;
        }
    } else {
        switch (outBufIdx) {
            case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
            {
                AMediaFormat *format = API_(AMediaCodec_getOutputFormat)(mediaCodec_);
                // API_(AMediaFormat_getInt32)(format, "width", &width_);
                API_(AMediaFormat_getInt32)(format, "height", &oriHeight_);
                API_(AMediaFormat_getInt32)(format, "color-format", &colorFormat_); // 21 COLOR_FormatYUV420SemiPlanar, AV_PIX_FMT_NV12
                API_(AMediaFormat_getInt32)(format, "stride", &stride_);
                API_(AMediaFormat_getInt32)(format, "slice-height", &sliceHeight_);
                API_(AMediaFormat_getInt32)(format, "crop-top", &cropTop_);
                API_(AMediaFormat_getInt32)(format, "crop-bottom", &cropBottom_);
                API_(AMediaFormat_getInt32)(format, "crop-left", &cropLeft_);
                API_(AMediaFormat_getInt32)(format, "crop-right", &cropRight_);
                API_(AMediaFormat_delete)(format);

                stride_ = stride_ > 0? stride_ : width_;
                sliceHeight_ = sliceHeight_ > 0? sliceHeight_ : height_;

                return 0;
            }
            case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                break;
            default:
                break;
        }
    }
    return 1;
}