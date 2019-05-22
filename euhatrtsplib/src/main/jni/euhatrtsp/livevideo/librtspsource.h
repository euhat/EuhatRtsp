#ifndef H_AV_STREAM_LIBRTSPSOURCE
#define H_AV_STREAM_LIBRTSPSOURCE

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO 1
#define VIDEO 2

#define RTP_OVER_RTSP 1
#define RTP_RAW_FRAME (1 << 1)
#define RTSP_ASYNC (1 << 2)

#ifndef _WIN32
typedef long long __int64;
#endif

typedef struct _FrameInfo {
	__int64 pts;
	__int64 dts;
	__int64 duration;
}FrameInfo, *FrameInfoPtr;

typedef struct _TrackInfo {
	int streamType;
	unsigned char payloadFormat;
	int channels; // for audio
	int timeFreq;
	char *codecName;
	char *spsparam;
	char *fmtp;
	struct _TrackInfo *next;
}TrackInfo, *TrackInfoPtr;

typedef void (*StreamCallbackFunc)(int streamType, unsigned char* frame, unsigned len, void* context, FrameInfo *info);

typedef void (*DisConnectNotifyFunc)();

void* rtsp_source_new(StreamCallbackFunc callback, /* DisConnectNotifyFunc disconnectcallback, */ void *context);

int rtsp_source_open(void* rtsp, const char* url, int flags, int timeout);


int rtsp_source_is_opened(void* rtsp);

int rtsp_source_is_play(void* rtsp);

int rtsp_source_play(void* rtsp);

void rtsp_source_close(void* rtsp);

void rtsp_source_free(void* rtsp);

int rtsp_source_get_resolution(void* rtsp,int &width,int &height);

char *rtsp_source_get_sdp(void *rtsp);

void rtsp_source_free_sdp(char *sdp);

void rtsp_source_free_tracks(TrackInfoPtr tracks);

int rtsp_source_get_tracks(void* rtsp, TrackInfoPtr *tracks);

#ifdef __cplusplus
}
#endif


#endif
