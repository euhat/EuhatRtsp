#pragma once

#include "DecodeOp.h"

class AMediaCodec;

class EuhatDecoderHard : public EuhatDecoderBase
{
    int updateSpsAndPps();

    int spsChanged_;
    int ppsChanged_;
    char *sps_;
    char *pps_;
    int width_;
    int height_;
    int oriHeight_;
    int colorFormat_;
    int stride_;
    int sliceHeight_;
    int cropTop_;
    int cropBottom_;
    int cropLeft_;
    int cropRight_;
    int isCodecInited_;
    void *surface_;
    int surfaceChanged_;

public:
    EuhatDecoderHard();
    virtual ~EuhatDecoderHard();

    virtual int init(EuhatDecoderCallback callback, void *context);
    virtual int fini();
    virtual int updateSps(const char *sps);
    virtual int updatePps(const char *pps);
    virtual int updateWH(int width, int height);
    virtual int updateSurface(void *surface);
    virtual int decode(char *frame, int frameLen);

    static int canWork();

    AMediaCodec *mediaCodec_;
};