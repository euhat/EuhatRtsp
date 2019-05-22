#pragma once

#include "DecodeOp.h"

extern "C"
{
#include "libavcodec\avcodec.h"
//#include "libswscale\swscale.h"
};

class EuhatDecoderSoft : public EuhatDecoderBase
{
    AVCodec *codec_;
    AVCodecContext *codecContext_;

public:
    EuhatDecoderSoft();
    virtual ~EuhatDecoderSoft();

    virtual int init(EuhatDecoderCallback callback, void *context);
    virtual int fini();
    virtual int updateSps(const char *sps);
    virtual int updatePps(const char *pps);
    virtual int updateWH(int width, int height);
    virtual int updateSurface(void *surface);
    virtual int decode(char *frame, int frameLen);

};