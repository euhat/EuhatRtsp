#include "RtspOp.h"
#include "DecoderSoft.h"
#include "CommonOp.h"

EuhatDecoderSoft::EuhatDecoderSoft()
{

}

EuhatDecoderSoft::~EuhatDecoderSoft()
{

}

int EuhatDecoderSoft::init(EuhatDecoderCallback callback, void *context)
{
    callback_ = callback;
    context_ = context;

//	avcodec_register_all();
	codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (NULL == codec_) {
		return -1 ;
	}

	codecContext_= avcodec_alloc_context3(codec_);
	if (NULL == codecContext_) {
	    codec_ = NULL;
		return -2;
	}

/*	if (codec_->capabilities & CODEC_CAP_TRUNCATED) {
		codecContext_->flags |= CODEC_FLAG_TRUNCATED;
	}
*/
	if (avcodec_open2(codecContext_, codec_, NULL) < 0)
	{
		avcodec_free_context(&codecContext_);
		codecContext_ = NULL;
		codec_ = NULL;
		return -3;
	}

    return 0;
}

int EuhatDecoderSoft::fini()
{
	avcodec_close(codecContext_);
	avcodec_free_context(&codecContext_);
    return 1;
}

int EuhatDecoderSoft::updateSps(const char *sps)
{
    return 1;
}

int EuhatDecoderSoft::updatePps(const char *pps)
{
    return 1;
}

int EuhatDecoderSoft::updateWH(int width, int height)
{
    return 1;
}

int EuhatDecoderSoft::updateSurface(void *surface)
{
    return 1;
}

int EuhatDecoderSoft::decode(char *frame, int frameLen)
{
    int ret = 0;
	AVPacket packet;
	av_init_packet(&packet);
    packet.data = (unsigned char *)frame;
    packet.size = frameLen;

    AVFrame *frameYUV = av_frame_alloc();

    avcodec_send_packet(codecContext_, &packet);

    do {
        ret = avcodec_receive_frame(codecContext_, frameYUV);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            DBG(("soft decoding error.\n"));
            break;
        }

        int sizeYUV = frameYUV->width * frameYUV->height * 3 / 2;
        char *yuv = (char *)outputMemPool_->alloc(sizeYUV + 4 * 3);
        char *p = yuv + 4 * 3;
        for (int i = 0; i < frameYUV->height; i++) {
            memcpy(p, frameYUV->data[0] + frameYUV->linesize[0] * i, frameYUV->width);
            p += frameYUV->width;
        }
        for (int j = 0; j < frameYUV->height / 2; j++) {
            memcpy(p, frameYUV->data[1] + frameYUV->linesize[1] * j, frameYUV->width / 2);
            p += frameYUV->width / 2;
        }
        for (int k = 0; k < frameYUV->height / 2; k++) {
            memcpy(p, frameYUV->data[2] + frameYUV->linesize[2] * k, frameYUV->width / 2);
            p += frameYUV->width / 2;
        }
        *(int *)yuv = frameYUV->width;
        *(int *)(yuv + 4) = frameYUV->height;
        *(int *)(yuv + 8) = sizeYUV;
        callback_(yuv, context_);
    } while (0);

    av_packet_unref(&packet);
   	av_frame_free(&frameYUV);

    return ret;
}